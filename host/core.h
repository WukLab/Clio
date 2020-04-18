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

#define LEGOMEM_VREGION_ALLOCATED (0x1u)

/*
 * pthread_rwlock_t is a big structure, around 50-60B.
 * Thus it's impossible to tame whole structure with one cacheline.
 * With hash entries being 6, we almost fully occupy two full lines.
 */
struct legomem_vregion {
	int board_ip;
	unsigned int udp_port;
	unsigned int flags;
	atomic_int avail_space;

#define LEGOMEM_VREGION_HT_ENTRIES	(6)
	struct hlist_head ht_sessions[LEGOMEM_VREGION_HT_ENTRIES];
	pthread_rwlock_t rwlock;
} __aligned(64);

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

/* Frequently used, sitting on datapath */
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

int get_ip_str(unsigned int ip, char *ip_str);
static inline void dump_legomem_vregion(struct legomem_vregion *v)
{
	int i;
	char ip_str[INET_ADDRSTRLEN];
	struct session_net *ses;

	get_ip_str(v->board_ip, ip_str);
	printf("vRegion (board %s:%u) avail_space: %u B flags: %#x\n",
		ip_str, v->udp_port, atomic_load(&v->avail_space), v->flags);

	printf("     bkt       ses_local       tid\n");
	printf("-------- ---------------  --------\n");
	pthread_rwlock_rdlock(&v->rwlock);
	hash_for_each(v->ht_sessions, i, ses, ht_link_vregion) {
		printf("%8d %15d %8d\n", i, get_local_session_id(ses), ses->tid);
	}
	pthread_rwlock_unlock(&v->rwlock);
}

static inline void init_legomem_vregion(struct legomem_vregion *v)
{
	atomic_store(&v->avail_space, VREGION_SIZE);
	pthread_rwlock_init(&v->rwlock, NULL);
}

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
	struct legomem_vregion *cached_vregion;
	struct list_head open_vregion;
} __aligned(64);

static inline void init_legomem_context(struct legomem_context *p)
{
	int i;

	BUG_ON(!p);

	memset(p, 0, sizeof(*p));
	INIT_HLIST_NODE(&p->link);
	pthread_spin_init(&p->lock, PTHREAD_PROCESS_PRIVATE);

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
int init_context_subsys(void);
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

/* Board */
#define ANY_BOARD	(UINT_MAX)
int init_board_subsys(void);
struct board_info *add_board(char *board_name, unsigned long mem_total,
			     struct endpoint_info *remote_ei,
			     struct endpoint_info *local_ei,
			     bool is_local);
void remove_board(struct board_info *bi);
struct board_info *find_board(unsigned int ip, unsigned int port);
void dump_boards(void);

/* Per-node session list */
int init_net_session_subsys(void);
void dump_net_sessions(void);
struct session_net *alloc_session(void);
void free_session(struct session_net *ses);
struct session_net *find_net_session(unsigned int session_id);

/*
 * LegoMem Public APIs
 */
struct legomem_context *legomem_open_context(void);
int legomem_close_context(struct legomem_context *ctx);
struct session_net *legomem_open_session(struct legomem_context *ctx, struct board_info *bi);
struct session_net *generic_handle_open_session(struct board_info *bi, unsigned int dst_sesid);
int generic_handle_close_session(struct legomem_context *ctx,
				 struct board_info *bi,
				 struct session_net *ses);
struct session_net *
legomem_open_session_remote_mgmt(struct board_info *bi);
struct session_net *
legomem_open_session_local_mgmt(struct board_info *bi);
int legomem_close_session(struct legomem_context *ctx, struct session_net *ses);
unsigned long __remote
legomem_alloc(struct legomem_context *ctx, size_t size, unsigned long vm_flags);
int legomem_free(struct legomem_context *ctx,
		 unsigned long __remote addr, size_t size);

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

/* Host-side only */
extern unsigned int monitor_ip_h;
extern char monitor_ip_str[INET_ADDRSTRLEN];
extern struct endpoint_info monitor_ei;
extern struct session_net *monitor_session;
extern struct board_info *monitor_bi;

#define BOARD_HASH_ARRAY_BITS (5)
extern DECLARE_HASHTABLE(board_list, BOARD_HASH_ARRAY_BITS);
extern pthread_spinlock_t board_lock;

extern struct endpoint_info default_local_ei;
extern struct board_info *default_local_bi;
int add_localhost_bi(struct endpoint_info *ei);
#include "stat.h"

/* Debugging info, useful for dev */
#if 1
#define dprintf_DEBUG(fmt, ...) \
	printf("\033[34m[%s:%d] " fmt "\033[0m", __func__, __LINE__, __VA_ARGS__)
#else
#define dprintf_DEBUG(fmt, ...)  do { } while (0)
#endif

/* General info, always on */
#define dprintf_INFO(fmt, ...) \
	printf("\033[34m[%s:%d] " fmt "\033[0m", __func__, __LINE__, __VA_ARGS__)

/* ERROR/WARNING info, always on */
#define dprintf_ERROR(fmt, ...) \
	printf("\033[31m[%s:%d] " fmt "\033[0m", __func__, __LINE__, __VA_ARGS__)

/*
 * for test
 */
int test_legomem_session(void);
int test_legomem_migration(void);
int test_legomem_board(char *);
int test_raw_net(char *);
int test_rel_net_mgmt(void);
int test_rel_net_normal(char *);
int test_legomem_context(void);
int test_legomem_alloc_free(void);

int manually_add_new_node_str(const char *ip_port_str, unsigned int node_type);
int manually_add_new_node(unsigned int ip, unsigned int udp_port,
			  unsigned int node_type);

static inline void getcpu(int *cpu, int *node)
{
	syscall(SYS_getcpu, cpu, node, NULL);
}

/*
 * Common handlers
 */
void handle_pingpong(struct thpool_buffer *tb);

#endif /* _HOST_CORE_H_ */
