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
#include <sys/mman.h>

#include "../../core.h"
#include "rmem.h"

#define NR_MAX_THREADS			(128)

#define NR_MAX_SESSIONS_PER_THREAD	(32)

unsigned long __remote global_addr[NR_MAX_THREADS][2];
   
struct remote_mem_legomem {
    struct remote_mem rmem;
    struct legomem_context *ctx;
    void * wbuf[NR_MAX_THREADS];

    unsigned long __remote addr[NR_MAX_THREADS][2];

    struct session_net *sess[NR_MAX_THREADS];
    struct session_net sess_array[NR_MAX_THREADS][NR_MAX_SESSIONS_PER_THREAD];
    int ses_idx;
};

struct remote_mem * rinit(int access, size_t size, void *args)
{
    struct remote_mem_legomem * rmem;
    struct legomem_context *p __maybe_unused;

    rmem = mmap(0, sizeof(struct remote_mem_legomem), PROT_READ | PROT_WRITE,
	        MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    if (rmem == MAP_FAILED) {
    	dprintf_ERROR("fail to alloc %d\n",0);
	exit(0);
    }
    memset(rmem, 0, sizeof(*rmem));

    rmem->ses_idx = 0;
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

    // reassign
    int i;
    for (i = 0; i < NR_MAX_SESSIONS_PER_THREAD; i++) {
	    rmem->sess_array[thread_id][i] = *rmem->sess[thread_id];
    }
    rmem->wbuf[thread_id] = buf;

    return buf;
}

#define ROUND_ROBIN_PER_THREAD_SESSION

int rread (struct remote_mem * _rmem, void *rbuf, uint64_t addr, size_t size, int buffer_index, int thread_id)
{
    struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;

    unsigned long raddr;
    struct session_net *ses;

    raddr = global_addr[thread_id][buffer_index] + addr;

#ifdef ROUND_ROBIN_PER_THREAD_SESSION
	int idx;
	idx = (rmem->ses_idx++) % NR_MAX_SESSIONS_PER_THREAD;
	ses = &rmem->sess_array[thread_id][idx];
#else
	ses = rmem->sess[thread_id];
#endif
    legomem_read_with_session(rmem->ctx, ses, rmem->wbuf[thread_id], rbuf, raddr, size);

    return 0;
}

int rwrite (struct remote_mem * _rmem, void *buf, uint64_t addr, size_t size, int buffer_index, int thread_id)
{
    struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;
    unsigned long raddr;
    int ret;
    struct session_net *ses;

    raddr = global_addr[thread_id][buffer_index] + addr;

#ifdef ROUND_ROBIN_PER_THREAD_SESSION
	int idx;
	idx = (rmem->ses_idx++) % NR_MAX_SESSIONS_PER_THREAD;
	ses = &rmem->sess_array[thread_id][idx];
#else
	ses = rmem->sess[thread_id];
#endif
    ret = __legomem_write_with_session(rmem->ctx, ses, buf, raddr, size, LEGOMEM_WRITE_SYNC);
    return ret;
}

int ralloc (struct remote_mem * _rmem, void *buf, uint64_t addr, size_t size, int thread_id)
{
	struct remote_mem_legomem * rmem = (struct remote_mem_legomem *)_rmem;
	struct session_net *ses;

	rmem->addr[thread_id][addr] = legomem_alloc(rmem->ctx, size, LEGOMEM_VM_FLAGS_POPULATE);
	if (rmem->addr[thread_id][addr] <= 0) {
		printf("legomem alloc failed\n");
		exit(0);
	}
	printf("ralloc: index: %ld, addr allocated: %#lx thread_id: %d\n",
		addr, rmem->addr[thread_id][addr], thread_id);

	global_addr[thread_id][addr] = rmem->addr[thread_id][addr];

	int i, base_sesid;
	ses = find_or_alloc_vregion_session(rmem->ctx, rmem->addr[thread_id][addr]);
	base_sesid = get_local_session_id(ses);

	rmem->sess[thread_id] = ses;
	for (i = 0; i < NR_MAX_SESSIONS_PER_THREAD; i++) {
		rmem->sess_array[thread_id][i] = *ses;
		rmem->sess_array[thread_id][i].session_id = base_sesid + i;
	}
	return addr;
}
