#include <infiniband/verbs.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "test.h"

static int client_simple_write(struct rdma_conn *conn);

int client() {

    struct rdma_conn conn;
    memset(&conn, 0, sizeof(struct rdma_conn));
    conn.port = config.client.port;

    // TODO: goto common
    create_context(&config.client, &conn);

    // Create PD
    conn.pd = ibv_alloc_pd(conn.context);
    if (conn.pd == NULL) {
        printf("create pd fail");
        return -1;
    }

    // Create CQ
    conn.cq = ibv_create_cq(conn.context, config.cq_size, NULL, NULL, 0);

    // Create QP
    create_qp(&conn);

    // Exchange with server
    client_exchange_info(&conn);

    // Enable QP
    qp_stm_reset_to_init(&conn);
    qp_stm_init_to_rtr(&conn);
    qp_stm_rtr_to_rts(&conn);

    // We need a buffer at client side
    create_mr(&conn, config.client_mr_size, IBV_ACCESS_LOCAL_WRITE);

    // client write..
    client_simple_write(&conn);

    // clean up
    for (int i = 0; i < conn.num_mr; i++) {
        ibv_dereg_mr(conn.mr + i);
    }
    ibv_close_device(conn.context);

    return 0;
}

static int client_simple_write(struct rdma_conn *conn){
    int ret;
    struct timespec tstart={0,0}, tend={0,0};
    struct ibv_wc wc;
    int bytes;

    // put tings into buffer
    memcpy(conn->mr[0].addr, "This is a message!xxxxxxxxxA", 16);

    // SGE for request, we use only 1
    struct ibv_sge* sge = (struct ibv_sge *)calloc(1, sizeof(struct ibv_sge));
    sge->addr = (uint64_t)conn->mr[0].addr;
    sge->length = config.request_size;
    sge->lkey = conn->mr[0].lkey;

    struct ibv_send_wr wr, *badwr = NULL;
    memset(&wr, 0, sizeof(wr));
    // user tag
    wr.wr_id = 233;
    wr.sg_list = sge;
    wr.num_sge = 1;

    wr.wr.rdma.remote_addr = (uint64_t)conn->peerinfo->mr[0].addr;
    wr.wr.rdma.rkey = conn->peerinfo->mr[0].rkey;
    wr.opcode = IBV_WR_RDMA_WRITE;

    wr.send_flags = IBV_SEND_SIGNALED;
    wr.next = NULL;

// PTE MISSES
// MR SIZE = 1G
// num_MR = 16

    if (strcmp(config.client_program, "lat_pte_miss") == 0) {
        int page_interval = 16;

        uint64_t num_pages = config.server_mr_size / 4096;
        uint64_t num_loops = config.server_num_mr * num_pages * 4;
        uint64_t per_region_loops = num_pages / page_interval;
        printf("accessing length %ld [%p]\n", conn->peerinfo->mr[0].length, conn->peerinfo->mr[0].addr);
        printf("trying to access on %lu pages, with total %lu accesses\n", num_pages, num_loops);

        for (uint64_t i = 0; i < num_loops; i++) {
            if (i % per_region_loops == 0) {
                int round = (int)(i / per_region_loops);
                int region = round % conn->peerinfo->num_mr;
                printf("start round %d, region %d\n", round, region);
                wr.wr.rdma.remote_addr = (uint64_t)conn->peerinfo->mr[region].addr;
                wr.wr.rdma.rkey = conn->peerinfo->mr[region].rkey;
            } else
                wr.wr.rdma.remote_addr += (uint64_t)4096 * page_interval;

            clock_gettime(CLOCK_MONOTONIC, &tstart);
            ibv_post_send(conn->qp, &wr, &badwr);

            // PULL for result...
            while ((bytes = ibv_poll_cq(conn->cq, 1, &wc)) == 0) {
                // sleep(1);
            }
            clock_gettime(CLOCK_MONOTONIC, &tend);
            uint64_t ns = (uint64_t) tend.tv_sec * 1000000000ULL -
                          (uint64_t) tstart.tv_sec * 1000000000ULL +
                          tend.tv_nsec - tstart.tv_nsec;

            if (bytes > 0 && wc.status == IBV_WC_SUCCESS) {
                printf("%lu\n", ns);
            }
        }
    }

    if (strcmp(config.client_program, "lat_mr_miss") == 0) {

        uint64_t num_send = 1024*256;
        for (uint64_t i = 0; i < num_send; i++) {
            int idx = num_send % conn->peerinfo->num_mr;
            wr.wr.rdma.remote_addr = (uint64_t)conn->peerinfo->mr[idx].addr;
            wr.wr.rdma.rkey = conn->peerinfo->mr[idx].rkey;

            clock_gettime(CLOCK_MONOTONIC, &tstart);
            ret = ibv_post_send(conn->qp, &wr, &badwr);
            if (ret != 0) {
                printf("POST SEND FAIL\n");
            }

            // PULL for result...
            while ((bytes = ibv_poll_cq(conn->cq, 1, &wc)) == 0) {
                // sleep(1);
            }
            clock_gettime(CLOCK_MONOTONIC, &tend);
            uint64_t ns = (uint64_t) tend.tv_sec * 1000000000ULL -
                          (uint64_t) tstart.tv_sec * 1000000000ULL +
                          tend.tv_nsec - tstart.tv_nsec;

            if (bytes > 0 && wc.status == IBV_WC_SUCCESS) {
                // printf("Write %d bytes, latency %lu ns\n", config.request_size, ns);
                printf("%lu\n", ns);
            }
            usleep(10);
        }
    }
    
    free(sge);
}

int main(int argc, char *argv[]) {
    parse_config(argc, argv);
    return client();
}
