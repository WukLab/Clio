/*
 * Copyright (c) 2020. Wuklab. All rights reserved.
 */

#ifndef _LEGOPGA_BOARD_SOC_CORE_H_
#define _LEGOPGA_BOARD_SOC_CORE_H_

#include <uapi/sched.h>
#include <uapi/thpool.h>
#include <fpga/zynq_regmap.h>
#include <fpga/lego_mem_ctrl.h>
#include "external.h"
#include "buddy.h"

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

extern int devmem_fd;

/* VM */
unsigned long alloc_va_vregion(struct proc_info *proc, struct vregion_info *vi,
			       unsigned long len, unsigned long vm_flags, int *nr_retry);
int free_va_vregion(struct proc_info *proc, struct vregion_info *vi,
	    unsigned long start, unsigned long len);
unsigned long alloc_va(struct proc_info *proc, unsigned long len,
		       unsigned long vm_flags, int *p_nr_retry);
int free_va(struct proc_info *proc, unsigned long start, unsigned long len);
int __free_va(struct proc_info *proc, struct vregion_info *vi,
	      unsigned long start, unsigned long len);

/*
 * SCHED APIs
 *
 * 1. alloc_proc: create a new proc and insert into hashtable
 * 2. free_proc: free the proc and delete from hashtable
 * 3. get_proc_by_pid: find the proc, and then increase refcount
 * 4. get_proc_info: increase refcount
 * 5. put_proc_info: decrese refcount
 *
 * Developers should use get_proc_by_pid, and put_proc_info in a pair
 * whenever you are using the proc_info. Never use free_proc directly.
 */
struct proc_info *alloc_proc(unsigned int pid,
			     char *proc_name, unsigned int host_ip);
void free_proc(struct proc_info *pi);
struct proc_info *get_proc_by_pid(unsigned int pid);
void dump_proc(struct proc_info *pi);
void dump_procs(void);

/* Session */
struct session_net *alloc_session(void);
void free_session_by_id(unsigned int id);
void dump_net_sessions(void);

/* Buddy */
int init_buddy(void);
struct page *alloc_page(unsigned int order);
void free_page(struct page *page, unsigned int order);
unsigned long alloc_pfn(unsigned int order);
void free_pfn(unsigned long pfn, unsigned int order);

int pin_cpu(int cpu_id);
void legomem_getcpu(int *cpu, int *node);
int parse_ip_str(const char *ip_str);

int init_shadow_pgtable(void);
int init_freepage_fifo(void);
void init_stat_mapping(void);
void init_migration_setup(void);
void init_tlbflush_setup(void);
int init_ctrl_polling(void);

void handle_test_pte(struct thpool_buffer *tb);
unsigned long __handle_ctrl_alloc(struct proc_info *pi, size_t size, unsigned long vm_flags);

void tlb_flush(struct proc_info *pi, unsigned long va, unsigned long size);

/* Test */
void test_buddy(void);
void test_vm(void);
void test_pgtable_access(void);
void test_clear_page(void);

/* board.c */
struct board_info *add_board(char *board_name, int board_ip, int udp_port);
struct board_info *find_board(int ip, unsigned int port);
void dump_boards(void);

void handle_ctrl_freepage_ack(struct lego_mem_ctrl *rx,
			      struct lego_mem_ctrl *tx);


void test_vm_conflict(void);

extern unsigned long nr_allocated_ptes;
void dump_shadow_pgtable_util(void);

unsigned long vma_tree_new(struct proc_info *proc, struct vregion_info *vi,
			   unsigned long addr, unsigned long len,
			   unsigned long vm_flags);
#endif /* _LEGOPGA_BOARD_SOC_CORE_H_ */
