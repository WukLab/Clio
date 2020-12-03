#ifndef _TEST_H_
#define _TEST_H_

#include <infiniband/verbs.h>

// For gid
#define RDMA_PROTOCOL_IB (0)

struct rdma_conn {
    struct ibv_context* context;
    struct ibv_pd* pd;
    struct ibv_cq* cq;
    struct ibv_qp* qp;

    int num_mr;
    struct ibv_mr* mr;

    int port;
    int gid;
    struct conn_info* peerinfo;
};

struct conn_info {
    int port;
    uint32_t local_id;
    uint16_t qp_number;

    int num_mr;
    union ibv_gid gid;
    struct ibv_mr mr[0];
};

struct epinfo {
    int num_devices;
    int port;
    char * device_name;
};

struct config {
    int cq_size;

    // gloabl config
    int use_roce;
    int gid_idx;

    // Memory region
    size_t client_mr_size;

    int server_num_mr;
    size_t server_mr_size;

    int share_mr;

    struct epinfo server, client;

    // test
    int request_size;
    char * server_url;
    char * server_listen_url;

    int server_enable_odp;
    int server_multi_conn;

    char * program;

    // application test
    size_t array_cell_size;

    char *server_rdma_read_url;
    char *server_rdma_write_url;

    int jpg_size;
    int legomem;
};

extern struct config config;

// network exchange info
int client_exchange_info(struct rdma_conn *conn, char * url);
int server_exchange_info(struct rdma_conn *conn, char * url);

// test_config
int parse_config(int argc, char *argv[]);

// common helper for rdma, may move to common.h later
int create_context(struct epinfo *ep, struct rdma_conn *conn);
int create_qp(struct rdma_conn *conn);
int create_mr(struct rdma_conn *conn, size_t size, int access);
//    QP state mahine functions
int qp_stm_reset_to_init(struct rdma_conn *conn);
int qp_stm_init_to_rtr(struct rdma_conn *conn);
int qp_stm_rtr_to_rts(struct rdma_conn *conn);
// network communication
int extract_info(struct rdma_conn *conn, void **buf);

#endif
