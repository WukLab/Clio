#include "test.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct config config;

int parse_config(int argc, char *argv[]) {
    // fill in config here
    memset(&config, 0, sizeof(struct config));

    config.cq_size = 16;
    // for ib servers, use 0
    config.use_roce = 0;
    config.gid_idx = 0;

    config.client_mr_size = 1024 * 1024 * 64;
    // config.server_num_mr = 1024 * 512;
    // config.server_mr_size = 4096 * 8;

    config.server_num_mr = 16;
    config.server_mr_size = (size_t)1024 * 1024 * 1024;

    // set ib info
    config.server.num_devices = 2;
    config.server.port = 1;
    config.server.device_name = "mlx5_1";

    config.client.num_devices = 2;
    config.client.port = 1;
    config.client.device_name = "mlx5_1";

    // test parameters
    config.request_size = 64;
    config.server_url = "tcp://wuklab-01.ucsd.edu:2345";
    config.server_listen_url = "tcp://*:2345";

    config.server_enable_odp = 0;
    config.server_multi_conn = 1;

    // application
    config.server_rdma_read_url = "tcp://wuklab-01.ucsd.edu:5300";
    config.server_rdma_write_url = "tcp://wuklab-01.ucsd.edu:5301";

    config.array_cell_size = 1024 * 1024;
    config.jpg_size = 20016;

    // parse arg
    int opt;
    while ((opt = getopt(argc, argv, "m:s:p:l:")) != -1) {
        switch(opt) {
            case 'l':
                config.server_listen_url = optarg;
                printf("set server_listen_url = %s\n", config.server_listen_url);
                break;
            case 'm':
                config.server_num_mr = atoi(optarg);
                printf("set server_num_mr = %d\n", config.server_num_mr);
                break;
            case 's':
                config.server_mr_size = atoi(optarg);
                printf("set server_mr_size = %lu\n", config.server_mr_size);
                break;
            case 'p':
                config.program = optarg;
                printf("set program = %s\n", config.program);
                break;
            default:
                printf("read config.c for ali opts.\n");
                return -1;
        }
    }


    return 0;
}
