/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Various compiler definitions for easier build, picked from various places,
 * mostly from the linux kernel.
 */

#ifndef _LEGOMEM_UAPI_COMPILER_H_
#define _LEGOMEM_UAPI_COMPILER_H_

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <linux/types.h>
#include <execinfo.h>
#include <stdio.h>
#ifndef  _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sys/syscall.h>

#include <uapi/stringify.h>

/*
 * ZCU106 Cortex-A53 is running at 64bit mode.
 * x86-64 as well.
 */

#define _AC(X,Y)	(X##Y)
#define UL(x)		(_AC(x, UL))
#define ULL(x)		(_AC(x, ULL))

#define BIT(nr)			(UL(1) << (nr))
#define BIT_ULL(nr)		(ULL(1) << (nr))
#define BIT_MASK(nr)		(UL(1) << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BIT_ULL_MASK(nr)	(ULL(1) << ((nr) % BITS_PER_LONG_LONG))
#define BIT_ULL_WORD(nr)	((nr) / BITS_PER_LONG_LONG)
#define BITS_PER_BYTE		(8)
#define BITS_PER_LONG		(64)
#define BITS_PER_LONG_SHIFT	(6)

#ifndef MSEC_PER_SEC
# define MSEC_PER_SEC	1000L
#endif

#ifndef USEC_PER_SEC
# define USEC_PER_SEC	1000000L
#endif

#ifndef NSEC_PER_SEC
# define NSEC_PER_SEC	1000000000L
#endif

static inline void print_backtrace(void)
{
#define BT_BUFFER_SIZE	128
	void *buffer[BT_BUFFER_SIZE];
	size_t size;

	size = backtrace(buffer, BT_BUFFER_SIZE);
	backtrace_symbols_fd(buffer, size, STDOUT_FILENO);
}

#define BUG()					\
	do {					\
		print_backtrace();		\
		assert(0);			\
	} while (0)
#define BUG_ON(cond)				\
	do {					\
		bool __cond = !!(cond);		\
		if (__cond)			\
			BUG();			\
	} while (0)

#define ARRAY_SIZE(x)		(sizeof(x) / sizeof((x)[0]))
#define BUILD_BUG_ON(condition)	((void)sizeof(char[1 - 2*!!(condition)]))

#define __stringify_1(x...)	#x
#define __stringify(x...)	__stringify_1(x)

#define NORETURN		__attribute__((__noreturn__))
#define __packed		__attribute__((__packed__))
#define __used			__attribute__((__used__))
#define __maybe_unused		__attribute__((unused))
#define __always_unused		__attribute__((unused))
#define __must_check		__attribute__((__warn_unused_result__))

#define __section(S)		__attribute__ ((__section__(#S)))

#define __constructor		__attribute__((constructor))

#ifndef __always_inline
# define __always_inline	inline __attribute__((always_inline))
#endif

#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

#ifndef noinline
# define noinline		__attribute__((noinline))
#endif

#define __aligned(x)		__attribute__((aligned(x)))

/*
 * Macro to define stack alignment. 
 * aarch64 requires stack to be aligned to 16 bytes.
 */
#define __stack_aligned__	__attribute__((aligned(16)))

#ifndef offsetof
# define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/*
 * Barrier for Compiler.
 * Prevent GCC from reordering memory accesses.
 */
#define barrier()		asm volatile("": : :"memory")

#ifndef container_of
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#ifndef FIELD_SIZEOF
# define FIELD_SIZEOF(t, f)	(sizeof(((t*)0)->f))
#endif

#define __round_mask(x, y)	((__typeof__(x))((y) - 1))
#define round_up(x, y)		((((x) - 1) | __round_mask(x, y)) + 1)
#define round_down(x, y)	((x) & ~__round_mask(x, y))
#define DIV_ROUND_UP(n,d)	(((n) + (d) - 1) / (d))
#define ALIGN(x, a)		(((x) + (a) - 1) & ~((a) - 1))

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1: __min2; })

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1: __max2; })

#define SWAP(x, y)				\
	do {					\
		typeof(x) ____val = x;		\
		x = y;				\
		y = ____val;			\
	} while (0)

#define is_log2(v)		(((v) & ((v) - 1)) == 0)

/*
 * swap - swap value of @a and @b
 */
#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

/*
 * Use "__ignore_value" to avoid a warning when using a function declared with
 * gcc's warn_unused_result attribute, but for which you really do want to
 * ignore the result.  Traditionally, people have used a "(void)" cast to
 * indicate that a function's return value is deliberately unused.  However,
 * if the function is declared with __attribute__((warn_unused_result)),
 * gcc issues a warning even with the cast.
 *
 * Caution: most of the time, you really should heed gcc's warning, and
 * check the return value.  However, in those exceptional cases in which
 * you're sure you know what you're doing, use this function.
 *
 * Normally casting an expression to void discards its value, but GCC
 * versions 3.4 and newer have __attribute__ ((__warn_unused_result__))
 * which may cause unwanted diagnostics in that case.  Use __typeof__
 * and __extension__ to work around the problem, if the workaround is
 * known to be needed.
 * Written by Jim Meyering, Eric Blake and Pádraig Brady.
 * (See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425 for the details)
 */
#if 3 < __GNUC__ + (4 <= __GNUC_MINOR__)
# define __ignore_value(x)	({ __typeof__ (x) __x = (x); (void) __x; })
#else
# define __ignore_value(x) ((void) (x))
#endif

/*
 * Prevent the compiler from merging or refetching reads or writes. The
 * compiler is also forbidden from reordering successive instances of
 * READ_ONCE, WRITE_ONCE and ACCESS_ONCE (see below), but only when the
 * compiler is aware of some particular ordering.  One way to make the
 * compiler aware of ordering is to put the two invocations of READ_ONCE,
 * WRITE_ONCE or ACCESS_ONCE() in different C statements.
 *
 * In contrast to ACCESS_ONCE these two macros will also work on aggregate
 * data types like structs or unions. If the size of the accessed data
 * type exceeds the word size of the machine (e.g., 32 bits or 64 bits)
 * READ_ONCE() and WRITE_ONCE()  will fall back to memcpy and print a
 * compile-time warning.
 *
 * Their two major use cases are: (1) Mediating communication between
 * process-level code and irq/NMI handlers, all running on the same CPU,
 * and (2) Ensuring that the compiler does not  fold, spindle, or otherwise
 * mutilate accesses that either do not require ordering or that interact
 * with an explicit memory barrier or atomic instruction that provides the
 * required ordering.
 *
 * If possible use READ_ONCE()/WRITE_ONCE() instead.
 */

static __always_inline void __read_once_size(const volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(__u8 *)res = *(volatile __u8 *)p; break;
	case 2: *(__u16 *)res = *(volatile __u16 *)p; break;
	case 4: *(__u32 *)res = *(volatile __u32 *)p; break;
	case 8: *(__u64 *)res = *(volatile __u64 *)p; break;
	default:
		barrier();
		__builtin_memcpy((void *)res, (const void *)p, size);
		barrier();
	}
}

static __always_inline void __write_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(volatile __u8 *)p = *(__u8 *)res; break;
	case 2: *(volatile __u16 *)p = *(__u16 *)res; break;
	case 4: *(volatile __u32 *)p = *(__u32 *)res; break;
	case 8: *(volatile __u64 *)p = *(__u64 *)res; break;
	default:
		barrier();
		__builtin_memcpy((void *)p, (const void *)res, size);
		barrier();
	}
}

#define READ_ONCE(x)						\
({								\
	union {							\
		typeof(x) __val;				\
		char __c[1];					\
	} __u;							\
	__read_once_size(&(x), __u.__c, sizeof(x));		\
	__u.__val;						\
})

#define WRITE_ONCE(x, val)					\
({								\
	union {							\
		typeof(x) __val;				\
		char __c[1];					\
	} __u = { .__val = (val) };				\
	__write_once_size(&(x), __u.__c, sizeof(x));		\
	__u.__val;						\
})

#define ACCESS_ONCE(x)						\
({								\
	__maybe_unused typeof(x) __var = (__force typeof(x)) 0;	\
	*(volatile typeof(x) *)&(x);				\
})

#define L1_CACHE_BYTES	(64)

/*
 * ____cacheline_aligned just make the marked data cache line aligned
 * __cacheline_aligned will also put the data into a specific section
 */
#define ____cacheline_aligned					\
	__attribute__((__aligned__(L1_CACHE_BYTES)))

/*
 * Tell gcc if a function is cold. The compiler will assume any path
 * directly leading to the call is unlikely.
 */
#if GCC_VERSION >= 40300
# define __cold				__attribute__((__cold__))
# define __compiletime_warning(message)	__attribute__((warning(message)))
# define __compiletime_error(message)	__attribute__((error(message)))
#else
# define __cold
# define __compiletime_warning(message)
# define __compiletime_error(message)
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define __read_mostly

/* To mark a remote legomem address */
#define __remote

static inline pid_t gettid(void)
{
	return syscall(SYS_gettid);
}

#endif /* _LEGOMEM_UAPI_COMPILER_H_ */
