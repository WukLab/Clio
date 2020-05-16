/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Testing
 * - legomem_migration
 *
 * Scripts
 * - scripts/test_migration.sh
 *
 * You need at least a monitor, a host running test case,
 * and either a real board or board emulator instance.
 *
 * legomem_migration is not an simple API, its perf varies depends on scenarios.
 * So, be careful.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../core.h"

#define NR_TESTS	1

int test_legomem_migration(char *_unused)
{
	struct legomem_context *ctx;
	struct timespec s, e;
	struct board_info *dst_bi;
	int i;
	unsigned long __remote addr[NR_TESTS];
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;
	double lat_ns;
	unsigned int alloc_size;

	char board_ip_port_str[] = "192.168.1.31:1234";
	sscanf(board_ip_port_str, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

	dst_bi = find_board(ip, port);
	if (!dst_bi) {
		dprintf_ERROR("Couldn't find board: %s\n", board_ip_port_str);
		return 0;
	}

	dprintf_INFO("Running migration test. %d\n", 0);

	ctx = legomem_open_context();

	alloc_size = VREGION_SIZE - 1;

	for (i = 0; i < NR_TESTS; i++) {
		addr[i] = legomem_alloc(ctx, alloc_size, 0);
		if (!addr[i]) {
			dprintf_ERROR("alloc failed %d\n", 0);
			goto close;
		}
#if 1
		dprintf_INFO(" alloc i=%3d addr=%#18lx vregion_idx=%u\n",
			i, addr[i], va_to_vregion_index(addr[i]));
#endif
	}

	clock_gettime(CLOCK_MONOTONIC, &s);
	/* 
	 * leave the first one open, we want to reuse its session
	 * if we happen to use only one board.
	 */
	for (i = 0; i < NR_TESTS; i++) {
#if 1
		dprintf_INFO(" migrate i=%3d addr=%#18lx vregion_idx=%u\n",
			i, addr[i], va_to_vregion_index(addr[i]));
#endif

		legomem_migration(ctx, dst_bi, addr[i], 128);
	}
	clock_gettime(CLOCK_MONOTONIC, &e);

	lat_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
		 (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
	lat_ns /= (NR_TESTS);

	dprintf_INFO("#nr_migration: %d, #avg_latency_ns: %lf\n",
		NR_TESTS, lat_ns);

close:
	legomem_close_context(ctx);
	return 0;
}
