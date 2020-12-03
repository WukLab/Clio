#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <infiniband/verbs.h>
#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>

#include "test.h"

static int sock_init = 0;
static nng_socket sock;

static void fatal(const char *func)
{
    int rv = 0;
    fprintf(stderr, "%s: %s\n", func, nng_strerror(rv));
    exit(1);
}

int client_exchange_info(struct rdma_conn *conn, char * url) {
    nng_socket sock;
    int rv;
    int send_size, bytes;
    size_t recv_size;
    void *info, *peerinfo = NULL;
    int size = 1024*1024*64;


    printf("connecting to server %s...\n", url);
    if ((rv = nng_req0_open(&sock)) < 0) {
        fatal("nn_socket");
    }

    // for large mr list!
    if ((rv = nng_setopt_size(sock, NNG_OPT_RECVMAXSZ, size)) != 0) {
        fatal("nng_setopt_size");
    }

    if ((rv = nng_dial(sock, url, NULL, 0)) < 0) {
        fatal("nn_connect");
    }

    send_size = extract_info(conn, &info);
    printf("try to send size %d\n", send_size);
    if ((rv = nng_send(sock, info, send_size, 0)) < 0) {
        fatal("nn_send");
    }
    if ((rv = nng_recv(sock, &peerinfo, &recv_size, NNG_FLAG_ALLOC)) < 0) {
        fatal("nn_recv");
    }

    conn->peerinfo = peerinfo;
    printf("received mr, with bytes %d, num %d\n", bytes, conn->peerinfo->num_mr);

    // TODO: check this
    // nn_freemsg(buf);

    free(info);

    return 0;
}

int server_exchange_info(struct rdma_conn *conn, char * url) {
    int send_size, rv;
    size_t recv_size;
    void *info, *peerinfo = NULL;

    // create server
    if (!sock_init) {
        if ((rv = nng_rep0_open(&sock)) < 0) {
            fatal("nn_socket");
        }

        if ((rv = nng_listen(sock, url, NULL, 0)) < 0) {
            fatal("nn_bind");
        }

        sock_init = 1;
    }

    printf("listening on server %s...\n", url);
    if ((rv = nng_recv(sock, &peerinfo, &recv_size, NNG_FLAG_ALLOC)) < 0) {
        fatal("nn_recv");
    }

    // TODO: fix this, use memcpy
    conn->peerinfo = peerinfo;
    send_size = extract_info(conn, &info);
    if ((rv = nng_send(sock, info, send_size, 0)) < 0) {
        fatal("nn_send");
    }
    printf("send %d bytes, expect to send %d bytes \n", rv, send_size);

    // TODO: check this
    // nn_freemsg(buf);
    
    free(info);

    return 0;
}

