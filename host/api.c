/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/page.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"
#include "net/net.h"

/*
 * Intialized during init_monitor_session.
 */
unsigned int monitor_ip_h;
char monitor_ip_str[INET_ADDRSTRLEN];
struct endpoint_info monitor_ei;
struct session_net *monitor_session;
struct board_info *monitor_bi;

/*
 * Depends on MTU: sysctl_link_mtu
 */
int max_lego_payload ____cacheline_aligned = 1300;

/*
 * Allocate a new process-local legomem context.
 * Monitor will be contacted. On success, the context is returned.
 *
 * Note that we will not contact boards at this point.
 * We leave that to legomem_alloc time, and totally handled by board.
 */
struct legomem_context *legomem_open_context(void)
{
	struct legomem_context *p;
	struct legomem_create_context_req req;
	struct legomem_create_context_resp resp;
	struct lego_header *lego_header;
	int ret;

	p = malloc(sizeof(*p));
	if (!p)
		return NULL;
	init_legomem_context(p);

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_CREATE_PROC;
	memset(req.op.proc_name, 'a', PROC_NAME_LEN);

	ret = net_send_and_receive_lock(monitor_session, &req, sizeof(req),
					&resp, sizeof(resp));
	if (ret <= 0) {
		dprintf_DEBUG("net error: %d\n", ret);
		goto err;
	}

	if (resp.op.ret) {
		dprintf_DEBUG("monitor fail to create context: %d\n",
			resp.op.ret);
		goto err;
	}

	p->pid = resp.op.pid;

	/* Add to per-node context list */
	ret = add_legomem_context(p);
	if (ret)
		goto err;
	return p;

err:
	free(p);
	return NULL;
}

/*
 * Close a given legomem context. Monitor will be contacted.
 * All resource associated with this context will be freed.
 */
int legomem_close_context(struct legomem_context *ctx)
{
	struct legomem_close_context_req req;
	struct legomem_close_context_resp resp;
	struct lego_header *lego_header;
	int ret;

	if (!ctx)
		return -EINVAL;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_FREE_PROC;
	lego_header->pid = ctx->pid;

	ret = net_send_and_receive_lock(monitor_session, &req, sizeof(req),
					&resp, sizeof(resp));
	if (ret <= 0) {
		dprintf_DEBUG("net error: %d\n", ret);
		return -EIO;
	}

	if (resp.ret) {
		dprintf_DEBUG("monitor fail to close context (pid %u) ret %d\n",
			ctx->pid, resp.ret);
		return -EFAULT;
	}

	remove_legomem_context(ctx);
	free(ctx);
	return 0;
}

int generic_handle_close_session(struct legomem_context *ctx,
				 struct board_info *bi,
				 struct session_net *ses)
{
	/*
	 * If we have created an user session handler thread,
	 * we need to stop it
	 */
	if (ses->flags & SESSION_NET_FLAGS_THREAD_CREATED) {
		WRITE_ONCE(ses->thread_should_stop, true);
		pthread_join(ses->thread, NULL);
	}

	/*
	 * Clear bookkeeping we've done when the session was open:
	 * Check __legomem_open_session and generic_handle_open_session.
	 */
	board_remove_session(bi, ses);
	if (ctx)
		context_remove_session(ctx, ses);

	/* Finally ask network layer to free resources */
	net_close_session(ses);
	return 0;
}

/*
 * @dst_sesis is from sender
 * we will allocate a local session id
 * 
 * XXX: if we use connectionless rpc, this will not be called
 */
struct session_net *
generic_handle_open_session(struct board_info *bi, unsigned int dst_sesid)
{
	struct session_net *ses;
	int ret;

	ses = net_open_session(&bi->local_ei, &bi->remote_ei);
	if (!ses)
		return NULL;
	ses->board_ip = bi->board_ip;
	ses->udp_port = bi->udp_port;
	ses->board_info = bi;

	set_remote_session_id(ses, dst_sesid);
	board_add_session(bi, ses);

	/*
	 * Create a server side handling thread
	 * for this new session, if needed:
	 */
#if 1
	ret = pthread_create(&ses->thread, NULL, user_session_handler, ses);
	if (ret) {
		dprintf_ERROR("Fail to create the assocaited handler thread "
			      "for session (local id %d)\n",
			      get_local_session_id(ses));
		board_remove_session(bi, ses);
		net_close_session(ses);
		return NULL;
	}
	ses->flags |= SESSION_NET_FLAGS_THREAD_CREATED;
#endif
	return ses;
}

/*
 * When open a session, there is a sender and a receiver.
 * This function is for the sender side.
 */
static struct session_net *
____legomem_open_session(struct legomem_context *ctx, struct board_info *bi,
		       pid_t tid, bool is_local_mgmt, bool is_remote_mgmt)
{
	struct session_net *ses;
	unsigned int dst_sesid = 0;

	if (!bi)
		return NULL;

	/*
	 * Only one option can be true
	 * We do not do mgmt2mgmt connection
	 * We will open a local session to connect to remote mgmt session.
	 */
	if (is_local_mgmt && is_remote_mgmt)
		return NULL;

	/* Ask network layer to prepare data structures for this ses */
	ses = net_open_session(&bi->local_ei, &bi->remote_ei);
	if (!ses)
		return NULL;
	ses->board_ip = bi->board_ip;
	ses->udp_port = bi->udp_port;
	ses->board_info = bi;
	ses->tid = tid;

	/*
	 * Local special sessions do not need
	 * any information about remote parties
	 */
	if (is_local_mgmt)
		goto bookkeeping;

	/*
	 * Setup remote session id
	 * If we are talking with remote mgmt session, use 0.
	 * Otherwise, talk to remote mgmt session.
	 */
	if (is_remote_mgmt) {
		dst_sesid = LEGOMEM_MGMT_SESSION_ID;
		set_remote_session_id(ses, dst_sesid);
	} else {
#ifdef CONFIG_TRANSPORT_GBN
		/*
		 * Contact the remote party to open a network session.
		 * If things go well, a remote session ID is returned.
		 */
		struct legomem_open_close_session_req req = { 0 };
		struct legomem_open_close_session_resp resp;
		struct lego_header *lego_header;
		struct session_net *remote_mgmt_ses;
		int ret;

		lego_header = to_lego_header(&req);
		lego_header->opcode = OP_OPEN_SESSION;
		lego_header->size = sizeof(req) - LEGO_HEADER_OFFSET;

		if (ctx) {
			lego_header->pid = ctx->pid;
		} else {
			/*
			 * User does not provide ctx to associate the session.
			 * This is okay but we lost some bookkeeping, and the
			 * remote will not know who requested this.
			 */
			lego_header->pid = 0;
		}

		req.op.session_id = get_local_session_id(ses);

		remote_mgmt_ses = get_board_mgmt_session(bi);
		if (!remote_mgmt_ses) {
			dump_boards();
			dump_net_sessions();
			printf("%s: bi->name %s\n", __func__, bi->name);
			BUG();
		}

		ret = net_send_and_receive_lock(remote_mgmt_ses, &req, sizeof(req),
						&resp, sizeof(resp));
		if (ret <= 0) {
			dprintf_ERROR("Fail to contact remote party %s\n", bi->name);
			net_close_session(ses);
			return NULL;
		}

		if (unlikely(resp.op.session_id == 0)) {
			dprintf_DEBUG("remote fail to open session %s\n", bi->name);
			net_close_session(ses);
			return NULL;
		}

		dst_sesid = resp.op.session_id;
#else
		/*
		 * If connectionless rpc, there is no destination session,
		 * so we will set dst_sesid the same as local session id
		 */
		dst_sesid = ses->session_id;
#endif
		set_remote_session_id(ses, dst_sesid);

	}

	/*
	 * Patch the UDP port
	 * for raw verbs.
	 */
	ses->route.udp.src_port = htons(global_base_udp_port + get_local_session_id(ses));
	ses->route.udp.dst_port = htons(bi->udp_port + get_remote_session_id(ses));

bookkeeping:
	board_add_session(bi, ses);
	if (ctx)
		context_add_session(ctx, ses);

	dprintf_DEBUG("remote=%s src_sesid=%u, dst_sesid=%u\n",
		bi->name, get_local_session_id(ses), dst_sesid);

	dump_net_sessions();
	return ses;
}

static pthread_spinlock_t open_ses_lock;

__constructor
static void init_open_ses_lock(void)
{
	pthread_spin_init(&open_ses_lock, PTHREAD_PROCESS_PRIVATE);
}

static struct session_net *
__legomem_open_session(struct legomem_context *ctx, struct board_info *bi,
		       pid_t tid, bool is_local_mgmt, bool is_remote_mgmt)
{
	struct session_net *ses;

	pthread_spin_lock(&open_ses_lock);
	ses = ____legomem_open_session(ctx, bi, tid, is_local_mgmt, is_remote_mgmt);
	pthread_spin_unlock(&open_ses_lock);
	return ses;
}

/*
 * Public API
 * Open a normal network connection.
 */
struct session_net *
legomem_open_session(struct legomem_context *ctx, struct board_info *bi)
{
	pid_t tid = gettid();

	/* This is user-visiable API, thus both mgmt options are false */
	return __legomem_open_session(ctx, bi, tid, false, false);
}

/*
 * We also register a send buffer for each mgmt session.
 * This registered buffer will be used to send control msg etc.
 */
struct session_net *
legomem_open_session_remote_mgmt(struct board_info *bi)
{
	void *buf;
	struct session_net *ses;

	ses = __legomem_open_session(NULL, bi, 0, false, true);
	if (!ses)
		return NULL;

	buf = mmap(0, 4096, PROT_READ | PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	if (!buf) { 
		legomem_close_session(NULL, ses);
		return NULL;
	}

	net_reg_send_buf(ses, buf, 4096);
	return ses;
}

struct session_net *
legomem_open_session_local_mgmt(struct board_info *bi)
{
	return __legomem_open_session(NULL, bi, 0, true, false);
}

/*
 * Close a open network session.
 * This runs on the sender side of the session. We will send a request
 * to the receiver side to ask to close it as well.
 */
int legomem_close_session(struct legomem_context *ctx, struct session_net *ses)
{
	struct board_info *bi;
	int ret __maybe_unused;

	bi = ses->board_info;

	/*
	 * If a session involves mgmt session at either end,
	 * we do not need to contact the remote party: it is
	 * established without contacting them (check __legomem_open_session).
	 * For all other normal user sessions, we need to contact remote.
	 */
#ifdef CONFIG_TRANSPORT_GBN
	if (!test_management_session(ses)) {
		struct legomem_open_close_session_req req;
		struct legomem_open_close_session_resp resp;
		struct lego_header *lego_header;
		struct session_net *remote_mgmt_ses;

		lego_header = to_lego_header(&req);
		lego_header->opcode = OP_CLOSE_SESSION;
		lego_header->size = sizeof(req) - LEGO_HEADER_OFFSET;

		if (ctx)
			lego_header->pid = ctx->pid;
		else {
			/*
			 * User does not provide ctx to associate the session.
			 * This is okay but we lost some bookkeeping, and the
			 * remote will not know who requested this.
			 *
			 * Same as open_session.
			 */
			lego_header->pid = 0;
		}

		/*
		 * Yes, virginia. We need to use the remote side session id.
		 * That's our counterparts.
		 */
		req.op.session_id = get_remote_session_id(ses);

		remote_mgmt_ses = get_board_mgmt_session(bi);
		BUG_ON(!remote_mgmt_ses);

		ret = net_send_and_receive_lock(remote_mgmt_ses, &req, sizeof(req),
						&resp, sizeof(resp));
		if (ret <= 0) {
			dprintf_ERROR("Fail to contact remote party %s [%u-%u]\n",
				bi->name, get_local_session_id(ses),
				get_remote_session_id(ses));
			return -EIO;
		}

		if (resp.op.session_id != 0) {
			dprintf_DEBUG("Remote fail to close session %s [%u-%u]\n",
				bi->name, get_local_session_id(ses),
				get_remote_session_id(ses));
			return -EFAULT;
		}
	}
#endif

	dprintf_DEBUG("remote=%s src_sesid=%u, dst_sesid=%u\n",
		bi->name, get_local_session_id(ses), get_remote_session_id(ses));

	return generic_handle_close_session(ctx, bi, ses);
}

/*
 * This function will contact monitor and ask for a new vRegion index
 * and its newly assigned board. On success, both info will be returned.
 * Also, the board is not notified at this time.
 */
static int
ask_monitor_for_new_vregion(struct legomem_context *ctx, size_t size,
			    unsigned long vm_flags,
			    int *board_ip, unsigned int *board_port,
			    unsigned int *vregion_idx)
{
#if 1
	struct legomem_alloc_free_req req;
	struct legomem_alloc_free_resp resp;
	struct lego_header *lego_header;
	int ret;

	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_ALLOC;
	lego_header->pid = ctx->pid;
	lego_header->size = sizeof(req) - LEGO_HEADER_OFFSET;

	req.op.len = size;
	req.op.vm_flags = 0;

	ret = net_send_and_receive_lock(monitor_session, &req, sizeof(req),
					&resp, sizeof(resp));
	if (ret <= 0) {
		dprintf_ERROR("net error %d\n", ret);
		return -EIO;
	}

	dprintf_INFO("Monitor replied %d %d\n", resp.op.board_ip, resp.op.udp_port);

	if (resp.op.ret) {
		dprintf_ERROR("Monitor fail to pick a new vRegion %d\n",
			resp.op.ret);
		return resp.op.ret;
	}

	/*
	 * Success. Monitor picked a new board and a new vRegion index
	 * board_ip+udp_port uniquely identify a legomem instance
	 */
	*board_ip = resp.op.board_ip;
	*board_port = resp.op.udp_port;
	*vregion_idx = resp.op.vregion_idx;
	return 0;
#else
	/*
	 * DEBUG version
	 * Just local allocation
	 */
	static int __i = 3;
	static int __j = 0;
	struct board_info *bi;

	bi = find_board_by_id(__i++);
	*board_ip = bi->board_ip;
	*board_port = bi->udp_port;
	*vregion_idx = __j++;
	return 0;
#endif
}

/*
 * This function tries to find an already established vregion that could
 * possibily allocate @size mem for us. Return the vRegion if found one,
 * NULL otherwise and caller needs to contact monitor.
 */
struct legomem_vregion *
find_vregion_candidate(struct legomem_context *p, size_t size)
{
	struct legomem_vregion *v;

	/*
	 * This is a small opt.
	 * nr_nonzero_vregions means then number of vRegions
	 * that still have available meomory. If this number is 0,
	 * we can skip checking the list and directly go to monitor.
	 *
	 * This does not work too well when there is fragmentation.
	 */
	if (atomic_load(&p->nr_nonzero_vregions) == 0)
		return NULL;

	/* Search from HEAD */
	pthread_spin_lock(&p->lock);
	list_for_each_entry(v, &p->alloc_list_head, list) {
		/* Found one */
		if (atomic_load(&v->avail_space) >= size) {
			pthread_spin_unlock(&p->lock);
			return v;
		}

		/*
		 * If this one is zero, then all the ones behind it are zero.
		 * Because we will move them to tail.
		 * Of course, this is not perfect.. should use a priority queue.
		 */
		if (atomic_load(&v->avail_space) == 0)
			break;
	}
	pthread_spin_unlock(&p->lock);
	return NULL;
}

static inline void
adjust_vregion_avail_size(struct legomem_context *ctx,
			  struct legomem_vregion *v, unsigned int size)
{
	int left;

	atomic_fetch_sub(&v->avail_space, size);
	left = atomic_load(&v->avail_space);

	if (left > 0)
		return;

	if (left < 0) {
		dprintf_ERROR("BUG!! left avail_space: %d\n", left);
		atomic_store(&v->avail_space, 0);
	}

	/*
	 * Move it to the TAIL of allocated vregion list
	 * so the next searcher can hopefully start from
	 * a new one:
	 */
	atomic_fetch_sub(&ctx->nr_nonzero_vregions, 1);
	vregion_alloclist_move_to_tail(ctx, v);
}

/*
 * @ctx: the legomem context
 * @size: the allocation size
 * @vm_flags: the read/write permission
 *
 * Return 0 on error otherwise return the allocated remote address.
 */
unsigned long __remote
legomem_alloc(struct legomem_context *ctx, size_t size, unsigned long vm_flags)
{
	struct legomem_alloc_free_req *req;
	struct legomem_alloc_free_resp *resp;
	size_t resp_size;
	struct lego_header *lego_header;
	struct legomem_vregion *v;
	struct board_info *bi;
	struct session_net *ses, *ses_vregion;
	unsigned int udp_port = 0, vregion_idx = 0;
	int board_ip = 0;
	int ret;
	pid_t tid;
	unsigned long __remote addr;
	bool new_session;

	tid = gettid();

	/*
	 * Step I:
	 * Try to find an existing vregion that could possibly fulfill
	 * this alloc request. We first search local, if not found, we
	 * will contact monitor.
	 */
	v = find_vregion_candidate(ctx, size);
	if (v) {
		vregion_idx = legomem_vregion_to_index(ctx, v);
		board_ip = v->board_ip;
		udp_port = v->udp_port;
	} else {
		/*
		 * Otherwise we ask monitor to alloc a new vRegion
		 * and it will tell us the new board and vRegion index
		 */
		ret = ask_monitor_for_new_vregion(ctx, size, vm_flags,
						  &board_ip, &udp_port, &vregion_idx);
		if (ret) {
			dprintf_ERROR("fail to get new vRegion from monitor %d\n",ret);
			return 0;
		}

		v = index_to_legomem_vregion(ctx, vregion_idx);
		if (unlikely(TestSetVregionAllocated(v))) {
			dprintf_ERROR("vregion was allocated before. "
				      "Either monitor or us has a bug. vregion_idx=%u\n",
					vregion_idx);
			return 0;
		}
		init_legomem_vregion(v);
		v->board_ip = board_ip;
		v->udp_port = udp_port;

		/*
		 * Insert newly allocated ones into HEAD
		 */
		vregion_alloclist_enqueue_head(ctx, v);
		atomic_fetch_add(&ctx->nr_nonzero_vregions, 1);
	}
	adjust_vregion_avail_size(ctx, v, size);

	bi = find_board(board_ip, udp_port);
	if (!bi) {
		char ip_str[INET_ADDRSTRLEN];
		get_ip_str(board_ip, ip_str);
		dprintf_ERROR("Board not found: ip %s port %u. "
			      "The board was selected by monitor.\n",
			      ip_str, udp_port);
		return 0;
	}

	/*
	 *
	 * Step II:
	 * We need to check if this running _thread_ (not process)
	 * has established a network connection with this particular board.
	 *
	 * We search the per-context cached session hashtable.
	 * The key is current thread tid, board_ip+udp_port.
	 */
	new_session = false;
	ses = context_find_session(ctx, tid, board_ip, udp_port);
	if (!ses) {
		/*
		 * There wasn't, so we create a new one.
		 * Once the session is open, it will be inserted
		 * into the per-context session hashtable,
		 * thus visiable to context_find_session() afterwards.
		 */
	dprintf_CRIT("NEW SESSION selected vregion_idx %u, mapped to board: %s new_session: %d\n",
			vregion_idx, bi->name, new_session);

		ses = __legomem_open_session(ctx, bi, tid, false, false);
		if (!ses) {
			dprintf_ERROR("Fail to open a net session with "
				      "the newly selected board: %s\n", bi->name);
			return 0;
		}
		new_session = true;
	}

	dprintf_CRIT("selected vregion_idx %u, mapped to board: %s new_session: %d\n",
			vregion_idx, bi->name, new_session);
	/*
	 * Step III:
	 *
	 * Next check if this new session has already been
	 * inserted into the per-vRegion cached session hashtable.
	 */
	ses_vregion = find_vregion_session(v, tid);
	if (ses_vregion) {
		if (unlikely(ses_vregion != ses)) {
			dprintf_ERROR("BUG. If a session is found in the per-vRegion "
				      "list, then it should match the one in the "
				      "per-context list.\n"
				      "    per-vRegion: %#lx per-context: %#lx new_session: %d\n",
					(unsigned long)ses_vregion,
					(unsigned long)ses,
					new_session);
			return 0;
		}
	} else {
		/*
		 * Otherwise we need to add it to the per-vRegion list.
		 * This session could be newly created, also can be an old
		 * one used by other vRegions.
		 */
		add_vregion_session(v, ses);
		dump_legomem_vregion(ctx, v);
	}

	/*
	 * Step IV:
	 * All good, once we are here, it means
	 * we have a valid vRegion and net session.
	 */
	req = net_get_send_buf(bi->mgmt_session);
	lego_header = to_lego_header(req);
	lego_header->opcode = OP_REQ_ALLOC;
	lego_header->pid = ctx->pid;
	lego_header->size = sizeof(*req) - LEGO_HEADER_OFFSET;

	req->op.len = size;
	req->op.vregion_idx = vregion_idx;
	req->op.vm_flags = vm_flags;

	ret = net_send_and_receive_zerocopy_lock(bi->mgmt_session,
						 req, sizeof(*req),
						 (void **)&resp, &resp_size);
	if (unlikely(ret <= 0)) {
		dprintf_ERROR("net error %d\n", ret);
		return ret;
	}

	/* Check response from the board */
	if (resp->op.ret != 0) {
		/*
		 * TODO
		 *
		 * there should be some legitimate failures for which
		 * we need to handle. e.g., concurrent alloc make this
		 * vregion too small thus we need to find a new one,
		 * monitor somehow decided to free the vRegion thus we need
		 * to ask monitor again etc. Those are valid failures.
		 */
		dprintf_ERROR("remote alloc failure %d\n", resp->op.ret);
		return 0;
	}

	addr = resp->op.addr;

	dprintf_DEBUG("allocated: addr [%#lx %#lx) size %#lx\n", addr, addr + size, size);
	return addr;
}

int legomem_free(struct legomem_context *ctx,
		 unsigned long __remote addr, size_t size)
{
	struct legomem_vregion *v;
	struct session_net *ses;
	struct legomem_alloc_free_req *req;
	struct legomem_alloc_free_resp *resp;
	size_t resp_size;
	struct lego_header *lego_header;
	struct board_info *bi;
	int ret;

	v = va_to_legomem_vregion(ctx, addr);
	bi = find_board(v->board_ip, v->udp_port);
	if (!bi) {
		char ip_str[INET_ADDRSTRLEN];
		get_ip_str(v->board_ip, ip_str);
		dprintf_ERROR("Fail to find associated board: %s:%d\n",
			ip_str, v->udp_port);
		return -ENODEV;
	}

	ses = get_board_mgmt_session(bi);
	req = net_get_send_buf(ses);

	lego_header = to_lego_header(req);
	lego_header->opcode = OP_REQ_FREE;
	lego_header->pid = ctx->pid;
	lego_header->size = sizeof(*req) - LEGO_HEADER_OFFSET;

	req->op.addr = addr;
	req->op.len = size;

	ret = net_send_and_receive_zerocopy_lock(ses, req, sizeof(*req),
						 (void **)&resp, &resp_size);
	if (ret <= 0) {
		dprintf_ERROR("net error %d\n", ret);
		return -EIO;
	}

	atomic_fetch_add(&v->avail_space, size);
	return 0;
}

/*
 * Find the per-thread session associated with vregion @v.
 */
struct session_net *__find_or_alloc_vregion_session(struct legomem_context *ctx,
						    struct legomem_vregion *v)
{
	char ip_str[INET_ADDRSTRLEN];
	struct session_net *ses;
	pid_t tid;

	tid = gettid();
	ses = find_vregion_session(v, tid);
	if (likely(ses))
		return ses;

	/*
	 * Find if this particular thread has an established
	 * session with the owner board of this vregion.
	 */
	ses = context_find_session(ctx, tid, v->board_ip, v->udp_port);
	if (!ses) {
		struct board_info *bi;

		bi = find_board(v->board_ip, v->udp_port);
		if (!bi) {
			get_ip_str(v->board_ip, ip_str);
			dprintf_ERROR("Fail to find board: %s:%d\n", ip_str, v->udp_port);
			return NULL;
		}

		ses = __legomem_open_session(ctx, bi, tid, false, false);
		if (!ses) {
			get_ip_str(v->board_ip, ip_str);
			dprintf_ERROR("Fail to open a net session "
				      "with board: %s:%d\n", ip_str, v->udp_port);
			return NULL;
		}
	}
	add_vregion_session(v, ses);

#ifdef LEGOMEM_DEBUG
	dprintf_INFO("new session %d enqueued into vregion %d, pid %u\n",
		get_local_session_id(ses), legomem_vregion_to_index(ctx, v), tid);
	dump_legomem_vregion(ctx, v);
#endif
	return ses;
}

struct session_net *find_or_alloc_vregion_session(struct legomem_context *ctx,
						  unsigned long __remote addr)
{
	struct legomem_vregion *v;

	v = va_to_legomem_vregion(ctx, addr);
	if (!VregionAllocated(v)) {
		dprintf_ERROR("remote va %#lx vregion index %#x not allocated\n",
			addr, va_to_vregion_index(addr));
		return NULL;
	}
	return __find_or_alloc_vregion_session(ctx, v);
}

int legomem_read_with_session_msgbuf(struct legomem_context *ctx, struct session_net *ses,
				     struct msg_buf *send_mb, void *recv_buf,
				     unsigned long __remote addr, size_t total_size)
{
	struct legomem_read_write_req *req;
	struct legomem_read_write_resp *resp;
	struct lego_header *tx_lego;
	struct lego_header *rx_lego __maybe_unused;
	int nr_sent, ret, i;
	size_t sz, recv_size;

	req = send_mb->buf;
	tx_lego = to_lego_header(req);
	tx_lego->pid = ctx->pid;
	tx_lego->opcode = OP_REQ_READ;

	nr_sent = 0;
	do {
		if (total_size >= max_lego_payload)
			sz = max_lego_payload;
		else
			sz = total_size;
		total_size -= sz;

		tx_lego->tag = nr_sent;
		req->op.va = addr + nr_sent * max_lego_payload;
		req->op.size = sz;

		ret = net_send_msg_buf(ses, send_mb, sizeof(*req));
		if (unlikely(ret < 0)) {
			dprintf_ERROR("Fail to send read at nr_sent: %d\n", nr_sent);
			break;
		}
		nr_sent++;
	} while (total_size);

	/* Shift to start of payload */
	recv_buf += sizeof(*resp);
	for (i = 0; i < nr_sent; i++) {
		ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);
		if (unlikely(ret <= 0)) {
			dprintf_ERROR("Fail to recv read at %dth reply\n", i);
			continue;
		}

#if 1
		/* Sanity Checks */
		rx_lego = to_lego_header(resp);
		if (unlikely(rx_lego->req_status != 0)) {
			dprintf_ERROR("errno: req_status=%x\n", rx_lego->req_status);
			continue;
		}
		if (unlikely(rx_lego->opcode != OP_REQ_READ_RESP)) {
			dprintf_ERROR("errnor: invalid resp msg %s\n",
				legomem_opcode_str(rx_lego->opcode));
			continue;
		}
		/* Minus header to get lego payload size */
		recv_size -= sizeof(*resp);
		memcpy(recv_buf, resp->ret.data, recv_size);
		recv_buf += recv_size;
#endif
	}
	return 0;
}

int legomem_read_with_session(struct legomem_context *ctx, struct session_net *ses,
			      void *send_buf, void *recv_buf,
			      unsigned long __remote addr, size_t total_size)
{
	struct legomem_read_write_req *req;
	struct legomem_read_write_resp *resp;
	struct lego_header *tx_lego;
	struct lego_header *rx_lego __maybe_unused;
	int nr_sent, ret, i;
	size_t sz, recv_size;

	req = send_buf;
	tx_lego = to_lego_header(req);
	tx_lego->pid = ctx->pid;
	tx_lego->opcode = OP_REQ_READ;

	nr_sent = 0;
	do {
		if (total_size >= max_lego_payload)
			sz = max_lego_payload;
		else
			sz = total_size;
		total_size -= sz;

		tx_lego->tag = nr_sent;
		req->op.va = addr + nr_sent * max_lego_payload;
		req->op.size = sz;

		ret = net_send(ses, req, sizeof(*req));
		if (unlikely(ret < 0)) {
			dprintf_ERROR("Fail to send read at nr_sent: %d\n", nr_sent);
			break;
		}
		nr_sent++;
	} while (total_size);

	/* Shift to start of payload */
	recv_buf += sizeof(*resp);
	for (i = 0; i < nr_sent; i++) {
		ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);
		if (unlikely(ret <= 0)) {
			dprintf_ERROR("Fail to recv read at %dth reply\n", i);
			continue;
		}

		/* Sanity Checks */
		rx_lego = to_lego_header(resp);
		if (unlikely(rx_lego->req_status != 0)) {
			dprintf_ERROR("errno: req_status=%x\n", rx_lego->req_status);
			continue;
		}
		if (unlikely(rx_lego->opcode != OP_REQ_READ_RESP)) {
			dprintf_ERROR("errnor: invalid resp msg %s\n",
				legomem_opcode_str(rx_lego->opcode));
			continue;
		}
		/* Minus header to get lego payload size */
		recv_size -= sizeof(*resp);
		memcpy(recv_buf, resp->ret.data, recv_size);
		recv_buf += recv_size;
	}
	return 0;
}

/*
 * @send_buf: the buf used to send request to remote board.
 *            (Must be DMA-able)
 * @recv_buf: placeholder for returned data, including eth/ip/udp/gbn/lego hdrs.
 *            (No requirement for DMA)
 * @addr: remote virtual address
 * @size: size of data to read
 */
int legomem_read(struct legomem_context *ctx, void *send_buf, void *recv_buf,
		 unsigned long __remote addr, size_t total_size)
{
	struct legomem_vregion *v;
	struct session_net *ses;

	v = va_to_legomem_vregion(ctx, addr);
	ses = __find_or_alloc_vregion_session(ctx, v);
	if (!ses) {
		dump_legomem_vregion(ctx, v);
		dprintf_ERROR("Cannot get or alloc session %#lx\n", addr);
		return -EIO;
	}
	return legomem_read_with_session(ctx, ses, send_buf, recv_buf, addr, total_size);
}

/*
 * BYOMB: Bring your own message buffer.
 * send_mb should be precooked and its buffer should be DMA-able.
 */
int __legomem_write_with_session_msgbuf(struct legomem_context *ctx, struct session_net *ses,
					struct msg_buf *send_mb, unsigned long __remote addr, size_t total_size,
					enum legomem_write_flag flag)
{
	struct legomem_read_write_req *req;
	struct legomem_read_write_resp *resp;
	size_t recv_size, sz; 
	struct lego_header *tx_lego;
	struct lego_header *rx_lego __maybe_unused;
	int i, ret, nr_sent;
	void *send_buf;

	send_buf = send_mb->buf;
	nr_sent = 0;
	do {
		u64 shift;

		if (total_size >= max_lego_payload)
			sz = max_lego_payload;
		else
			sz = total_size;
		total_size -= sz;

		/*
		 * Shift to next pkt start
		 * We will override portion of already-sent user data
		 */
		shift = (u64)(nr_sent * max_lego_payload);
		req = (struct legomem_read_write_req *)((u64)send_buf + shift);
		req->op.va = addr + shift;
		req->op.size = sz;
		tx_lego = to_lego_header(req);
		tx_lego->pid = ctx->pid;
		if (flag == LEGOMEM_WRITE_SYNC)
			tx_lego->opcode = OP_REQ_WRITE;
		else if (flag == LEGOMEM_WRITE_ASYNC)
			tx_lego->opcode = OP_REQ_WRITE_NOREPLY;

		/* Temporary uses the latest buf point */
		send_mb->buf = req;

		ret = net_send_msg_buf(ses, send_mb, sz + sizeof(*req));
		if (unlikely(ret < 0)) {
			dprintf_ERROR("Fail to send write at nr_sent: %d\n", nr_sent);
			break;
		}
		nr_sent++;
	} while (total_size);

	/* Restore the original pointer */
	send_mb->buf = send_buf;

	if (flag == LEGOMEM_WRITE_ASYNC)
		return 0;

	for (i = 0; i < nr_sent; i++) {
		ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);
		if (unlikely(ret <= 0)) {
			dprintf_ERROR("Fail to recv write at %dth reply\n", i);
			continue;
		}
#if 0
		/* Sanity Checks */
		rx_lego = to_lego_header(resp);
		if (unlikely(rx_lego->req_status != 0)) {
			dprintf_ERROR("errno: req_status=%x\n", rx_lego->req_status);
			continue;
		}
		if (unlikely(rx_lego->opcode != OP_REQ_WRITE_RESP)) {
			dprintf_ERROR("errnor: invalid resp msg %s. at %dth reply\n",
				legomem_opcode_str(rx_lego->opcode), i);
			continue;
		}
#endif
	}
	return 0;
}

int __legomem_write_with_session(struct legomem_context *ctx, struct session_net *ses,
				 void *send_buf, unsigned long __remote addr, size_t total_size,
				 enum legomem_write_flag flag)
{
	struct legomem_read_write_req *req;
	struct legomem_read_write_resp *resp;
	size_t recv_size, sz; 
	struct lego_header *tx_lego;
	struct lego_header *rx_lego __maybe_unused;
	int i, ret, nr_sent;

	nr_sent = 0;
	do {
		u64 shift;

		if (total_size >= max_lego_payload)
			sz = max_lego_payload;
		else
			sz = total_size;
		total_size -= sz;

		/*
		 * Shift to next pkt start
		 * We will override portion of already-sent user data
		 */
		shift = (u64)(nr_sent * max_lego_payload);
		req = (struct legomem_read_write_req *)((u64)send_buf + shift);
		req->op.va = addr + shift;
		req->op.size = sz;
		tx_lego = to_lego_header(req);
		tx_lego->pid = ctx->pid;
		if (flag == LEGOMEM_WRITE_SYNC)
			tx_lego->opcode = OP_REQ_WRITE;
		else if (flag == LEGOMEM_WRITE_ASYNC)
			tx_lego->opcode = OP_REQ_WRITE_NOREPLY;

		ret = net_send(ses, req, sz + sizeof(*req));
		if (unlikely(ret < 0)) {
			dprintf_ERROR("Fail to send write at nr_sent: %d\n", nr_sent);
			break;
		}
		nr_sent++;
	} while (total_size);

	if (flag == LEGOMEM_WRITE_ASYNC)
		return 0;

	for (i = 0; i < nr_sent; i++) {
		ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);
		if (unlikely(ret <= 0)) {
			dprintf_ERROR("Fail to recv write at %dth reply\n", i);
			continue;
		}
#if 0
		/* Sanity Checks */
		rx_lego = to_lego_header(resp);
		if (unlikely(rx_lego->req_status != 0)) {
			dprintf_ERROR("errno: req_status=%x\n", rx_lego->req_status);
			continue;
		}
		if (unlikely(rx_lego->opcode != OP_REQ_WRITE_RESP)) {
			dprintf_ERROR("errnor: invalid resp msg %s. at %dth reply\n",
				legomem_opcode_str(rx_lego->opcode), i);
			continue;
		}
#endif
	}
	return 0;
}

static __always_inline int
__legomem_write(struct legomem_context *ctx, void *send_buf,
		unsigned long __remote addr, size_t total_size,
		enum legomem_write_flag flag)
{
	struct legomem_vregion *v;
	struct session_net *ses;

	v = va_to_legomem_vregion(ctx, addr);
	ses = __find_or_alloc_vregion_session(ctx, v);
	if (!ses) {
		dump_legomem_vregion(ctx, v);
		dprintf_ERROR("Cannot get or alloc session %#lx\n", addr);
		return -EIO;
	}
	return __legomem_write_with_session(ctx, ses, send_buf, addr, total_size, flag);
}

/*
 * Perform a legomem write to remote board (s).
 * This is the synchronous version where we will wait for the ACK from board (s).
 */
int legomem_write_sync(struct legomem_context *ctx, void *send_buf,
		       unsigned long __remote addr, size_t size)
{
	return __legomem_write(ctx, send_buf, addr, size, LEGOMEM_WRITE_SYNC);
}

/*
 * This function will return right after the data is sent out from current host. N
 * In other words, there is no guarantee that data has persisted when it returns.
 */
int legomem_write_async(struct legomem_context *ctx, void *send_buf,
			unsigned long __remote addr, size_t size)
{
	return __legomem_write(ctx, send_buf, addr, size, LEGOMEM_WRITE_ASYNC);
}

/*
 * Given the remote memory pointer @ptr, we do a indirection read,
 * return the result into @buf. The pointer @ptr is 8B by default.
 * The final size of @buf is determined by size. It can be expressed as:
 *	_tmp = *ptr;
 *	memcpy(buf, _tmp, size);
 */
int legomem_ptr_chasing_read(struct legomem_context *ctx, void *buf,
			     unsigned long __remote ptr, size_t size)
{
	struct legomem_vregion *v;
	struct session_net *ses;

	v = va_to_legomem_vregion(ctx, ptr);
	ses = __find_or_alloc_vregion_session(ctx, v);
	if (!ses) {
		dump_legomem_vregion(ctx, v);
		dprintf_ERROR("Cannot get or alloc session %#lx\n", ptr);
		return -EIO;
	}

	return 0;
}

/*
 * Given the remote memory pointer @ptr, we do a indirection write:
 *	_tmp = *ptr;
 *	memcpy(_tmp, buf, size)
 */
int legomem_ptr_chasing_write(struct legomem_context *ctx, void *buf,
			      unsigned long __remote ptr, size_t size)
{
	struct legomem_vregion *v;
	struct session_net *ses;

	v = va_to_legomem_vregion(ctx, ptr);
	ses = __find_or_alloc_vregion_session(ctx, v);
	if (!ses) {
		dump_legomem_vregion(ctx, v);
		dprintf_ERROR("Cannot get or alloc session %#lx\n", ptr);
		return -EIO;
	}

	return 0;
}

int legomem_migration_vregion(struct legomem_context *ctx,
			      struct board_info *dst_bi, unsigned int vregion_index)
{
	struct legomem_migration_req req;
	struct legomem_migration_resp resp;
	struct lego_header *lego_header;
	struct op_migration *op;
	struct session_net *ses;
	struct legomem_vregion *v;
	int ret;
	pid_t tid;

	/*
	 * Set migration flag,
	 * stop all accessing of this vRegion
	 */
	v = index_to_legomem_vregion(ctx, vregion_index);

	if (unlikely(!VregionAllocated(v))) {
		dprintf_ERROR("vregion_idx %d is not allocated yet\n", vregion_index);
		return -EINVAL;
	}

	if (unlikely(TestSetVregionMigration(v))) {
		dprintf_DEBUG("vregion_idx %u is being migrated now.\n", vregion_index);
		return -EBUSY;
	}

	/* Prepare legomem header */
	lego_header = to_lego_header(&req);
	lego_header->opcode = OP_REQ_MIGRATION_H2M;
	lego_header->pid = ctx->pid;
	lego_header->size = sizeof(req) - LEGO_HEADER_OFFSET;

	/* Prepare migration req header */
	op = &req.op;
	op->src_board_ip = v->board_ip;
	op->src_udp_port = v->udp_port;
	op->dst_board_ip = dst_bi->board_ip;
	op->dst_udp_port = dst_bi->udp_port;
	op->vregion_index = vregion_index;

	ret = net_send_and_receive_lock(monitor_session, &req, sizeof(req),
					&resp, sizeof(resp));
	if (ret <= 0) {
		dprintf_ERROR("net error %d\n", ret);
		return -EIO;
	}

	if (unlikely(resp.op.ret)) {
		struct board_info *src_bi;
		src_bi = find_board(v->board_ip, v->udp_port);
		dprintf_ERROR("fail to migrate vregion %u from %s to %s\n",
			vregion_index, src_bi->name, dst_bi->name);
		return resp.op.ret;
	}
	dprintf_DEBUG("Monitor replied, data was migrated %d\n", 0);

	/*
	 * Data was migrated. Now update vregion medadata.
	 * First close the original session with the old board,
	 * then open a new session with new board.
	 */
	tid = gettid();
	ses = find_vregion_session(v, tid);
	if (!ses) {
		dprintf_ERROR("Fail to find the original session associated "
			      "with this thread (tid %d) and vregion (%u). "
			      "This is BUG. But we proceed anyway.\n",
			      tid, vregion_index);
		dump_legomem_vregion(ctx, v);
		dump_legomem_context_sessions(ctx);
	} else {
		remove_vregion_session(v, tid);

		/*
		 * XXX We cannot just close this session now.
		 * Because this particular session might also
		 * be used by other vRegions this thread owned.
		 *
		 * A proper way is using refcount/put/get everywhere.
		 * Well, no time for this "decent" stuff now.
		 */
		//legomem_close_session(ctx, ses);
	}

	/*
	 * Find out if this thread already has a session with the new board
	 * If not, we need to open a new one.
	 */
	ses = context_find_session(ctx, tid, dst_bi->board_ip, dst_bi->udp_port);
	if (!ses) {
		ses = __legomem_open_session(ctx, dst_bi, tid, false, false);
		if (!ses) {
			dprintf_ERROR("Fail to open a session with the new board %s. "
				      "Data was already migrated by monitor. "
				      "We are not moving it back, for now.\n", dst_bi->name);
			return 0;
		}
	}
	add_vregion_session(v, ses);

	ClearVregionMigration(v);

	inc_stat(STAT_NR_MIGRATED_VREGION);
	return 0;
}

/*
 * Migrate [address, address+size) from @src_bi to @dst_bi.
 * We will contact monitor first.
 */
int legomem_migration(struct legomem_context *ctx, struct board_info *dst_bi,
		      unsigned long __remote addr, unsigned long size)
{
	unsigned long end;
	unsigned int ret, start_index, end_index;

	end = addr + size - 1;

	start_index = va_to_vregion_index(addr);
	end_index = va_to_vregion_index(end);

	dprintf_INFO("s %d e %d\n", start_index, end_index);
	while (start_index <= end_index) {
		ret = legomem_migration_vregion(ctx, dst_bi, start_index);
		start_index++;

		if (ret)
			break;
	}
	return ret;
}

/*
 * Do pointer chasing
 */
int legomem_pointer_chasing(struct legomem_context *ctx,
		            uint64_t __remote ptr,
			    uint64_t key, uint16_t structSize,
			    uint16_t valueSize, uint8_t keyOffset,
			    uint8_t valueOffset, uint8_t depth,
			    uint8_t nextOffset)
{
	struct legomem_pointer_chasing_req *req;
	struct legomem_read_write_resp *resp;
	struct legomem_vregion *v;
	struct session_net *ses;
	size_t recv_size;
	struct lego_header *tx_lego;
	int ret;

	v = va_to_legomem_vregion(ctx, ptr);
	ses = __find_or_alloc_vregion_session(ctx, v);
	if (!ses) {
		dump_legomem_vregion(ctx, v);
		dprintf_ERROR("Cannot get or alloc session %#lx\n", ptr);
		return -EIO;
	}

	req = net_get_send_buf(ses);
	BUG_ON(!req);

	tx_lego = to_lego_header(req);
	tx_lego->pid = ctx->pid;
	tx_lego->opcode = OP_REQ_POINTER_CHASING;

	req->op.addr = ptr;
	req->op.key = key & 0xFFFFFFFFULL;
	req->op.structSize = structSize;
	req->op.valueSize = valueSize;
	req->op.keyOffset = keyOffset;
	req->op.valueOffset = valueOffset;
	req->op.depth = depth;
	// Must be 64 bit align here
	req->op.nextOffset = nextOffset >> 3;
	req->op.flag_useDepth = 1;
	req->op.flag_useKey = 1;
	req->op.flag_useValuePtr = 1;
	req->op.reserved = 1;

#if 0
	dprintf_INFO("[pc_req] addr %#lx key %#lx structSize %#x valueSize %#x "
		     "keyOffset %#x valueOffset %#x depth %d nextOffset %#x\n",
		req->op.addr, req->op.key, req->op.structSize,
		req->op.valueSize, req->op.keyOffset,
		req->op.valueOffset, req->op.depth, req->op.nextOffset);
#endif

	ret = net_send(ses, req, sizeof(*req));
	if (unlikely(ret < 0)) {
		dprintf_ERROR("Fail to send ptr chasing %d\n", ret);
		return ret;
	}

	ret = net_receive_zerocopy(ses, (void **)&resp, &recv_size);
	if (unlikely(ret <= 0)) {
		dprintf_ERROR("Fail to recv %d\n", ret);
		return ret;
	}
#if 0
	{
		struct lego_header *rx_lego;
		rx_lego = to_lego_header(resp);
		if (rx_lego->req_status) {
			dprintf_ERROR("req_status %#x va %#lx key %#lx depth %d\n",
				rx_lego->req_status, ptr, key, depth);
		}
	}
#endif
	return 0;
}
/*
 * A very simple distributed barrier mechanism.
 * Once received such req, pop up the local barrier counter.
 * legomem_dist_barrier() is spinning on this.
 */
atomic_long legomem_barrier_counter;
void handle_dist_barrier(struct thpool_buffer *tb)
{
	struct legomem_common_headers *tx;
	struct lego_header *lego;

	atomic_fetch_add(&legomem_barrier_counter, 1);

	tx = (struct legomem_common_headers *)tb->tx;
	set_tb_tx_size(tb, sizeof(*tx));

	lego = to_lego_header(tx);
	lego->opcode = OP_REQ_DIST_BARRIER_RESP;

	dprintf_DEBUG("Received a barrier then current_counter=%ld\n",
			atomic_load(&legomem_barrier_counter));
}

void __legomem_dist_barrier(void)
{
#if 0
	while (atomic_load(&legomem_barrier_counter) !=
	       atomic_load(&nr_online_hosts))
		;
#else
	/*
	 * Hardcode the number of other clients except myself.
	 * This is useful if the join timing is hard to control.
	 */
	int __NR_OTHER_CLINETS = 2;
	while (atomic_load(&legomem_barrier_counter) != __NR_OTHER_CLINETS)
		;
#endif
	dprintf_DEBUG("All barriers received. %ld\n", atomic_load(&legomem_barrier_counter));
}

/*
 * Wait for all online clinets have reached this point.
 *
 * Be careful during testing, if a node joined after we've sent
 * out all the notifications.. we may stuck here forever. Thus
 * just make sure all boards have joined _before_ calling this func.
 */
int legomem_dist_barrier(void)
{
	struct board_info *bi;
	struct session_net *ses;
	struct legomem_common_headers *req, *resp;
	size_t resp_size;
	int i;

	for (i = 0; i <= nr_max_board_id; i++) {
		bi = find_board_by_id(i);
		if (!(bi->flags & BOARD_INFO_FLAGS_HOST))
			continue;

		dprintf_DEBUG("sending to host %s\n", bi->name);
		
		ses = get_board_mgmt_session(bi);
		req = net_get_send_buf(ses);
		req->lego.opcode = OP_REQ_DIST_BARRIER;

		net_send_and_receive_zerocopy_lock(ses, req, sizeof(*req),
						   (void **)&resp, &resp_size);
	}

	dprintf_DEBUG("nr_online_hosts: %ld. current_barrier: %ld\n",
			atomic_load(&nr_online_hosts),
			atomic_load(&legomem_barrier_counter));

	__legomem_dist_barrier();

	return 0;
}
