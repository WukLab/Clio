/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <getopt.h>
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
#include "endpoint.h"

#define NR_THPOOL_WORKERS	1
#define NR_THPOOL_BUFFER	32

static atomic_int nr_hosts;
static atomic_int nr_boards;

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
static int TW_HEAD = 0;
static int TB_HEAD = 0;
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
 * This is the handler for host side legomem_open_context.
 * We will not contact boards to create the context for now.
 *
 * We will return a unique global PID to caller.
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

	/* Allocate global unique PID and proc structure */
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
	put_proc_info(pi);
	put_proc_info(pi);

	resp->ret = 0;
}

static inline struct vregion_info *
alloc_vregion(struct proc_info *p)
{
	struct vregion_info *v;
	int i;

	pthread_spin_lock(&p->lock);
	for (i = 0; i < NR_VREGIONS; i++) {
		v = p->vregion + i;
		if (!(v->flags & VREGION_INFO_FLAG_ALLOCATED)) {
			v->flags |= VREGION_INFO_FLAG_ALLOCATED;
			pthread_spin_unlock(&p->lock);
			return v;
		}
	}
	pthread_spin_unlock(&p->lock);
	return NULL;
}

static inline void free_vregion(struct proc_info *p,
				struct vregion_info *v)
{
	pthread_spin_lock(&p->lock);
	if (v->flags & VREGION_INFO_FLAG_ALLOCATED) {
		v->flags &=  ~VREGION_INFO_FLAG_ALLOCATED;
	} else {
		BUG();
	}
	pthread_spin_unlock(&p->lock);
}

/*
 * Handler for the case where hosts need to allocate
 * a new vRegion, i.e., ask_monitor_for_new_vregion.
 */
static void handle_alloc(struct thpool_buffer *tb)
{
	struct legomem_alloc_free_req *req;
	struct legomem_alloc_free_resp *resp;
	unsigned int pid, host_ip;
	struct proc_info *pi;
	unsigned long len;
	struct board_info *bi;
	struct vregion_info *vi;

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

	vi = alloc_vregion(pi);
	if (!vi) {
		resp->op.ret = -ENOMEM;
		goto out;
	}

	/*
	 * TODO: Policy plug-in
	 * We are randomly selecting a board now
	 * During practice we should use nr_avail_space, load etc
	 */
	bi = find_board_by_ip(ANY_BOARD);
	if (!bi) {
		free_vregion(pi, vi);

		resp->op.ret = -EPERM;
		printf("%s(): WARN. No board available. Requester: %d %x\n",
			__func__, pid, host_ip);
		goto out;
	}

	/*
	 * All info in place
	 * reply to host
	 */
	resp->op.ret = 0;
	resp->op.board_ip = bi->board_ip;
	resp->op.vregion_idx = vregion_to_index(pi, vi);

out:
	put_proc_info(pi);
}

static void handle_free(struct thpool_buffer *tb)
{

}

static void handle_test(struct thpool_buffer *tb)
{
	struct reply {
		struct legomem_common_headers comm_headers;
		int cnt;
	} *reply, *req;
	static int nr_rx = 0;

	reply = (struct reply *)tb->tx;
	set_tb_tx_size(tb, sizeof(*reply));

	nr_rx++;

	req = (struct reply *)tb->rx;
	printf("%s: cnt: %5d nr_rx: %5d\n", __func__, req->cnt, nr_rx);
}

/*
 * The handler for join_cluster(). Handle requests sent from either hosts or boards.
 * Whenever a node comes online, it will try to contact us, the monitor.
 * We will further broadcast this great news to all out relatives.
 */
static void handle_join_cluster(struct thpool_buffer *tb)
{
	struct legomem_membership_join_cluster_req *req;
	struct legomem_membership_join_cluster_resp *resp;
	struct endpoint_info *ei;
	struct board_info *bi;
	char new_name[BOARD_NAME_LEN];
	char ip_str[INET_ADDRSTRLEN];
	unsigned char mac[6];
	int ret, id, i;

	resp = (struct legomem_membership_join_cluster_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	req = (struct legomem_membership_join_cluster_req *)tb->rx;
	ei = &req->op.ei;

	/*
	 * Cook the node name
	 * this name is globally unique and will be
	 * broadcast to all online nodes.
	 */
	get_ip_str(ei->ip, ip_str);
	if (req->op.type == BOARD_INFO_FLAGS_HOST) {
		id = atomic_fetch_add(&nr_hosts, 1);
		sprintf(new_name, "host%d_%s:%u", id, ip_str, ei->udp_port);
	} else if (req->op.type == BOARD_INFO_FLAGS_BOARD) {
		id = atomic_fetch_add(&nr_boards, 1);
		sprintf(new_name, "board%d_%s:%u", id, ip_str, ei->udp_port);
	} else {
		printf("%s(): invalid type: %lu\n", __func__, req->op.type);
		resp->ret = -EINVAL;
		return;
	}

	/*
	 * We may use a different local MAC address to reach the new host
	 * run our local ARP protocol to get the latest and update if necessary.
	 */
	ret = get_mac_of_remote_ip(ei->ip, ip_str, mac);
	if (ret) {
		dprintf_ERROR("fail to get latest mac for new node. ip: %s\n",
			ip_str);
		resp->ret = -EINVAL;
		return;
	}
	if (memcmp(mac, ei->mac, 6)) {
		printf("%s(): INFO mac updated ", __func__);
		for (i = 0; i < 6; i++) {
			if (i < 5)
				printf("%x:", ei->mac[i]);
			else
				printf("%x -> ", ei->mac[i]);
		}
		for (i = 0; i < 6; i++) {
			if (i < 5)
				printf("%x:", mac[i]);
			else
				printf("%x\n", mac[i]);
		}

		/* Override the original MAC */
		memcpy(ei->mac, mac, 6);
	}

	/* Add the remote party to our list */
	bi = add_board(new_name, req->op.mem_size_bytes,
		       ei, &default_local_ei, false);
	if (!bi) {
		resp->ret = -ENOMEM;
		return;
	}

	if (req->op.type == BOARD_INFO_FLAGS_HOST ||
	    req->op.type == BOARD_INFO_FLAGS_BOARD) {
		bi->flags = req->op.type;
	} else {
		printf("%s(): unknown remote type: %lu",
			__func__, req->op.type);
		remove_board(bi);
		resp->ret = -EINVAL;
		return;
	}

	dprintf_INFO("new node added: %s:%d name: %s type: %s\n",
		ip_str, ei->udp_port, new_name,
		board_info_type_str(bi));
	dump_boards();
	dump_net_sessions();

	/* Notify all online nodes */
	pthread_spin_lock(&board_lock);
	hash_for_each(board_list, i, bi, link) {
		struct legomem_membership_new_node_req new_req;
		struct lego_header *new_lego_header;
		struct session_net *ses;

		/* .. except this one */
		if (bi->board_ip == ei->ip && bi->udp_port == ei->udp_port)
			continue;

		new_lego_header = to_lego_header(&new_req);
		new_lego_header->opcode = OP_REQ_MEMBERSHIP_NEW_NODE;

		/* Cook the request */
		new_req.op.type = req->op.type;
		new_req.op.mem_size_bytes = req->op.mem_size_bytes;
		strncpy(new_req.op.name, new_name, BOARD_NAME_LEN);
		memcpy(&new_req.op.ei, ei, sizeof(*ei));

		/* Send to remote party's mgmt session */
		ses = get_board_mgmt_session(bi);
		net_send(ses, &new_req, sizeof(new_req));
	}
	pthread_spin_unlock(&board_lock);

	/* success */
	resp->ret = 0;
}

static void worker_handle_request(struct thpool_worker *tw,
				  struct thpool_buffer *tb)
{
	struct lego_header *lego_hdr;
	struct gbn_header *gbn_hdr;
	uint16_t opcode;
	struct routing_info *ri;

	lego_hdr = to_lego_header(tb->rx);
	opcode = lego_hdr->opcode;

	switch (opcode) {
	case OP_REQ_TEST:
		handle_test(tb);
		break;
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

	case OP_REQ_MEMBERSHIP_JOIN_CLUSTER:
		handle_join_cluster(tb);
		break;
	default:
		break;
	};

	if (likely(!ThpoolBufferNoreply(tb))) {
		/*
		 * We the mgmt session accepting all traffics
		 * thus we do not really know who is the sender prior
		 * we can only infer that info from the incoming traffic
		 * To reply, we can only, and should, simply swap the routing info
		 */
		ri = (struct routing_info *)tb->rx;
		swap_routing_info(ri);

		/*
		 * Original must be X -> 0
		 * It will become 0 -> X
		 * (X is larger than 0)
		 */
		gbn_hdr = to_gbn_header(tb->rx);
		swap_gbn_session(gbn_hdr);

		net_send_with_route(mgmt_session, tb->tx, tb->tx_size, ri);
	}
	free_thpool_buffer(tb);
}

/*
 * Dispatcher uses local mgmt session to receive and send back msg.
 * Since mgmt session does not have any remote end's information,
 * we must rely on each packet's routing info to send it back.
 */
static void dispatcher(void)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	int ret;

	while (1) {
		tb = alloc_thpool_buffer();
		tw = select_thpool_worker_rr();

		ret = net_receive(mgmt_session, tb->rx, THPOOL_BUFFER_SIZE);
		if (ret <= 0)
			continue;
		tb->rx_size = ret;

		/*
		 * Inline handling for now
		 * We will need to pass down the request once go SMP
		 */
		worker_handle_request(tw, tb);
	}
}

static void print_usage(void)
{
	printf("Usage ./host.o [Options]\n"
	       "\n"
	       "Options:\n"
	       "  --dev=<name>                Specify the local network device\n"
	       "  --port=<port>               Specify the local UDP port we listen to\n"
	       "\n"
	       "Examples:\n"
	       "  ./monitor.o --port 8888 --dev=\"lo\" \n"
	       "  ./monitor.o -p 8888 -d ens4\n");
}

static struct option long_options[] = {
	{ "port",	required_argument,	NULL,	'p'},
	{ "dev",	required_argument,	NULL,	'd'},
	{ 0,		0,			0,	0  }
};

int main(int argc, char **argv)
{
	int ret;
	int c, option_index = 0;
	char ndev[32];
	bool ndev_set = false;
	int port = 0;

	/* Parse arguments */
	while (1) {
		c = getopt_long(argc, argv, "p:d:",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			port = atoi(optarg);
			break;
		case 'd':
			strncpy(ndev, optarg, sizeof(ndev));
			ndev_set = true;
			break;
		default:
			print_usage();
			exit(-1);
		}
	}

	/* Check if required arguments are passed */
	if (!ndev_set) {
		printf("ERROR: Please specify the network device (Use ifconfig to check).\n\n");
		print_usage();
		return 0;
	}

	/*
	 * Init the local endpoint info
	 * - mac, ip, port
	 * Use information based on ndev and port.
	 */
	ret = init_default_local_ei(ndev, port, &default_local_ei);
	if (ret) {
		printf("Fail to init local endpoint. ndev %s port %d\n",
			ndev, port);
		exit(-1);
	}

	ret = init_net(&default_local_ei);
	if (ret) {
		printf("Fail to init network layer.\n");
		exit(-1);
	}

	init_thpool(NR_THPOOL_WORKERS, &thpool_worker_map);
	init_thpool_buffer(NR_THPOOL_BUFFER, &thpool_buffer_map);

	/* Same as host side init */
	init_board_subsys();
	init_context_subsys();
	init_net_session_subsys();

	atomic_init(&nr_hosts, 0);
	atomic_init(&nr_boards, 0);

	pthread_spin_init(&proc_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&pid_lock, PTHREAD_PROCESS_PRIVATE);

	ret = init_local_management_session(false);
	if (ret) {
		printf("Fail to init local mgmt session\n");
		exit(-1);
	}

	dump_net_sessions();
	dispatcher();
}
