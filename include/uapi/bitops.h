/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#ifndef _LEGOMEM_UAPI_BITOPS_H_
#define _LEGOMEM_UAPI_BITOPS_H_

#include <uapi/compiler.h>

#if BITS_PER_LONG == 32
# define _BITOPS_LONG_SHIFT 5
#elif BITS_PER_LONG == 64
# define _BITOPS_LONG_SHIFT 6
#else
# error "Unexpected BITS_PER_LONG"
#endif

#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_LONG)

#define DECLARE_BITMAP(name, bits)		\
	unsigned long name[BITS_TO_LONGS(bits)]

#ifdef CONFIG_ARCH_X86
# include <uapi/bitops_x86.h>
#else
# include <uapi/bitops_asm_generic.h>
#endif

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))
#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))

/*
 * This is a common helper function for find_next_bit and
 * find_next_zero_bit.  The difference is the "invert" argument, which
 * is XORed with each fetched word before searching it for one bits.
 */
static inline unsigned long
_find_next_bit(const unsigned long *addr, unsigned long nbits,
	       unsigned long start, unsigned long invert)
{
	unsigned long tmp;

	if (!nbits || start >= nbits)
		return nbits;

	tmp = addr[start / BITS_PER_LONG] ^ invert;

	/* Handle 1st word. */
	tmp &= BITMAP_FIRST_WORD_MASK(start);
	start = round_down(start, BITS_PER_LONG);

	while (!tmp) {
		start += BITS_PER_LONG;
		if (start >= nbits)
			return nbits;

		tmp = addr[start / BITS_PER_LONG] ^ invert;
	}

	return min(start + __ffs(tmp), nbits);
}

/*
 * Find the next set bit in a memory region.
 */
static inline unsigned long
find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset)
{
	return _find_next_bit(addr, size, offset, 0UL);
}

static inline unsigned long
find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset)
{
	return _find_next_bit(addr, size, offset, ~0UL);
}

/*
 * Find the first set bit in a memory region.
 */
static inline unsigned long
find_first_bit(const unsigned long *addr, unsigned long size)
{
	unsigned long idx;

	for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
		if (addr[idx])
			return min(idx * BITS_PER_LONG + __ffs(addr[idx]), size);
	}

	return size;
}

/*
 * Find the first cleared bit in a memory region.
 */
static inline unsigned long
find_first_zero_bit(const unsigned long *addr, unsigned long size)
{
	unsigned long idx;

	for (idx = 0; idx * BITS_PER_LONG < size; idx++) {
		if (addr[idx] != ~0UL)
			return min(idx * BITS_PER_LONG + ffz(addr[idx]), size);
	}

	return size;
}

static inline unsigned long
find_last_bit(const unsigned long *addr, unsigned long size)
{
	if (size) {
		unsigned long val = BITMAP_LAST_WORD_MASK(size);
		unsigned long idx = (size-1) / BITS_PER_LONG;

		do {
			val &= addr[idx];
			if (val)
				return idx * BITS_PER_LONG + __fls(val);

			val = ~0ul;
		} while (idx--);
	}
	return size;
}

#define for_each_set_bit(bit, addr, size)			\
	for ((bit) = find_first_bit((addr), (size));		\
	     (bit) < (size);					\
	     (bit) = find_next_bit((addr), (size), (bit) + 1))

#define for_each_clear_bit(bit, addr, size)			\
	for ((bit) = find_first_zero_bit((addr), (size));	\
	     (bit) < (size);					\
	     (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

#endif /* _LEGOMEM_UAPI_BITOPS_H_ */
