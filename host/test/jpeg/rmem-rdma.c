#include <stdio.h>
#include <stdlib.h>

#include "rmem.h"
#include "test.h"

#define RMEM_RDMA_WRID_READ  101
#define RMEM_RDMA_WRID_WRITE 102

// One RMEM maps to a single MR, fornow
struct remote_mem_rdma {
    struct remote_mem rmem;
    struct rdma_conn conn;
};


// Different kind of remote buffers will handle different MRs
// parameter: target memory
struct remote_mem * rinit(int access, size_t size, void *args) {
    struct remote_mem_rdma * rmem;

    // the target 
    char * url = (char *) args;

    // TODO: check ROCE here?
    rmem = (struct remote_mem_rdma *)calloc(1, sizeof(struct remote_mem_rdma));
    rmem->rmem.access = access;

    rmem->conn.port = config.client.port;
    if (config.use_roce)
        rmem->conn.gid = config.gid_idx;

    // This port is rnic port
    rmem->conn.port = config.client.port;

    // TODO: goto common
    create_context(&config.client, &rmem->conn);

    // Create PD
    rmem->conn.pd = ibv_alloc_pd(rmem->conn.context);
    if (rmem->conn.pd == NULL) {
        printf("create pd fail");
        return NULL;
    }

    // Create CQ
    rmem->conn.cq = ibv_create_cq(rmem->conn.context, config.cq_size, NULL, NULL, 0);

    // Create QP
    create_qp(&rmem->conn);

    // Exchange with server
    client_exchange_info(&rmem->conn, url);

    // Enable QP
    qp_stm_reset_to_init(&rmem->conn);
    qp_stm_init_to_rtr(&rmem->conn);
    qp_stm_rtr_to_rts(&rmem->conn);

    return (struct remote_mem *)rmem;
}

void * rcreatebuf (struct remote_mem * _rmem, size_t size) {
    int ret;
    struct remote_mem_rdma * rmem = (struct remote_mem_rdma *)_rmem;

    // We need a buffer at client side
    // TODO: access?
    ret = create_mr(&rmem->conn, size, IBV_ACCESS_LOCAL_WRITE);
    if (ret != 0) {
        return NULL;
    }

    return rmem->conn.mr[rmem->conn.num_mr - 1].addr;
}

int rread (struct remote_mem * _rmem, void *buf, uint64_t addr, size_t size) {
    int ret;
    struct ibv_wc wc;
    struct ibv_sge * sge;
    struct ibv_mr * mr;
    struct ibv_send_wr wr, *badwr = NULL;

    struct rdma_conn *conn = &((struct remote_mem_rdma *)_rmem)->conn;

    // TODO: multiple MR
    sge = (struct ibv_sge *)calloc(1, sizeof(struct ibv_sge));

    sge->addr = (uint64_t)conn->mr[0].addr;
    sge->length = size;
    sge->lkey = conn->mr[0].lkey;

    memset(&wr, 0, sizeof(wr));
    // user tag
    wr.wr_id = RMEM_RDMA_WRID_READ;
    wr.sg_list = sge;
    wr.num_sge = 1;

    wr.wr.rdma.remote_addr = (uint64_t)conn->peerinfo->mr[0].addr + addr;
    wr.wr.rdma.rkey = conn->peerinfo->mr[0].rkey;
    wr.opcode = IBV_WR_RDMA_READ;

    wr.send_flags = IBV_SEND_SIGNALED;
    wr.next = NULL;

    ibv_post_send(conn->qp, &wr, &badwr);

    // TODO: WHEN DO WE POLL?
    int need_poll = 1;
    while (need_poll) {
        while ((ret = ibv_poll_cq(conn->cq, 1, &wc)) == 0) {
            need_poll = 0;
        }
    }

    // TODO: free SGE?
    return 0;
}

int rwrite (struct remote_mem * rmem, void *buf, uint64_t addr, size_t size) {
    int ret;
    struct ibv_wc wc;
    struct ibv_sge* sge;
    struct ibv_send_wr wr, *badwr = NULL;

    struct rdma_conn *conn = &((struct remote_mem_rdma *)rmem)->conn;

    sge = (struct ibv_sge *)calloc(1, sizeof(struct ibv_sge));

    sge->addr = (uint64_t)conn->mr[0].addr;
    sge->length = size;
    sge->lkey = conn->mr[0].lkey;

    memset(&wr, 0, sizeof(wr));
    // user tag
    wr.wr_id = RMEM_RDMA_WRID_WRITE;
    wr.sg_list = sge;
    wr.num_sge = 1;

    wr.wr.rdma.remote_addr = (uint64_t)conn->peerinfo->mr[0].addr + addr;
    wr.wr.rdma.rkey = conn->peerinfo->mr[0].rkey;
    wr.opcode = IBV_WR_RDMA_WRITE;

    wr.send_flags = IBV_SEND_SIGNALED;
    wr.next = NULL;

    ibv_post_send(conn->qp, &wr, &badwr);

    // TODO: WHEN DO WE POLL?
    int need_poll = 1;
    while (need_poll) {
        while ((ret = ibv_poll_cq(conn->cq, 1, &wc)) == 0) {
            need_poll = 0;
        }
    }

    // TODO: free SGE?
    return 0;
}

// currently, we do not support ralloc
int ralloc (struct remote_mem * rmem, void *buf, uint64_t addr, size_t size) {
    fprintf(stderr, "ralloc is not supportted on RMEM-RDMA\n");
    return -1;
}
