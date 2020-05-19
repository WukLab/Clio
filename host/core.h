/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _HOST_CORE_H_
#define _HOST_CORE_H_

#include <uapi/vregion.h>
#include <uapi/compiler.h>
#include <uapi/sched.h>
#include <uapi/opcode.h>
#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/thpool.h>
#include "net/net.h"
#include <limits.h>
#include <time.h>
#include <pthread.h>

#include "api.h"

#if 0
# define LEGOMEM_DEBUG
#endif

#ifdef LEGOMEM_DEBUG
# define dprintf_DEBUG(fmt, ...) \
	printf("[%s/%s()/%d]: " fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
# define dprintf_DEBUG(fmt, ...)  do { } while (0)
#endif

/* General info, always on */
#define dprintf_INFO(fmt, ...) \
	printf("[%s/%s()/%d]: " fmt, __FILE__, __func__, __LINE__, __VA_ARGS__)

/* ERROR/WARNING info, always on */
#define dprintf_ERROR(fmt, ...) \
	printf("\033[1;31m[%s/%s()/%d]: " fmt "\033[0m", __FILE__, __func__, __LINE__, __VA_ARGS__)

#define dprintf_CRIT(fmt, ...) \
	printf("\033[1;33m[%s/%s()/%d]: " fmt "\033[0m", __FILE__, __func__, __LINE__, __VA_ARGS__)

extern int gbn_polling_thread_cpu;
extern int mgmt_dispatcher_thread_cpu;

extern bool stop_gbn_poll_thread;
extern bool stop_mgmt_dispatcher_thread;

extern int max_lego_payload;

/*
 * pthread_rwlock_t is a big structure, around 50-60B.
 * Thus it's impossible to tame whole structure with one cacheline.
 * With hash entries being 6, we almost fully occupy two full lines.
 */
struct legomem_vregion {
	int board_ip;
	unsigned int udp_port;
	unsigned long flags;
	atomic_int avail_space;

	struct list_head	list;

#define LEGOMEM_VREGION_HT_ENTRIES	(6)
	struct hlist_head ht_sessions[LEGOMEM_VREGION_HT_ENTRIES];
	pthread_rwlock_t rwlock;
} __aligned(64);

enum legomem_vregion_flags {
	V_allocated,
	V_migration,
};

#define TEST_VREGION_FLAG(uname, lname)				\
static inline int Vregion##uname(const struct legomem_vregion *p)	\
{								\
	return test_bit(V_##lname, &p->flags);			\
}

#define SET_VREGION_FLAG(uname, lname)				\
static inline void SetVregion##uname(struct legomem_vregion *p)		\
{								\
	set_bit(V_##lname, &p->flags);				\
}

#define CLEAR_VREGION_FLAG(uname, lname)			\
static inline void ClearVregion##uname(struct legomem_vregion *p)	\
{								\
	clear_bit(V_##lname, &p->flags);			\
}

#define __SET_VREGION_FLAG(uname, lname)			\
static inline void __SetVregion##uname(struct legomem_vregion *p)	\
{								\
	__set_bit(V_##lname, &p->flags);			\
}

#define __CLEAR_VREGION_FLAG(uname, lname)			\
static inline void __ClearVregion##uname(struct legomem_vregion *p)	\
{								\
	__clear_bit(V_##lname, &p->flags);			\
}

#define TEST_SET_FLAG(uname, lname)				\
static inline int TestSetVregion##uname(struct legomem_vregion *p)	\
{								\
	return test_and_set_bit(V_##lname, &p->flags);		\
}

#define TEST_CLEAR_FLAG(uname, lname)				\
static inline int TestClearVregion##uname(struct legomem_vregion *p)	\
{								\
	return test_and_clear_bit(V_##lname, &p->flags);	\
}

#define __TEST_SET_FLAG(uname, lname)				\
static inline int __TestSetVregion##uname(struct legomem_vregion *p)	\
{								\
	return __test_and_set_bit(V_##lname, &p->flags);	\
}

#define __TEST_CLEAR_FLAG(uname, lname)				\
static inline int __TestClearVregion##uname(struct legomem_vregion *p)	\
{								\
	return __test_and_clear_bit(V_##lname, &p->flags);	\
}

#define VREGION_FLAG(uname, lname)				\
	TEST_VREGION_FLAG(uname, lname)				\
	SET_VREGION_FLAG(uname, lname)				\
	CLEAR_VREGION_FLAG(uname, lname)			\
	__SET_VREGION_FLAG(uname, lname)			\
	__CLEAR_VREGION_FLAG(uname, lname)			\
	TEST_SET_FLAG(uname, lname)				\
	TEST_CLEAR_FLAG(uname, lname)				\
	__TEST_SET_FLAG(uname, lname)				\
	__TEST_CLEAR_FLAG(uname, lname)

VREGION_FLAG(Allocated, allocated)
VREGION_FLAG(Migration, migration)

/*
 * The following couple functions play with the per-vregion session list.
 * This session hashtable is keyed by local kernel thread id (pid),
 * it is used to diffrentiate multiple threads from the same process.
 * In the hope of better performance, we used a rwlock instead of a spinlock.
 */
static __always_inline int
add_vregion_session(struct legomem_vregion *v, struct session_net *ses)
{
	int key;

	key = ses->tid;

	pthread_rwlock_wrlock(&v->rwlock);
	hash_add(v->ht_sessions, &ses->ht_link_vregion, key);
	pthread_rwlock_unlock(&v->rwlock);

	return 0;
}

static __always_inline int
remove_vregion_session(struct legomem_vregion *v, pid_t tid) 
{
	struct session_net *ses;
	int key = tid;

	pthread_rwlock_wrlock(&v->rwlock);
	hash_for_each_possible(v->ht_sessions, ses, ht_link_vregion, key) {
		if (likely(tid == ses->tid)) {
			hash_del(&ses->ht_link_vregion);
			pthread_rwlock_unlock(&v->rwlock);
			return 0;
		}
	}
	pthread_rwlock_unlock(&v->rwlock);
	return -1;
}

/*
 * Frequently used, sitting on datapath
 * Find out if thread @tid has a session associated with vregion @v.
 */
static __always_inline struct session_net *
find_vregion_session(struct legomem_vregion *v, pid_t tid)
{
	struct session_net *ses;
	int key = tid;

	pthread_rwlock_rdlock(&v->rwlock);
	hash_for_each_possible(v->ht_sessions, ses, ht_link_vregion, key) {
		if (likely(tid == ses->tid)) {
			pthread_rwlock_unlock(&v->rwlock);
			return ses;
		}
	}
	pthread_rwlock_unlock(&v->rwlock);
	return NULL;
}

static inline void init_legomem_vregion(struct legomem_vregion *v)
{
	atomic_store(&v->avail_space, VREGION_SIZE);
	pthread_rwlock_init(&v->rwlock, NULL);
}

/*
 * HACK!!
 * If you add anything, please update init_legomem_context()!
 */
struct legomem_context {
	unsigned long flags;
	unsigned int pid;

	/* List all contexts */
	struct hlist_node link;

	/*
	 * List of all open sessions of this context.
	 * Served as a cache for future vRegion additions.
	 */
#define LEGOMEM_CONTEXT_HT_ENTREIS (16)
	struct hlist_head ht_sessions[LEGOMEM_CONTEXT_HT_ENTREIS];
	pthread_spinlock_t lock;

	struct legomem_vregion vregion[NR_VREGIONS];
	struct list_head alloc_list_head;
	atomic_int nr_nonzero_vregions;
} __aligned(64);

static inline struct legomem_vregion * 
__vregion_alloclist_dequeue_head(struct legomem_context *ctx)
{
	struct legomem_vregion *v;

	v = list_entry(ctx->alloc_list_head.next, struct legomem_vregion, list);
	list_del(&v->list);
	return v;
}

static inline struct legomem_vregion * 
vregion_allolist_dequeue_head(struct legomem_context *ctx)
{
	struct legomem_vregion *v = NULL;

	pthread_spin_lock(&ctx->lock);
	if (!list_empty(&ctx->alloc_list_head))
		v = __vregion_alloclist_dequeue_head(ctx);
	pthread_spin_unlock(&ctx->lock);
	return v;
}

static inline void
__vregion_alloclist_enqueue_tail(struct legomem_context *ctx,
			       struct legomem_vregion *v)
{
	list_add_tail(&v->list, &ctx->alloc_list_head);
}

static inline void
__vregion_alloclist_enqueue_head(struct legomem_context *ctx,
			       struct legomem_vregion *v)
{
	list_add(&v->list, &ctx->alloc_list_head);
}

static inline void
vregion_alloclist_enqueue_head(struct legomem_context *ctx,
			       struct legomem_vregion *v)
{
	pthread_spin_lock(&ctx->lock);
	__vregion_alloclist_enqueue_head(ctx, v);
	pthread_spin_unlock(&ctx->lock);
}

static inline void
vregion_alloclist_move_to_tail(struct legomem_context *ctx,
				struct legomem_vregion *v)
{
	pthread_spin_lock(&ctx->lock);
	list_del(&v->list);
	__vregion_alloclist_enqueue_tail(ctx, v);
	pthread_spin_unlock(&ctx->lock);
}

static inline void init_legomem_context(struct legomem_context *p)
{
	int i;

	BUG_ON(!p);

	memset(p, 0, sizeof(*p));
	INIT_HLIST_NODE(&p->link);
	pthread_spin_init(&p->lock, PTHREAD_PROCESS_PRIVATE);

	/* vregion related */
	INIT_LIST_HEAD(&p->alloc_list_head);
	atomic_store(&p->nr_nonzero_vregions, 0);
	for (i = 0; i < NR_VREGIONS; i++) {
		struct legomem_vregion *v;
		v = p->vregion + i;
		init_legomem_vregion(v);
	}
}

static inline struct legomem_vregion *
va_to_legomem_vregion(struct legomem_context *p, unsigned long __remote va)
{
	unsigned int idx;
	struct legomem_vregion *head;

	head = p->vregion;
	idx = va_to_vregion_index(va);
	return head + idx;
}

static inline unsigned int
legomem_vregion_to_index(struct legomem_context *p, struct legomem_vregion *v)
{
	struct legomem_vregion *head = p->vregion;
	unsigned int idx;

	idx = v - head;
	BUG_ON(idx >= NR_VREGIONS);
	return idx;
}

static inline struct legomem_vregion *
index_to_legomem_vregion(struct legomem_context *p, unsigned int index)
{
	BUG_ON(index >= NR_VREGIONS);
	return p->vregion + index;
}

/* ip is host order. */
static inline int __get_session_key(unsigned int ip,
				    unsigned int udp_port,
				    unsigned int ses_id,
				    unsigned int tid)
{
	return ip + udp_port + ses_id + tid;
}

static inline int get_session_key(struct session_net *ses)
{
	unsigned int ip, udp_port, ses_id, tid;

	/* Uniquely identify a remote party */
	ip = ses->board_ip;
	udp_port = ses->udp_port;

	ses_id = ses->session_id;
	tid = ses->tid;

	return __get_session_key(ip, udp_port, ses_id, tid);
}

/* Per-node context list */
int add_legomem_context(struct legomem_context *p);
int remove_legomem_context(struct legomem_context *p);
struct legomem_context *find_legomem_context(unsigned int pid);
void dump_legomem_contexts(void);

/* Per-context's session list */
void dump_legomem_context_sessions(struct legomem_context *p);
int context_add_session(struct legomem_context *p, struct session_net *ses);
int context_remove_session(struct legomem_context *p, struct session_net *ses);
struct session_net *
context_find_session(struct legomem_context *p, pid_t tid,
		     int board_ip, unsigned udp_port);
struct session_net *context_find_session_by_board(struct legomem_context *p,
						  pid_t tid,
						  struct board_info *bi);

struct board_info *add_board(char *board_name, unsigned long mem_total,
			     struct endpoint_info *remote_ei,
			     struct endpoint_info *local_ei,
			     bool is_local);
void remove_board(struct board_info *bi);
struct board_info *find_board(unsigned int ip, unsigned int port);
void dump_boards(void);

/* Per-node session list */
void dump_net_sessions(void);
struct session_net *alloc_session(void);
void free_session(struct session_net *ses);
struct session_net *find_net_session(unsigned int session_id);

/* init and utils */
extern char global_net_dev[32];
extern struct board_info *mgmt_dummy_board;
extern struct session_net *mgmt_session;
int ibdev2netdev(const char *ibdev, char *ndev, size_t ndev_buf_size);
int get_ip_str(unsigned int ip, char *ip_str);
int get_mac_of_remote_ip(int ip, char *ip_str, char *dev,
			 unsigned char *mac);

unsigned int get_device_mtu(const char *dev);
int get_interface_mac_and_ip(const char *dev, unsigned char *mac,
			     char *ip_str, int *ip);
int init_default_local_ei(const char *dev, unsigned int port,
			  struct endpoint_info *ei);
int init_local_management_session(void);
int init_monitor_session(char *ndev, char *monitor_addr,
			 struct endpoint_info *local_ei);

/* Host-side only */
extern unsigned int monitor_ip_h;
extern char monitor_ip_str[INET_ADDRSTRLEN];
extern struct endpoint_info monitor_ei;
extern struct session_net *monitor_session;
extern struct board_info *monitor_bi;

#define BOARD_HASH_ARRAY_BITS (5)
extern DECLARE_HASHTABLE(board_list, BOARD_HASH_ARRAY_BITS);
extern pthread_rwlock_t board_lock;

extern struct endpoint_info default_local_ei;
extern struct board_info *default_local_bi;
int add_localhost_bi(struct endpoint_info *ei);
#include "stat.h"

/*
 * for test
 */
int test_legomem_session(char *);
int test_legomem_migration(char *);
int test_legomem_board(char *);
int test_raw_net(char *);
int test_rel_net_mgmt(char *);
int test_rel_net_normal(char *);
int test_legomem_context(char *);
int test_legomem_alloc_free(char *);
int test_legomem_read_write(char *);
int test_legomem_soc(char *);
int test_pingpong_soc(char *board_ip_port_str);
int test_legomem_pte(char *board_ip_port_str);
int test_legomem_rw_seq(char *_unused);
int test_legomem_rw_fault(char *_unused);
int test_legomem_rw_inline(char *_unused);

int manually_add_new_node_str(const char *ip_port_str, unsigned int node_type);
int manually_add_new_node(unsigned int ip, unsigned int udp_port,
			  unsigned int node_type);

int pin_cpu(int cpu_id);
static inline void legomem_getcpu(int *cpu, int *node)
{
	syscall(SYS_getcpu, cpu, node, NULL);
}

/*
 * Common handlers
 */
void handle_pingpong(struct thpool_buffer *tb);
void handle_new_node(struct thpool_buffer *tb);
void handle_close_session(struct thpool_buffer *tb);
void handle_open_session(struct thpool_buffer *tb);

int create_watchdog_thread(void);

void *user_session_handler(void *_ses);

int get_ip_str(unsigned int ip, char *ip_str);

static inline void
dump_legomem_vregion(struct legomem_context *ctx, struct legomem_vregion *v)
{
	int i;
	unsigned int index;
	char ip_str[INET_ADDRSTRLEN];
	struct session_net *ses;

	index = legomem_vregion_to_index(ctx, v);

	get_ip_str(v->board_ip, ip_str);
	printf("Dump vRegion@[%#lx-%#lx] index=%u (%s:%u) avail=%uB flags=%#lx. Associated sessions:\n",
		(u64)(index * VREGION_SIZE), (u64)(index + 1) * VREGION_SIZE, index,
		ip_str, v->udp_port, atomic_load(&v->avail_space), v->flags);
	printf("    bkt      ses_local       tid\n");
	printf("    -------- --------------- --------\n");

	pthread_rwlock_rdlock(&v->rwlock);
	hash_for_each(v->ht_sessions, i, ses, ht_link_vregion) {
		printf("    %-8d %-15d %-8d\n", i, get_local_session_id(ses), ses->tid);
	}
	pthread_rwlock_unlock(&v->rwlock);
}

#endif /* _HOST_CORE_H_ */
