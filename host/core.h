#ifndef _HOST_CORE_H_
#define _HOST_CORE_H_

#include <uapi/sched.h>
#include "net/net.h"
#include <limits.h>

struct legomem_vregion {
	int board_id;
	int avail_space;
	struct session_net *ses_net;
};

static inline void
set_vregion_session(struct legomem_vregion *v, struct session_net *ses)
{
	v->ses_net = ses;
}

static inline struct session_net *
get_vregion_session(struct legomem_vregion *v)
{
	return v->ses_net;
}

#define LEGOMEM_CONTEXT_FLAGS_MGMT	0x1

struct legomem_context {
	unsigned long flags;
	unsigned int pid;

	/* List all contexts */
	struct list_head list;

	/*
	 * List of all open sessions of this context.
	 * Served as a cache for future vRegion additions.
	 */
	struct hlist_head ht_sessions[4];
	pthread_spinlock_t lock;

	struct legomem_vregion vregion[NR_VREGIONS];
	struct legomem_vregion *cached_vregion;
	struct list_head open_vregion;
};

static inline void init_legomem_context(struct legomem_context *p)
{
	int i;

	BUG_ON(!p);

	memset(p, 0, sizeof(*p));
	INIT_LIST_HEAD(&p->list);
	pthread_spin_init(&p->lock, PTHREAD_PROCESS_PRIVATE);

	/* init all vregions */
	for (i = 0; i < NR_VREGIONS; i++) {
		struct legomem_vregion *v;

		v = p->vregion + i;
		v->avail_space = VREGION_SIZE;
		v->ses_net = NULL;
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
void dump_legomem_context(void);
int context_add_session(struct legomem_context *p, struct session_net *ses);
int context_remove_session(struct legomem_context *p, struct session_net *ses);
struct session_net *context_find_session_by_ip(struct legomem_context *p,
					       pid_t tid,
					       unsigned int board_ip);
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
int add_net_session(struct session_net *ses);
int remove_net_session(struct session_net *ses);
struct session_net *
find_net_session(unsigned int board_ip, unsigned int udp_port, unsigned int session_id);

int alloc_session_id(void);
void free_session_id(unsigned int session_id);

/*
 * LegoMem Public APIs
 */
struct legomem_context *legomem_open_context(void);
struct legomem_context *legomem_open_context_mgmt(void);
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

/* init and utils */
extern struct board_info *mgmt_dummy_board;
extern struct session_net *mgmt_session;
int get_ip_str(unsigned int ip, char *ip_str);
int get_mac_of_remote_ip(unsigned int ip, char *ip_str, unsigned char *mac);
int get_interface_mac_and_ip(const char *dev, unsigned char *mac,
			     char *ip_str, unsigned int *ip);
int init_default_local_ei(const char *dev, unsigned int port,
			  struct endpoint_info *ei);
int init_local_management_session(bool);

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

/* Debugging info, useful for dev */
#define dprintf_DEBUG(fmt, ...) \
	printf("\033[34m[%s:%d] " fmt "\033[0m", __func__, __LINE__, __VA_ARGS__)

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

#endif /* _HOST_CORE_H_ */
