/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _LEGOMEM_UAPI_SCHED_H_
#define _LEGOMEM_UAPI_SCHED_H_

#include <uapi/list.h>
#include <uapi/vregion.h>

#define BOARD_NAME_LEN		(32)
#define PROC_NAME_LEN		(32)

struct board_info {
	char			name[BOARD_NAME_LEN];
	unsigned int		board_ip;

	struct list_head	list;

	unsigned long		mem_total;
	unsigned long		mem_avail;
};

struct proc_info {
	char			proc_name[PROC_NAME_LEN];
	unsigned long		flags;

	struct list_head	list;

	char			host_name[PROC_NAME_LEN];
	unsigned int		host_ip;
	struct vregion_info	vregion[NR_VREGIONS];
};

#endif /* _LEGOMEM_UAPI_SCHED_H_ */
