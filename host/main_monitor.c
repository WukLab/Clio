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
static struct thpool_worker *thpool_worker_map;
static struct thpool_buffer *thpool_buffer_map;

static DECLARE_BITMAP(pid_map, NR_MAX_PID);
static pthread_spinlock_t(pid_lock);

#define PID_ARRAY_HASH_BITS	(10)
static DEFINE_HASHTABLE(proc_hash_array, PID_ARRAY_HASH_BITS);
static pthread_spinlock_t proc_lock;

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
	unsigned int key;

	new = malloc(sizeof(*new));
	if (!new)
		return NULL;
	init_proc_info(new);
	new->pid = pid;
	new->host_ip = host_ip;

	if (proc_name)
		strncpy(new->proc_name, proc_name, PROC_NAME_LEN);

	key = pid;
	pthread_spin_lock(&proc_lock);
	hash_add(proc_hash_array, &new->link, key);
	pthread_spin_unlock(&proc_lock);

	return new;
}

/*
 * The refcount is incremented by 1 if found.
 * The caller must call put_proc() afterwards.
 */
struct proc_info *get_proc_by_pid(unsigned int pid)
{
	struct proc_info *pi;
	unsigned int key;

	key = pid;
	pthread_spin_lock(&proc_lock);
	hash_for_each_possible(proc_hash_array, pi, link, key) {
		dprintf_INFO("pid %d\n", pi->pid);
		if (likely(pi->pid == pid)) {
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
	unsigned int key;
	struct proc_info *_pi;

	if (!pi)
		return;

	if (atomic_load(&pi->refcount)) {
		printf("BUG: refcount is not zero. Use put_proc()\n");
		return;
	}

	key = pi->pid;

	pthread_spin_lock(&proc_lock);
	hash_for_each_possible(proc_hash_array, _pi, link, key) {
		if (likely(_pi == pi)) {
			hash_del(&pi->link);
			pthread_spin_unlock(&proc_lock);
			free(pi);
			return;
		}
	}
	pthread_spin_unlock(&proc_lock);
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
	struct ipv4_hdr *ipv4_hdr;
	struct proc_info *proc;
	unsigned int pid, src_ip;
	char *proc_name;

	resp = (struct legomem_create_context_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	req = (struct legomem_create_context_req *)tb->rx;
	proc_name = req->op.proc_name;

	ipv4_hdr = to_ipv4_header(req);
	src_ip = ntohl(ipv4_hdr->src_ip);

	/* Allocate global unique PID and proc structure */
	pid = alloc_pid();
	if (unlikely(pid < 0)) {
		resp->op.ret = -ENOMEM;
		return;
	}

	proc = alloc_proc(pid, proc_name, src_ip);
	if (unlikely(!proc)) {
		free_pid(pid);
		resp->op.ret = -ENOMEM;
		return;
	}

	/* Succeed, return PID to user */
	resp->op.ret = 0;
	resp->op.pid = pid;

	if (1) {
		char ip_str[INET_ADDRSTRLEN];
		get_ip_str(src_ip, ip_str);
		dprintf_DEBUG("new context pid %u for host %s\n",
			pid, ip_str);
	}
}

static void handle_free_proc(struct thpool_buffer *tb)
{
	struct legomem_close_context_req *req;
	struct legomem_close_context_resp *resp;
	struct lego_header *lego_header;
	struct proc_info *pi;
	unsigned int pid;

	resp = (struct legomem_close_context_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	req = (struct legomem_close_context_req *)tb->rx;
	lego_header = to_lego_header(req);
	pid = lego_header->pid;

	pi = get_proc_by_pid(pid);
	if (!pi) {
		resp->ret = -ESRCH;
		return;
	}

	/* We grabbed one ref above, thus put twice to FREE it */
	put_proc_info(pi);
	put_proc_info(pi);

	resp->ret = 0;

	dprintf_DEBUG("free context pid: %u\n", pid);
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
	unsigned int pid;
	struct proc_info *pi;
	unsigned long len;
	struct board_info *bi;
	struct vregion_info *vi;

	resp = (struct legomem_alloc_free_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	req = (struct legomem_alloc_free_req *)tb->rx;
	pid = 0;
	pi = get_proc_by_pid(pid);
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
	bi = find_board(ANY_BOARD, ANY_BOARD);
	if (!bi) {
		free_vregion(pi, vi);

		resp->op.ret = -EPERM;
		printf("%s(): WARN. No board available. Requester: %d\n",
			__func__, pid);
		goto out;
	}

	/*
	 * All info in place
	 * reply to host
	 */
	resp->op.ret = 0;
	resp->op.board_ip = bi->board_ip;
	resp->op.udp_port = bi->udp_port;
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

static int migration_notify_cancel_recv(struct board_info *dst_bi,
					struct legomem_migration_req *orig_req)
{
	struct legomem_migration_req req;
	struct legomem_migration_resp resp;
	struct lego_header *lego_header;
	struct session_net *ses;
	int ret;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_MIGRATION_M2B_RECV_CANCEL;
	lego_header->pid = to_lego_header(orig_req)->pid;
	memcpy(&req.op, &orig_req->op, sizeof(struct op_migration));

	ses = get_board_mgmt_session(dst_bi);
	ret = net_send_and_receive(ses, &req, sizeof(req), &resp, sizeof(resp));
	if (ret <= 0) {
		dprintf_DEBUG("net error: %d board: %s\n", ret, dst_bi->name);
		return -EIO;
	}
	return resp.op.ret;
}

/*
 * Notify the remote party @dst_bi to prepare for a upcoming migration.
 * We will send the vregion_index and original owner info.
 * Return 0 on success otherwise failures.
 */
static int migration_notify_recv(struct board_info *dst_bi,
				 struct legomem_migration_req *orig_req)
{
	struct legomem_migration_req req;
	struct legomem_migration_resp resp;
	struct lego_header *lego_header;
	struct session_net *ses;
	int ret;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_MIGRATION_M2B_RECV;
	lego_header->pid = to_lego_header(orig_req)->pid;
	memcpy(&req.op, &orig_req->op, sizeof(struct op_migration));

	ses = get_board_mgmt_session(dst_bi);
	ret = net_send_and_receive(ses, &req, sizeof(req), &resp, sizeof(resp));
	if (ret <= 0) {
		dprintf_DEBUG("net error: %d board: %s\n", ret, dst_bi->name);
		return -EIO;
	}
	return resp.op.ret;
}

static int migration_notify_send(struct board_info *src_bi,
				 struct legomem_migration_req *orig_req)
{
	struct legomem_migration_req req;
	struct legomem_migration_resp resp;
	struct lego_header *lego_header;
	struct session_net *ses;
	int ret;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_MIGRATION_M2B_SEND;
	lego_header->pid = to_lego_header(orig_req)->pid;
	memcpy(&req.op, &orig_req->op, sizeof(struct op_migration));

	ses = get_board_mgmt_session(src_bi);
	ret = net_send_and_receive(ses, &req, sizeof(req), &resp, sizeof(resp));
	if (ret <= 0) {
		dprintf_DEBUG("net error: %d board: %s\n", ret, src_bi->name);
		return -EIO;
	}
	return resp.op.ret;
}

/*
 * Handle the case where a host application explicitly asking
 * for data migration. The requester has already choosed the new board.
 *
 * Monitor is the coordinate for vRegion migrations.
 *
 * TODO
 * Be careful about future vRegion sharing case. Especially if there are
 * multiple hosts using the same vRegion. We have to make sure everyone
 * is on the samge page before we proceed any real migration.
 * (See our design slides.)
 */
static void handle_migration_h2m(struct thpool_buffer *tb)
{
	struct legomem_migration_req *req;
	struct legomem_migration_resp *resp;
	struct lego_header *lego_header;
	struct board_info *src_bi, *dst_bi;
	struct proc_info *p;
	struct vregion_info *v;
	unsigned int vregion_index;
	unsigned int pid, ret;

	resp = (struct legomem_migration_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	req = (struct legomem_migration_req *)tb->rx;
	lego_header = to_lego_header(req);

	/* Found those two involved boards */
	src_bi = find_board(req->op.src_board_ip, req->op.src_udp_port);
	if (!src_bi) {
		dprintf_DEBUG("src board %s not found\n", src_bi->name);
		goto error;
	}
	dst_bi = find_board(req->op.dst_board_ip, req->op.dst_udp_port);
	if (!dst_bi) {
		dprintf_DEBUG("dst board %s not found\n", dst_bi->name);
		goto error;
	}

	/* First notify the new board to let it prepare */
	ret = migration_notify_recv(dst_bi, req);
	if (ret) {
		dprintf_DEBUG("dst board %s does not accept migration. Error %d\n",
			dst_bi->name, ret);
		goto error;
	}

	/* Then notify the old board to start migration */
	ret = migration_notify_send(src_bi, req);
	if (ret) {
		/* Tell new board to cancel the party */
		migration_notify_cancel_recv(dst_bi, req);
		dprintf_DEBUG("src board %s cannot start migration. Error %d\n",
			src_bi->name, ret);
		goto error;
	}

	/*
	 * Update monitor local vRegion info
	 * to the latest board
	 */
	pid = lego_header->pid;
	vregion_index = req->op.vregion_index;

	p = get_proc_by_pid(pid);
	v = index_to_vregion(p, vregion_index);
	v->board_ip = dst_bi->board_ip;
	v->udp_port = dst_bi->udp_port;

	resp->op.ret = 0;
	return;

error:
	resp->op.ret = -EFAULT;
}

/*
 * Handle the case where a board explicitly asking for data migration.
 */
static void handle_migration_b2m(struct thpool_buffer *tb)
{

}

/*
 * The handler for join_cluster(). Handle requests sent from either hosts or boards.
 * Whenever a node comes online, it will try to contact us, the monitor.
 * We will further broadcast this great news to all out relatives.
 */
static pthread_spinlock_t join_cluster_lock;
static void handle_join_cluster(struct thpool_buffer *tb)
{
	struct legomem_membership_join_cluster_req *req;
	struct legomem_membership_join_cluster_resp *resp;
	struct endpoint_info *ei;
	struct board_info *new_bi, *bi;
	char new_name[BOARD_NAME_LEN];
	char ip_str[INET_ADDRSTRLEN];
	unsigned char mac[6];
	int ret, id, i;

	resp = (struct legomem_membership_join_cluster_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));

	req = (struct legomem_membership_join_cluster_req *)tb->rx;
	ei = &req->op.ei;

	/*
	 * Step 1:
	 * Sanity check the original request and add the new node locally
	 */

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
		dprintf_ERROR("Unknown node type: %s\n",
			board_info_type_str(req->op.type));
		resp->ret = -EINVAL;
		return;
	}

	/*
	 * We may use a different local MAC address to reach the new host
	 * run our local ARP protocol to get the latest and update if necessary.
	 */
	ret = get_mac_of_remote_ip(ei->ip, ip_str, global_net_dev, mac);
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
	new_bi = add_board(new_name, req->op.mem_size_bytes,
		       ei, &default_local_ei, false);
	if (!new_bi) {
		resp->ret = -ENOMEM;
		return;
	}

	if (req->op.type == BOARD_INFO_FLAGS_HOST ||
	    req->op.type == BOARD_INFO_FLAGS_BOARD) {
		new_bi->flags |= req->op.type;
	} else {
		printf("%s(): unknown remote type: %lu",
			__func__, req->op.type);
		remove_board(new_bi);
		resp->ret = -EINVAL;
		return;
	}

	/* Debugging info */
	dprintf_INFO("new node added: %s:%d name: %s type: %s\n",
		ip_str, ei->udp_port, new_name, board_info_type_str(new_bi->flags));
	dump_boards();
	dump_net_sessions();

	/*
	 * Step 2:
	 * Notify all other online nodes about this new born
	 */
	pthread_spin_lock(&board_lock);
	hash_for_each(board_list, i, bi, link) {
		struct legomem_membership_new_node_req new_req;
		struct legomem_membership_new_node_resp new_resp;
		struct lego_header *new_lego_header;
		struct session_net *ses;

		/* .. except the original sender */
		if (bi == new_bi)
			continue;

		if (special_board_info_type(bi->flags))
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
		net_send_and_receive(ses, &new_req, sizeof(new_req),
					  &new_resp, sizeof(new_resp));
	}
	pthread_spin_unlock(&board_lock);

	/*
	 * Step 3:
	 * Meanwhile, for implementation simplicity,
	 * send a bunch of requests to the original sender
	 * for each existing node in the cluster
	 */
	pthread_spin_lock(&board_lock);
	hash_for_each(board_list, i, bi, link) {
		struct legomem_membership_new_node_req new_req;
		struct legomem_membership_new_node_resp new_resp;
		struct lego_header *new_lego_header;
		struct session_net *ses;

		/* .. except the original sender */
		if (bi == new_bi)
			continue;

		if (special_board_info_type(bi->flags))
			continue;

		new_lego_header = to_lego_header(&new_req);
		new_lego_header->opcode = OP_REQ_MEMBERSHIP_NEW_NODE;

		/* Copy existing board's information into the req */
		new_req.op.type = bi->flags & BOARD_INFO_FLAGS_BITS_MASK;
		new_req.op.mem_size_bytes = bi->mem_total;
		strncpy(new_req.op.name, bi->name, BOARD_NAME_LEN);
		memcpy(&new_req.op.ei, &bi->remote_ei, sizeof(*ei));

		/* Send to original sender's mgmt session */
		ses = get_board_mgmt_session(new_bi);
		net_send_and_receive(ses, &new_req, sizeof(new_req),
					  &new_resp, sizeof(new_resp));
	}
	pthread_spin_unlock(&board_lock);

	/* success */
	resp->ret = 0;
}

static void handle_query_stat(struct thpool_buffer *tb)
{
	struct legomem_query_stat_resp *resp;
	size_t size;
	unsigned long *local_stat;

	/*
	 * calculate the size of resp msg
	 * minus 1 because of the original pointer
	 */
	resp = (struct legomem_query_stat_resp *)tb->tx;
	size = sizeof(*resp) + (NR_STAT_TYPES - 1) * sizeof(unsigned long);
	set_tb_tx_size(tb, size);

	local_stat = default_local_bi->stat;
	memcpy(resp->stat, local_stat, NR_STAT_TYPES * sizeof(unsigned long));
	resp->nr_items = NR_STAT_TYPES;
}

static void handle_pingpong(struct thpool_buffer *tb)
{
	struct legomem_pingpong_resp *resp;

	resp = (struct legomem_pingpong_resp *)tb->tx;
	set_tb_tx_size(tb, sizeof(*resp));
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

	if (0) {
		dprintf_INFO("received opcode: %u (%s)\n",
			opcode, legomem_opcode_str(opcode));
	}

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

	/* Handle process management */
	case OP_CREATE_PROC:
		handle_create_proc(tb);
		break;
	case OP_FREE_PROC:
		handle_free_proc(tb);
		break;

	/* Handle migration requests */
	case OP_REQ_MIGRATION_H2M:
		handle_migration_h2m(tb);
		break;
	case OP_REQ_MIGRATION_B2M:
		handle_migration_b2m(tb);
		break;

	case OP_REQ_MEMBERSHIP_JOIN_CLUSTER:
		/*
		 * Yeah this is ugly, but necessary to rule out nasty race conditions.
		 * This only happens only when a node join/leave the cluster, thus rare.
		 * Lock is needed inside handle_join_cluster because
		 * we will walk though the list of boards multiple times.
		 */
		pthread_spin_lock(&join_cluster_lock);
		handle_join_cluster(tb);
		pthread_spin_unlock(&join_cluster_lock);
		break;
	case OP_REQ_PINGPONG:
		handle_pingpong(tb);
		break;
	case OP_REQ_QUERY_STAT:
		handle_query_stat(tb);
		break;
	default:
		dprintf_ERROR("received unknown or un-implemented opcode: %u (%s)\n",
			opcode, legomem_opcode_str(opcode));
		return;
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
}

/*
 * Dispatcher uses local mgmt session to receive and send back msg.
 * Since mgmt session does not have any remote end's information,
 * we must rely on each packet's routing info to send it back.
 */
static void *dispatcher(void *_unused)
{
	struct thpool_buffer *tb;
	struct thpool_worker *tw;
	int ret;

	tb = thpool_buffer_map;
	tw = thpool_worker_map;

	ret = net_reg_send_buf(mgmt_session, tb->tx, THPOOL_BUFFER_SIZE);
	if (ret) {
		dprintf_ERROR("Fail to register TX buffer %d\n", ret);
		return NULL;
	}

	while (1) {
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
	return NULL;
}

/* Gather stats from all online nodes. */
__used static void monitor_gather_stats(void)
{
	struct legomem_query_stat_req *req;
	struct legomem_query_stat_resp *resp;
	struct board_info *bi;
	struct lego_header *lego_header;
	struct session_net *ses;
	int ret, i;

	req = malloc(sizeof(*req));
	resp = malloc(legomem_query_stat_resp_size());
	if (!req || !resp)
		return;

	lego_header = to_lego_header(req);
	lego_header->opcode = OP_REQ_QUERY_STAT;

	pthread_spin_lock(&board_lock);
	hash_for_each(board_list, i, bi, link) {
		if (special_board_info_type(bi->flags))
			continue;

		/* Send to remote party's mgmt session */
		ses = get_board_mgmt_session(bi);
		ret = net_send_and_receive(ses, req, sizeof(*req), resp,
					   legomem_query_stat_resp_size());
		if (ret <= 0) {
			dprintf_ERROR("net error: %d\n", ret);
			break;
		}

		/* Copy to per-node stat list */
		memcpy(bi->stat, resp->stat, NR_STAT_TYPES * sizeof(unsigned long));
	}
	pthread_spin_unlock(&board_lock);

	free(req);
	free(resp);
}

/*
 * This is monitor's backround daemon thread.
 * It will monitor cluster status, query stats from other machines,
 * make migration decisions and so on. It runs in conjunction with
 * the handler thread.
 */
static void *daemon_thread_func(void *_unused)
{
#if 0
	while (1) {
		sleep(5);
		monitor_gather_stats();
	}
#endif
	return NULL;
}

/*
 * Creat a background daemon thread that will monitor
 * cluster status and so on.
 */
static void create_daemon_thread(void)
{
	pthread_t t;
	int ret;

	ret = pthread_create(&t, NULL, daemon_thread_func, NULL);
	if (ret) {
		dprintf_ERROR("Fail to create daemon thread%d\n", errno);
		exit(-1);
	}
}

static void print_usage(void)
{
	printf("Usage ./host.o [Options]\n"
	       "\n"
	       "Options:\n"
	       "  --dev=<name>                Specify local network device (Required)\n"
	       "  --port=<port>               Specify local UDP port we listen to (Required)\n"
	       "  --net_raw_ops=[options]     Select raw network layer implementation (Optional)\n"
	       "                              Available Options are:\n"
	       "                                1. raw_verbs (default if nothing is specified)\n"
	       "                                2. raw_udp\n"
	       "                                3. raw_socket\n"
	       "  --net_trans_ops=[options]   Select transport layer implementations (Optional)\n"
	       "                              Available Options are:\n"
	       "                                1. gbn (go-back-N reliable stack, default if nothing is specified)\n"
	       "                                2. bypass (simple bypass transport layer, unreliable)\n"
	       "\n"
	       "Examples:\n"
	       "  ./monitor.o --port 8888 --dev=\"lo\" \n"
	       "  ./monitor.o -p 8888 -d ens4\n");
}

#define OPT_NET_TRANS_OPS		(10000)
static struct option long_options[] = {
	{ "port",		required_argument,	NULL,	'p'},
	{ "dev",		required_argument,	NULL,	'd'},
	{ "net_raw_ops",	required_argument,	NULL,	'n'},
	{ "net_trans_ops",	required_argument,	NULL,	OPT_NET_TRANS_OPS},
	{ 0,			0,			0,	0  }
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
			strncpy(global_net_dev, optarg, sizeof(global_net_dev));
			ndev_set = true;
			break;
		case 'n':
			if (!strncmp(optarg, "raw_verbs", 16))
				raw_net_ops = &raw_verbs_ops;
			else if (!strncmp(optarg, "raw_udp", 16))
				raw_net_ops = &raw_udp_socket_ops;
			else if (!strncmp(optarg, "raw_socket", 16))
				raw_net_ops = &raw_socket_ops;
			else {
				printf("Invalid net_raw_ops: %s\n"
				       "Available Options are:\n"
				       "  1. raw_verbs (default if nothing is specified)\n"
				       "  2. raw_udp\n"
				       "  3. raw_socket\n", optarg);
				exit(-1);
			}
			break;
		case OPT_NET_TRANS_OPS:
			if (!strncmp(optarg, "gbn", 8))
				transport_net_ops = &transport_gbn_ops;
			else if (!strncmp(optarg, "bypass", 8))
				transport_net_ops = &transport_bypass_ops;
			else {
				printf("Invalid net_trans_ops: %s\n"
				       "Available Options are:\n"
				       "1. gbn (go-back-N reliable stack, default if nothing is specified)\n"
				       "2. bypass (simple bypass transport layer, unreliable)\n", optarg);
				exit(-1);
			}
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

	atomic_init(&nr_hosts, 0);
	atomic_init(&nr_boards, 0);

	pthread_spin_init(&proc_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&pid_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&join_cluster_lock, PTHREAD_PROCESS_PRIVATE);

	/* Same as host side init */
	init_board_subsys();
	init_context_subsys();
	init_net_session_subsys();

	ret = init_local_management_session();
	if (ret) {
		printf("Fail to init local mgmt session\n");
		exit(-1);
	}

	/*
	 * Add a special localhost board_info
	 * and a special localhost session_net
	 */
	add_localhost_bi(&default_local_ei);

	create_daemon_thread();

	/*
	 * Now init the thpool stuff and create a new thread
	 * to handle the mgmt session traffic. 
	 */
	init_thpool(NR_THPOOL_WORKERS, &thpool_worker_map);
	init_thpool_buffer(NR_THPOOL_BUFFER, &thpool_buffer_map,
			   default_thpool_buffer_alloc_cb);

	ret = pthread_create(&mgmt_session->thread, NULL, dispatcher, NULL);
	if (ret) {
		dprintf_ERROR("Fail to create mgmt thread %d\n", errno);
		exit(-1);
	}
	pthread_join(mgmt_session->thread, NULL);
}
