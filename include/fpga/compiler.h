/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Some Macro in uapi/compiler.h won't work in Vivado
 * here are some alternative
 */

#ifndef _LEGOMEM_FPGA_COMPILER_H_
#define _LEGOMEM_FPGA_COMPILER_H_

/*
 * alternative of BUILD_BUG_ON
 */
#define ASSERT_CONCAT_(a, b) a##b
#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)
/* These can't be used after statements in c89. */
#ifdef __COUNTER__
#define STATIC_ASSERT(e,m) \
	;enum { ASSERT_CONCAT(__static_assert_, __COUNTER__) = 1/(int)(!!(e)) }
#else
/*
 * This can't be used twice on the same line so ensure if using in headers
 * that the headers are not included twice (by wrapping in #ifndef...#endif)
 * Note it doesn't cause an issue when used on same line of separate modules
 * compiled with gcc -combine -fwhole-program.
 */
#define STATIC_ASSERT(e,m) \
	;enum { ASSERT_CONCAT(__assert_line_, __LINE__) = 1/(int)(!!(e)) }
#endif

#endif /* _LEGOMEM_FPGA_COMPILER_H_ */
