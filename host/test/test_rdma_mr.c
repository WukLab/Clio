/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#include <netinet/in.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

#include <infiniband/verbs.h>

#define NSEC_PER_SEC		(1000000000)
#define ARRAY_SIZE(x)		(sizeof(x) / sizeof((x)[0]))

int main(void)
{
        struct ibv_device **dev_list;
        struct ibv_device *ib_dev;
	struct ibv_mr *mr;
	int ib_port = 1;
	struct ibv_context *ib_context;
	struct ibv_pd *ib_pd;
        int ret;

        dev_list = ibv_get_device_list(NULL);
        if (!dev_list) {
                perror("Failed to get devices list");
                return -ENODEV;
        }

        ib_dev = dev_list[0];
        if (!ib_dev) {
                fprintf(stderr, "IB device not found\n");
                return -ENODEV;
        }

        printf("Using IB Device: %s \n", ibv_get_device_name(ib_dev)); 

        ib_context = ibv_open_device(ib_dev);
        if (!ib_context) {
                fprintf(stderr, "Couldn't get ib_context for %s\n",
                        ibv_get_device_name(ib_dev));
                return -ENODEV;
        }

        ib_pd = ibv_alloc_pd(ib_context);
        if (!ib_pd) {
                fprintf(stderr, "Couldn't allocate PD\n");
                return -ENOMEM;
        }

#define OneM (1024*1024)
	void *tbuf;
	size_t tsize;
	size_t size_array[] = {4*OneM, 16*OneM, 64*OneM, 256*OneM, 1024*OneM};
	double lat_alloc, lat_free;
	struct timespec s, e;

        //tbuf = malloc(1024*1024*1024);
        tbuf = mmap(0, 1024*1024*1024, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0); 
        if (tbuf == MAP_FAILED) {
                perror("mmap");
                printf("Fail to allocate memory %d\n", errno);
                exit(0);
        }
        if (!tbuf)
		exit(0);

        int j, i = 0, nr=1000;

        for (j = 0; j < ARRAY_SIZE(size_array); j++) {
                tsize = size_array[j];
                lat_alloc = 0;
                lat_free = 0;
                i = 0;

                while (i++ < nr) {
                        clock_gettime(CLOCK_MONOTONIC, &s);
                        //mr = ibv_reg_mr(ib_pd, tbuf, tsize, IBV_ACCESS_LOCAL_WRITE);
                        mr = ibv_reg_mr(ib_pd, tbuf, tsize, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_ON_DEMAND);
                        if (!mr) {
                                perror("reg mr:");
                                return errno;
                        }
                        clock_gettime(CLOCK_MONOTONIC, &e);
                        lat_alloc += (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) - (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

                        clock_gettime(CLOCK_MONOTONIC, &s);
                        ibv_dereg_mr(mr);
                        clock_gettime(CLOCK_MONOTONIC, &e);
                        lat_free += (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) - (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);
                }
                printf("size: %zu avg_alloc: %lf ns, avg_free: %lf\n",
                        tsize, lat_alloc/nr, lat_free/nr);
        }

        return 0;
}
