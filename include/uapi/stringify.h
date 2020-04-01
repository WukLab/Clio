/*
 * Copyright (c) 2020，Wuklab, UCSD.
 */

#ifndef _UAPI_STRINGIFY_H_
#define _UAPI_STRINGIFY_H_

/*
 * Indirect stringification.
 * Doing two levels allows the parameter to be a macro itself.
 */

#define __stringify_1(x...)	#x
#define __stringify(x...)	__stringify_1(x)

#endif /* _UAPI_STRINGIFY_H_ */
