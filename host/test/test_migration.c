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

int test_legomem_migration(void)
{
	struct legomem_context *ctx;
	struct timespec s, e;
	struct board_info *dst_bi;
	unsigned long __remote addr;
	unsigned int ip, port;
	unsigned int ip1, ip2, ip3, ip4;
	double lat_ns;
	unsigned int alloc_size;

	char board_ip_port_str[] = "127.0.0.1:9998";
	sscanf(board_ip_port_str, "%u.%u.%u.%u:%d", &ip1, &ip2, &ip3, &ip4, &port);
	ip = ip1 << 24 | ip2 << 16 | ip3 << 8 | ip4;

	dprintf_INFO("Running migration test. %d\n", 0);

	ctx = legomem_open_context();

	alloc_size = 128;
	addr = legomem_alloc(ctx, alloc_size, 0);
	if (!addr) {
		dprintf_ERROR("alloc failed %d\n", 0);
		goto close;
	}

	dst_bi = find_board(ip, port);
	if (!dst_bi) {
		dprintf_ERROR("Couldn't find board: %s\n", board_ip_port_str);
		goto close;
	}

	clock_gettime(CLOCK_MONOTONIC, &s);
	legomem_migration(ctx, dst_bi, addr, 128);
	clock_gettime(CLOCK_MONOTONIC, &e);

	lat_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
		 (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

	dprintf_INFO("migrated [%#lx %#lx] vregion %d, latency_ns: %lf\n",
		addr, addr + alloc_size, va_to_vregion_index(addr), lat_ns);

close:
	legomem_close_context(ctx);
	return 0;
}
