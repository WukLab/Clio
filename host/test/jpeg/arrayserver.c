#include <infiniband/verbs.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include <unistd.h>

static struct rdma_conn static_conn;

int server() {

    struct rdma_conn conn;
    memset(&conn, 0, sizeof(struct rdma_conn));
    conn.port = config.server.port;

    // TODO: goto common
    create_context(&config.server, &conn);

    // Create PD
    conn.pd = ibv_alloc_pd(conn.context);
    if (conn.pd == NULL) {
        printf("iDIE");
        return -1;
    }

    // Create CQ
    conn.cq = ibv_create_cq(conn.context, config.cq_size, NULL, NULL, 0);

    // Create QP
    create_qp(&conn);

    // create MRs
    int access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
    if (config.server_enable_odp)
        access |= IBV_ACCESS_ON_DEMAND;
    for (int i = 0; i < config.server_num_mr; i++)
        create_mr(&conn, config.server_mr_size, access);

    // DO Extra job
    if (config.program != NULL) {


        if (strcmp("array-jpg", config.program) == 0) {
            printf("Do server job %s ...\n", config.program);
            int jpg_size = config.jpg_size;

            int cells = config.server_mr_size / config.array_cell_size;

            void * jpg = malloc(config.array_cell_size);
            char * buf = conn.mr[0].addr;

            FILE *jpg_file = fopen("./data/cat.jpg", "r");
            fread(jpg, 1, jpg_size, jpg_file);
            fclose(jpg_file);

            for (int i = 0; i < cells; i++) {
                memcpy(buf, jpg, jpg_size);
                buf += config.array_cell_size;
            }
        }
    }

    // Exchange with server
    server_exchange_info(&conn, config.server_listen_url);

    // Enable QP, server only need to get to RTR
    qp_stm_reset_to_init(&conn);
    qp_stm_init_to_rtr(&conn);

    return 0;
}

int main(int argc, char *argv[]) {
    parse_config(argc, argv);
    int num_server = 0;
    do {
        server();
        printf("Finish server %d setup.\n", num_server++);
    } while (config.server_multi_conn);

    // Serve here
    for (;;) ;
    return 0;
}
