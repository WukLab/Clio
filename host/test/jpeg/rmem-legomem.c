#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/page.h>
#include <uapi/err.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "../../core.h"
#include "rmem.h"

#define NR_THREADS	(1024)

unsigned long __remote global_addr[NR_THREADS][2];
   
struct remote_mem_legomem {
    struct remote_mem rmem;
    struct legomem_context *ctx;
    struct session_net *sess[NR_THREADS];
    void * wbuf[NR_THREADS];

    unsigned long __remote addr[NR_THREADS][2];
};

struct remote_mem * rinit(int access, size_t size, void *args)
{
    struct remote_mem_legomem * rmem;
    struct legomem_context *p __maybe_unused;
    rmem = (struct remote_mem_legomem *)calloc(1, sizeof(struct remote_mem_legomem));
    rmem->rmem.access = access;

#if 1
	rmem->ctx = legomem_open_context();
#else
	p = malloc(sizeof(*p));
	init_legomem_context(p);
	p->pid = 1;
	add_legomem_context(p);
	rmem->ctx = p;
#endif

	dump_legomem_contexts();

    return (struct remote_mem *)rmem;
}

int rclose(struct remote_mem * _rmem) {
    struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;
	legomem_close_context(rmem->ctx);
    free(rmem);
    return 0;
}

void * rcreatebuf (struct remote_mem * _rmem, size_t size, int thread_id)
{
    struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;
    void * buf;

    // only the send size (wbuf) will call this
    buf = malloc(size);
    net_reg_send_buf(rmem->sess[thread_id], buf, size);
    rmem->wbuf[thread_id] = buf;

    return buf;
}

int rread (struct remote_mem * _rmem, void *rbuf, uint64_t addr, size_t size, int buffer_index, int thread_id)
{
    struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;

    unsigned long raddr;

    raddr = global_addr[thread_id][buffer_index] + addr;

    legomem_read_with_session(rmem->ctx, rmem->sess[thread_id], rmem->wbuf[thread_id], rbuf, raddr, size);

    return 0;
}

int rwrite (struct remote_mem * _rmem, void *buf, uint64_t addr, size_t size, int buffer_index, int thread_id)
{
    struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;
    unsigned long raddr;

    raddr = global_addr[thread_id][buffer_index] + addr;

    return legomem_write_sync(rmem->ctx, buf, raddr, size);
}

int ralloc (struct remote_mem * _rmem, void *buf, uint64_t addr, size_t size, int thread_id)
{
	struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;

	rmem->addr[thread_id][addr] = legomem_alloc(rmem->ctx, size, LEGOMEM_VM_FLAGS_POPULATE);
	if (rmem->addr[thread_id][addr] <= 0) {
		printf("legomem alloc failed\n");
		exit(0);
	}
	printf("ralloc: index: %ld, addr allocated: %#lx thread_id: %d\n",
		addr, rmem->addr[thread_id][addr], thread_id);

	global_addr[thread_id][addr] = rmem->addr[thread_id][addr];

	rmem->sess[thread_id] = find_or_alloc_vregion_session(rmem->ctx, rmem->addr[thread_id][addr]);

    return addr;
}
