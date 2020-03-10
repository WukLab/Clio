#ifndef _HOST_CORE_H_
#define _HOST_CORE_H_

#include <uapi/sched.h>

struct legomem_vregion {
	int board_id;
	int avail_space;
	struct session_net *ses_net;
};

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
};

static inline void init_legomem_context(struct legomem_context *p)
{
	BUG_ON(!p);

	memset(p, 0, sizeof(*p));
	INIT_LIST_HEAD(&p->list);
	pthread_spin_init(&p->lock, PTHREAD_PROCESS_PRIVATE);
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

static inline int get_session_key(unsigned int ip, unsigned int session_id)
{
	return ip + session_id;
}

/* Per-node context list */
int init_context_subsys(void);
int add_legomem_context(struct legomem_context *p);
int remove_legomem_context(struct legomem_context *p);
void dump_legomem_context(void);
int context_add_session(struct legomem_context *p, struct session_net *ses);
int context_remove_session(struct legomem_context *p, struct session_net *ses);
struct session_net *context_find_session_by_ip(struct legomem_context *p,
					       unsigned int board_ip);
struct session_net *context_find_session_by_board(struct legomem_context *p,
						  struct board_info *bi);

/* Board */
int init_board_subsys(void);
struct board_info *add_board(char *board_name, unsigned long mem_total,
			     struct endpoint_info *remote_ei,
			     struct endpoint_info *local_ei);
void remove_board(struct board_info *bi);
void dump_boards(void);

/* Per-node session list */
int init_net_session_subsys(void);
void dump_net_sessions(void);
int add_net_session(struct session_net *ses);
int remove_net_session(struct session_net *ses);
struct session_net *
find_net_session(unsigned int board_ip, unsigned int session_id);

/*
 * LegoMem Public APIs
 */
struct legomem_context *legomem_open_context(void);
int legomem_close_context(struct legomem_context *ctx);
struct session_net *legomem_open_session(struct legomem_context *ctx, struct board_info *bi);
int legomem_close_session(struct legomem_context *ctx, struct session_net *ses);

#endif /* _HOST_CORE_H_ */
