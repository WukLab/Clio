/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _HOST_STAT_H_
#define _HOST_STAT_H_

/*
 * Stat counting is not free. We are using one single global variable
 * with atomic inc. The overhead is not trivial. Thus we are having
 * an option to disable it.
 */
#ifdef CONFIG_DEBUG_STAT
static __always_inline void inc_stat(enum STAT_TYPES item)
{
	asm volatile("lock; incq %0"
		: "=m" (default_local_bi->stat[item])
		: "m" (default_local_bi->stat[item]));
}

static inline void __dump_stats(unsigned long *stat)
{
	int i;

	for (i = 0; i < NR_STAT_TYPES; i++) {
		printf("%40s    %10lu\n",
			stat_type_string(i),
			stat[i]);
	}
}

static inline void dump_stats(void)
{
	__dump_stats(default_local_bi->stat);
}
#else
static inline void inc_stat(enum STAT_TYPES item) { }
static inline void __dump_stats(unsigned long *stat) { }
static inline void dump_stats(void) { }
#endif

#endif /* _HOST_STAT_H_ */
