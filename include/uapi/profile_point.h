#ifndef _LEGOMEM_UAPI_PROFILE_POINT_H_
#define _LEGOMEM_UAPI_PROFILE_POINT_H_

#include <uapi/compiler.h>
#include <stdatomic.h>

struct profile_point {
	bool		enabled;
	char		pp_name[64];
	atomic_long	nr;
	atomic_long	time_cycles;
} __aligned(64);

#define __profile_point	__section(profile_point)

static inline unsigned long rdtsc(void)
{
	unsigned long var;
	unsigned int hi, lo;

	asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));

	var = ((unsigned long)hi << 32) | lo;
	return var;
}

#ifdef CONFIG_PROFILING_POINTS

#define _PP_TIME(name)	__profilepoint_start_ns_##name
#define _PP_NAME(name)	__profilepoint_##name

/*
 * Define a profile point
 * It is ON by default.
 */
#define DEFINE_PROFILE_POINT(name)							\
	struct profile_point _PP_NAME(name) __profile_point = {				\
		.enabled	=	true,						\
		.pp_name	=	__stringify(name),				\
	};

/*
 * This is just a solution if per-cpu is not used.
 * Stack is per-thread, thus SMP safe.
 */
#define PROFILE_POINT_TIME_STACK(name)							\
	unsigned long _PP_TIME(name) __maybe_unused;

#define PROFILE_START(name)								\
	do {										\
		_PP_TIME(name) = rdtsc();						\
	} while (0)

#define PROFILE_LEAVE(name)								\
	do {										\
		unsigned long __PP_end_time;						\
		unsigned long __PP_diff_time;						\
		__PP_end_time = rdtsc();						\
		__PP_diff_time = __PP_end_time - _PP_TIME(name);			\
		atomic_fetch_add(&(_PP_NAME(name).nr), 1);				\
		atomic_fetch_add(&(_PP_NAME(name).time_cycles), __PP_diff_time);	\
	} while (0)

void print_profile_point(struct profile_point *pp);
void print_profile_points(void);

#else

#define DEFINE_PROFILE_POINT(name)
#define PROFILE_POINT_TIME_STACK(name)
#define PROFILE_START(name)		do { } while (0)
#define PROFILE_LEAVE(name)		do { } while (0)

static inline void print_profile_point(struct profile_point *pp) { }
static inline void print_profile_points(void) { }
#endif /* CONFIG_PROFILING_POINTS */

#endif /* _LEGOMEM_UAPI_PROFILE_POINT_H_ */
