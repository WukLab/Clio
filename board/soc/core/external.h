#ifndef _LEGOPGA_BOARD_SOC_EXTERNAL_H_
#define _LEGOPGA_BOARD_SOC_EXTERNAL_H_

#include <uapi/sched.h>
#include <uapi/thpool.h>

void board_soc_handle_alloc_free(struct thpool_buffer *tb, bool is_alloc);
void board_soc_handle_migration_send(struct thpool_buffer *tb);
void board_soc_handle_migration_recv(struct thpool_buffer *tb);
void board_soc_handle_migration_recv_cancel(struct thpool_buffer *tb);

#endif /* _LEGOPGA_BOARD_SOC_EXTERNAL_H_ */
