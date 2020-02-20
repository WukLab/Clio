/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Reliable Go-Back-N Transport Layer.
 */
#include <infiniband/verbs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <uapi/list.h>
#include <uapi/err.h>
#include <uapi/net_header.h>

#include "net.h"

static inline int gbn_send_one(struct session_net *net,
				  void *buf, size_t buf_size)
{
	return raw_net_send(net, buf, buf_size);
}

static inline int gbn_receive_one(struct session_net *net,
				  void *buf, size_t buf_size)
{
	return raw_net_receive(net, buf, buf_size);
}

static inline int gbn_receive_one_nb(struct session_net *net,
				  void *buf, size_t buf_size)
{
	return raw_net_receive_nb(net, buf, buf_size);
}

static inline int gbn_init(void)
{
	return 0;
}

struct transport_net_ops transport_gbn_ops = {
	.name			= "transport_rel_gbn",
	.send_one		= gbn_send_one,
	.receive_one		= gbn_receive_one,
	.receive_one_nb		= gbn_receive_one_nb,
	.init			= gbn_init,
};

