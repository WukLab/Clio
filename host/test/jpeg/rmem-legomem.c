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

struct remote_mem_legomem {
    struct remote_mem rmem;
    struct legomem_context *ctx;
    struct session_net *sess;
    void * wbuf;

    unsigned long __remote addr[16];
};

struct remote_mem * rinit(int access, size_t size, void *args)
{
    struct remote_mem_legomem * rmem;
    struct legomem_context *p;
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

void * rcreatebuf (struct remote_mem * _rmem, size_t size)
{
    struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;
    void * buf;

    // only the send size (wbuf) will call this
    buf = malloc(size);
    net_reg_send_buf(rmem->sess, buf, size);
    rmem->wbuf = buf;

    return buf;
}

int rread (struct remote_mem * _rmem, void *buf, uint64_t addr, size_t size)
{
    struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;

    return legomem_read_with_session(rmem->ctx, rmem->sess, rmem->wbuf, buf, rmem->addr[0]+addr, size);
}

int rwrite (struct remote_mem * _rmem, void *buf, uint64_t addr, size_t size)
{
    struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;

    return legomem_write_sync(rmem->ctx, buf, rmem->addr[1] + addr, size);
}

int ralloc (struct remote_mem * _rmem, void *buf, uint64_t addr, size_t size)
{
	struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;

	rmem->addr[addr] = legomem_alloc(rmem->ctx, size, 0);
	if (rmem->addr[addr] < 0) {
		printf("legomem alloc failed\n");
		exit(0);
	}
	rmem->sess = find_or_alloc_vregion_session(rmem->ctx, addr);

    return addr;
}
