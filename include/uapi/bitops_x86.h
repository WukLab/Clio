/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#ifndef _LEGOMEM_UAPI_ARCH_X86_BITOPS_H_
#define _LEGOMEM_UAPI_ARCH_X86_BITOPS_H_

#include <uapi/compiler.h>

#define LOCK_PREFIX "lock; "

#define BITOP_ADDR(x) "+m" (*(volatile long *) (x))

/*
 * We do the locked ops that don't return the old value as
 * a mask operation on a byte.
 */
#define IS_IMMEDIATE(nr)		(__builtin_constant_p(nr))
#define CONST_MASK_ADDR(nr, addr)	BITOP_ADDR((void *)(addr) + ((nr)>>3))
#define CONST_MASK(nr)			(1 << ((nr) & 7))

/**
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 *
 * Note: there are no guarantees that this function will not be reordered
 * on non x86 architectures, so if you are writing portable code,
 * make sure not to rely on its reordering guarantees.
 *
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static __always_inline void
set_bit(long nr, volatile unsigned long *addr)
{
	if (IS_IMMEDIATE(nr)) {
		asm volatile(LOCK_PREFIX "orb %1,%0"
			: CONST_MASK_ADDR(nr, addr)
			: "iq" ((uint8_t)CONST_MASK(nr))
			: "memory");
	} else {
		asm volatile(LOCK_PREFIX "bts %1,%0"
			: BITOP_ADDR(addr) : "Ir" (nr) : "memory");
	}
}

/**
 * __set_bit - set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * unlike set_bit(), this function is non-atomic and may be reordered.
 * if it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __always_inline void __set_bit(long nr, volatile unsigned long *addr)
{
	asm volatile("bts %1,%0" : BITOP_ADDR(addr) : "ir" (nr) : "memory");
}

/**
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_atomic() and/or smp_mb__after_atomic()
 * in order to ensure changes are visible on other processors.
 */
static __always_inline void
clear_bit(long nr, volatile unsigned long *addr)
{
	if (IS_IMMEDIATE(nr)) {
		asm volatile(LOCK_PREFIX "andb %1,%0"
			: CONST_MASK_ADDR(nr, addr)
			: "iq" ((uint8_t)~CONST_MASK(nr)));
	} else {
		asm volatile(LOCK_PREFIX "btr %1,%0"
			: BITOP_ADDR(addr)
			: "Ir" (nr));
	}
}

static __always_inline void __clear_bit(long nr, volatile unsigned long *addr)
{
	asm volatile("btr %1,%0" : BITOP_ADDR(addr) : "Ir" (nr));
}

/**
 * change_bit - Toggle a bit in memory
 * @nr: Bit to change
 * @addr: Address to start counting from
 *
 * change_bit() is atomic and may not be reordered.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static __always_inline void change_bit(long nr, volatile unsigned long *addr)
{
	if (IS_IMMEDIATE(nr)) {
		asm volatile(LOCK_PREFIX "xorb %1,%0"
			: CONST_MASK_ADDR(nr, addr)
			: "iq" ((uint8_t)CONST_MASK(nr)));
	} else {
		asm volatile(LOCK_PREFIX "btc %1,%0"
			: BITOP_ADDR(addr)
			: "Ir" (nr));
	}
}

/**
 * __change_bit - Toggle a bit in memory
 * @nr: the bit to change
 * @addr: the address to start counting from
 *
 * Unlike change_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __always_inline void __change_bit(long nr, volatile unsigned long *addr)
{
	asm volatile("btc %1,%0" : BITOP_ADDR(addr) : "Ir" (nr));
}

/**
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static __always_inline bool
test_and_set_bit(long nr, volatile unsigned long *addr)
{
	int oldbit;

	asm volatile (
		LOCK_PREFIX "bts %2,%1\n\t"
		"sbb %0,%0"
		: "=r" (oldbit), BITOP_ADDR(addr)
		: "Ir" (nr)
		: "memory"
	);

	return oldbit;
}

/**
 * __test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __always_inline bool
__test_and_set_bit(long nr, volatile unsigned long *addr)
{
	bool oldbit;

	asm("bts %2,%1\n\t"
	    "sbb %0,%0"
	    : "=r" (oldbit), BITOP_ADDR(addr)
	    : "Ir" (nr));
	return oldbit;
}

/**
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static __always_inline bool
test_and_clear_bit(long nr, volatile unsigned long *addr)
{
	int oldbit;

	asm volatile (
		LOCK_PREFIX "btr %2,%1\n\t"
		"sbb %0,%0"
		: "=r" (oldbit), BITOP_ADDR(addr)
		: "Ir" (nr)
		: "memory"
	);

	return oldbit;
}

/**
 * __test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 *
 * Note: the operation is performed atomically with respect to
 * the local CPU, but not other CPUs. Portable code should not
 * rely on this behaviour.
 * KVM relies on this behaviour on x86 for modifying memory that is also
 * accessed from a hypervisor on the same CPU if running in a VM: don't change
 * this without also updating arch/x86/kernel/kvm.c
 */
static __always_inline bool
__test_and_clear_bit(long nr, volatile unsigned long *addr)
{
	int oldbit;

	asm volatile("btr %2,%1\n\t"
		     "sbb %0,%0"
		     : "=r" (oldbit), BITOP_ADDR(addr)
		     : "Ir" (nr));
	return oldbit;
}

/* WARNING: non atomic and it can be reordered! */
static __always_inline int
__test_and_change_bit(int nr, volatile unsigned long *addr)
{
	int oldbit;

	asm volatile("btc %2,%1\n\t"
		     "sbb %0,%0"
		     : "=r" (oldbit), BITOP_ADDR(addr)
		     : "Ir" (nr) : "memory");

	return oldbit;
}

/**
 * test_and_change_bit - Change a bit and return its old value
 * @nr: Bit to change
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static __always_inline int
test_and_change_bit(int nr, volatile unsigned long *addr)
{
	int oldbit;

	asm volatile (
		LOCK_PREFIX "btc %2,%1\n\t"
		"sbb %0,%0"
		: "=r" (oldbit), BITOP_ADDR(addr)
		: "Ir" (nr)
		: "memory"
	);

	return oldbit;
}

static __always_inline bool
constant_test_bit(long nr, const volatile unsigned long *addr)
{
	return ((1UL << (nr & (BITS_PER_LONG-1))) &
		(addr[nr >> _BITOPS_LONG_SHIFT])) != 0;
}

static __always_inline bool
variable_test_bit(int nr, const volatile unsigned long * addr)
{
	int oldbit;

	asm volatile("bt %2,%1\n\t"
		     "sbb %0,%0"
		     : "=r" (oldbit)
		     : "m" (*(unsigned long *)addr), "Ir" (nr));

	return oldbit;
}

/**
 * test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 */
#define test_bit(nr, addr)			\
	(__builtin_constant_p((nr))		\
	 ? constant_test_bit((nr), (addr))	\
	 : variable_test_bit((nr), (addr)))

/**
 * __ffs - find first set bit in word
 * @word: The word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static __always_inline unsigned long __ffs(unsigned long word)
{
	asm("rep; bsf %1,%0"
		: "=r" (word)
		: "rm" (word));
	return word;
}

/**
 * ffz - find first zero bit in word
 * @word: The word to search
 *
 * Undefined if no zero exists, so code should check against ~0UL first.
 */
static __always_inline unsigned long ffz(unsigned long word)
{
	asm("rep; bsf %1,%0"
		: "=r" (word)
		: "r" (~word));
	return word;
}

/*
 * __fls: find last set bit in word
 * @word: The word to search
 *
 * Undefined if no set bit exists, so code should check against 0 first.
 */
static __always_inline unsigned long __fls(unsigned long word)
{
	asm("bsr %1,%0"
	    : "=r" (word)
	    : "rm" (word));
	return word;
}

static __always_inline int fls(unsigned int x)
{
	return x ? sizeof(x) * 8 - __builtin_clz(x) : 0;
}

static __always_inline int fls64(__u64 x)
{
	if (x == 0)
		return 0;
	return __fls(x) + 1;
}

#endif /* _LEGOMEM_UAPI_ARCH_X86_BITOPS_H_ */
