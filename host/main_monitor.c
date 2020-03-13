/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <uapi/err.h>
#include <uapi/list.h>
#include <uapi/vregion.h>
#include <uapi/hashtable.h>
#include <uapi/bitops.h>
#include <uapi/sched.h>
#include <uapi/thpool.h>
#include <uapi/opcode.h>
#include <uapi/net_header.h>

#include "core.h"

#define NR_THPOOL_WORKERS	(1)
#define NR_THPOOL_BUFFER	(32)

/*
 * Each thpool worker is described by struct thpool_worker,
 * and it is a standalone thread, running the generic handler only.
 * We can have one or multiple workers depends on config.
 *
 * The flow is:
 * a) Dispatcher allocate thpool buffer
 * b) Dispatcher receive network packet from FPGA
 * c) Dispacther find a worker, and delegate the request
 * d) The worker handles the req, send reply to FPGA, and free the thpool buffer.
 *
 * The thpool buffer is using a simple ring-based design.
 */
static int TW_HEAD;
static int TB_HEAD;
static struct thpool_worker *thpool_worker_map;
static struct thpool_buffer *thpool_buffer_map;

/*
 * Select a worker to handle the next request
 * in a round-robin fashion.
 */
static __always_inline struct thpool_worker *
select_thpool_worker_rr(void)
{
	struct thpool_worker *tw;
	int idx;

	idx = TW_HEAD % NR_THPOOL_WORKERS;
	tw = thpool_worker_map + idx;
	TW_HEAD++;
	return tw;
}

static __always_inline struct thpool_buffer *
alloc_thpool_buffer(void)
{
	struct thpool_buffer *tb;
	int idx;

	idx = TB_HEAD % NR_THPOOL_BUFFER;
	tb = thpool_buffer_map + idx;
	TB_HEAD++;

	/*
	 * If this happens during runtime, it means:
	 * - ring buffer is not large enough
	 * - some previous handlers are too slow
	 */
	while (unlikely(ThpoolBufferUsed(tb))) {
		;
	}

	SetThpoolBufferUsed(tb);
	barrier();
	return tb;
}

static __always_inline void
free_thpool_buffer(struct thpool_buffer *tb)
{
	tb->flags = 0;
	barrier();
}

static int init_thpool_buffer(void)
{
	int i;
	size_t buf_sz;
	struct thpool_buffer *map;

	buf_sz = sizeof(struct thpool_buffer) * NR_THPOOL_BUFFER;
	map = malloc(buf_sz);
	if (!map)
		return -ENOMEM;

	for (i = 0; i < NR_THPOOL_BUFFER; i++) {
		struct thpool_buffer *tb;

		tb = map + i;
		tb->flags = 0;
		tb->rx_size = 0;
		tb->tx_size = 0;
		memset(&tb->tx, 0, THPOOL_BUFFER_SIZE);
		memset(&tb->rx, 0, THPOOL_BUFFER_SIZE);
	}

	thpool_buffer_map = map;
	TB_HEAD = 0;
	return 0;
}

static int init_thpool(void)
{
	int i;
	size_t buf_sz;

	buf_sz = sizeof(struct thpool_worker) * NR_THPOOL_WORKERS;
	thpool_worker_map = malloc(buf_sz);
	if (!thpool_worker_map)
		return -ENOMEM;

	for (i = 0; i < NR_THPOOL_WORKERS; i++) {
		struct thpool_worker *tw;

		tw = thpool_worker_map + i;
		tw->cpu = 0;
		tw->nr_queued = 0;
		pthread_spin_init(&tw->lock, PTHREAD_PROCESS_PRIVATE);
	}
	TW_HEAD = 0;
	return 0;
}

static DECLARE_BITMAP(pid_map, NR_MAX_PID);
static pthread_spinlock_t(pid_lock);

#define PID_ARRAY_HASH_BITS	(10)
static DEFINE_HASHTABLE(proc_hash_array, PID_ARRAY_HASH_BITS);
static pthread_spinlock_t proc_lock;

static inline int getKey(unsigned int pid, unsigned int node)
{
        return node * NR_MAX_PROCS_PER_NODE + pid;
}

static void init_vregion(struct vregion_info *v)
{
	v->flags = VM_UNMAPPED_AREA_TOPDOWN;
	v->mmap = NULL;
	v->mm_rb = RB_ROOT;
	v->nr_vmas = 0;
	v->highest_vm_end = 0;
	pthread_spin_init(&v->lock, PTHREAD_PROCESS_PRIVATE);
}

static void init_proc_info(struct proc_info *pi)
{
	int j;
	struct vregion_info *v;

	pi->flags = 0;

	INIT_HLIST_NODE(&pi->link);
	pi->pid = 0;
	pi->node = 0;

	pthread_spin_init(&pi->lock, PTHREAD_PROCESS_PRIVATE);
	atomic_init(&pi->refcount, 1);

	pi->nr_vmas = 0;
	for (j = 0; j < NR_VREGIONS; j++) {
		v = pi->vregion + j;
		init_vregion(v);
	}
}

int alloc_pid(void)
{
	int bit;

	/*
	 * note that find_next_zero_bit is not atomic,
	 * and we need to have lock here. Even though
	 * its possible to use test_and_set_bit without lock,
	 * use lock will do harm here.
	 */
	pthread_spin_lock(&pid_lock);
	bit = find_next_zero_bit(pid_map, NR_MAX_PID, 1);
	if (bit >= NR_MAX_PID) {
		bit = -1;
		goto unlock;
	}
	__set_bit(bit, pid_map);

unlock:
	pthread_spin_unlock(&pid_lock);
	return bit;
}

void free_pid(unsigned int pid)
{
	BUG_ON(pid >= NR_MAX_PID);

	pthread_spin_lock(&pid_lock);
	if (!test_and_clear_bit(pid, pid_map))
		BUG();
	pthread_spin_unlock(&pid_lock);
}

static struct proc_info *
alloc_proc(unsigned int pid, char *proc_name, unsigned int host_ip)
{
	struct proc_info *new;
	struct proc_info *old;
	unsigned int key;

	new = malloc(sizeof(*new));
	if (!new)
		return NULL;

	init_proc_info(new);
	new->host_ip = host_ip;

	if (proc_name)
		strncpy(new->proc_name, proc_name, PROC_NAME_LEN);

	key = getKey(pid, host_ip);

	/* Insert into the hashtable */
	pthread_spin_lock(&proc_lock);
	hash_for_each_possible(proc_hash_array, old, link, key) {
		if (unlikely(old->pid == pid && old->host_ip == host_ip)) {
			pthread_spin_unlock(&proc_lock);
			free(new);
			printf("alloc_proc: pid %u exists\n", pid);
			return NULL;
		}
	}
	hash_add(proc_hash_array, &new->link, key);
	pthread_spin_unlock(&proc_lock);

	printf("alloc_proc: new proc pid %u ip %x\n", pid, host_ip);

	return new;
}

/*
 * Find the pi structure by given pid and ip.
 * The refcount is incremented by 1 if found.
 * The caller must call put_proc() afterwards.
 */
struct proc_info *get_proc_by_pid(unsigned int pid, unsigned int host_ip)
{
	struct proc_info *pi;
	unsigned int key;

	key = getKey(pid, host_ip);

	pthread_spin_lock(&proc_lock);
	hash_for_each_possible(proc_hash_array, pi, link, key) {
		if (likely(pi->pid == pid && pi->host_ip == host_ip)) {
			get_proc_info(pi);
			pthread_spin_unlock(&proc_lock);
			return pi;
		}
	}
	pthread_spin_unlock(&proc_lock);
	return NULL;
}

/*
 * Free the given pi and remove it from the hashtable.
 * The refcount must be 0 upon invocation.
 */
void free_proc(struct proc_info *pi)
{
	unsigned int ip, pid, key;
	struct proc_info *tsk;

	if (!pi)
		return;

	if (atomic_load(&pi->refcount)) {
		printf("BUG: refcount is not zero. Use put_proc()\n");
		return;
	}

	ip = pi->host_ip;
	pid = pi->pid;
	key = getKey(pid, ip);

	/* Walk through all the possible buckets, check ip and pid */
	pthread_spin_lock(&proc_lock);
	hash_for_each_possible(proc_hash_array, tsk, link, key) {
		if (likely(tsk->host_ip == ip && tsk->pid == pid)) {
			hash_del(&tsk->link);
			pthread_spin_unlock(&proc_lock);
			free(tsk);
			return;
		}
	}
	pthread_spin_unlock(&proc_lock);
	printf("WARN: Fail to find tsk (ip %u pid %d)\n", ip, pid);
}

/*
 * This is the handler for host side __legomem_open_context.
 */
static void handle_create_proc(struct thpool_buffer *tb)
{
	struct legomem_create_context_req *req;
	struct legomem_create_context_resp *resp;
	struct proc_info *proc;
	unsigned int pid, host_ip;
	char *proc_name;

	resp = (struct legomem_create_context_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	req = (struct legomem_create_context_req *)tb->rx;
	proc_name = req->op.proc_name;
	host_ip = 0;

	pid = alloc_pid();
	if (unlikely(pid < 0)) {
		resp->op.ret = -ENOMEM;
		return;
	}

	proc = alloc_proc(pid, proc_name, host_ip);
	if (unlikely(!proc)) {
		free_pid(pid);
		resp->op.ret = -ENOMEM;
		return;
	}

	/* Succeed, return PID to user */
	resp->op.ret = 0;
	resp->op.pid = pid;
}

static void handle_free_proc(struct thpool_buffer *tb)
{
	struct legomem_close_context_req *req;
	struct legomem_close_context_resp *resp;
	struct lego_header *lego_header;
	struct proc_info *pi;
	unsigned int pid, host_ip;

	resp = (struct legomem_close_context_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*req));

	req = (struct legomem_close_context_req *)tb->rx;
	lego_header = to_lego_header(req);
	pid = lego_header->pid;
	host_ip = 0;

	pi = get_proc_by_pid(pid, host_ip);
	if (!pi) {
		resp->ret = -ESRCH;
		return;
	}

	/* We grabbed one ref above, thus put twice */
	resp->ret = 0;
	put_proc_info(pi);
	put_proc_info(pi);
}

static void handle_alloc(struct thpool_buffer *tb)
{
	struct legomem_alloc_free_req *req;
	struct legomem_alloc_free_resp *resp;
	unsigned int pid, host_ip;
	struct proc_info *pi;
	unsigned long len;

	resp = (struct legomem_alloc_free_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	req = (struct legomem_alloc_free_req *)tb->rx;
	pid = 0;
	host_ip = 0;
	pi = get_proc_by_pid(pid, host_ip);
	if (!pi) {
		resp->op.ret = -ESRCH;
		return;
	}

	/* Get the allocation size */
	len = req->op.len;
	if (!len) {
		resp->op.ret = -EINVAL;
		goto out;
	}

	/*
	 * TODO
	 * Go through the list of vregions and fine one
	 * that could satisfy the requirement
	 * need to fill the board and index.
	 */
	resp->op.ret = 0;
	resp->op.board_ip = 0;
	resp->op.vregion_index = 0;

out:
	put_proc_info(pi);
}

static void handle_free(struct thpool_buffer *tb)
{

}

/* Port current host net */
static inline size_t _net_send(void *buf, size_t buf_size)
{
	return -ENOSYS;
}

static inline size_t _net_receive(void *buf, size_t buf_size)
{
	return -ENOSYS;
}

static void worker_handle_request(struct thpool_worker *tw,
				  struct thpool_buffer *tb)
{
	struct lego_header *lego_hdr;
	uint16_t opcode;
	void *rx_buf;

	rx_buf = tb->rx;

	lego_hdr = (struct lego_header *)(rx_buf + LEGO_HEADER_OFFSET);
	opcode = lego_hdr->opcode;

	switch (opcode) {
	case OP_REQ_ALLOC:
		handle_alloc(tb);
		break;
	case OP_REQ_FREE:
		handle_free(tb);
		break;

	/* Proc */
	case OP_CREATE_PROC:
		handle_create_proc(tb);
		break;
	case OP_FREE_PROC:
		handle_free_proc(tb);
		break;
	default:
		break;
	};

	if (likely(!ThpoolBufferNoreply(tb)))
		_net_send(tb->tx, tb->tx_size);
	free_thpool_buffer(tb);
}

static void dispatcher(void)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	size_t ret;

	while (1) {
		tb = alloc_thpool_buffer();
		tw = select_thpool_worker_rr();

		ret = _net_receive(tb->rx, tb->rx_size);
		if (ret < 0) {
			printf("axi dma fpga to soc failed\n");
			return;
		}

		/*
		 * Inline handling for now
		 * We will need to pass down the request once go SMP
		 */
		worker_handle_request(tw, tb);
	}
}

struct legomem_context *mgmt_context;
struct board_info *mgmt_dummy_board;
struct session_net *mgmt_session;

int init_management_session(void)
{
	struct endpoint_info dummy_ei;

	mgmt_dummy_board = add_board("local_mgmt", 0, &dummy_ei, &dummy_ei);
	if (!mgmt_dummy_board)
		return -ENOMEM;

	mgmt_context = legomem_open_context_mgmt();
	if (!mgmt_context)
		return -ENOMEM;

	mgmt_session = legomem_open_session_mgmt(mgmt_context, mgmt_dummy_board);
	if (!mgmt_session)
		return -ENOMEM;
	return 0;
}

int main(int argc, char **argv)
{
	init_thpool();
	init_thpool_buffer();

	/* Same as host side init */
	init_board_subsys();
	init_net_session_subsys();
	init_management_session();

	pthread_spin_init(&proc_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&pid_lock, PTHREAD_PROCESS_PRIVATE);
}
