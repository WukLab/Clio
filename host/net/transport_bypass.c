/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 *
 * Dummy transport layer that uses raw net directly.
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

static inline int bypass_send_one(struct session_net *net,
				  void *buf, size_t buf_size)
{
	return raw_net_send(net, buf, buf_size);
}

static inline int bypass_receive_one(struct session_net *net,
				  void *buf, size_t buf_size)
{
	return raw_net_receive(net, buf, buf_size);
}

static inline int bypass_receive_one_nb(struct session_net *net,
				  void *buf, size_t buf_size)
{
	return raw_net_receive_nb(net, buf, buf_size);
}

static inline int bypass_init(struct session_net *ses)
{
	return 0;
}

struct transport_net_ops transport_bypass_ops = {
	.name			= "transport_bypass",
	.send_one		= bypass_send_one,
	.receive_one		= bypass_receive_one,
	.receive_one_nb		= bypass_receive_one_nb,
	.init			= bypass_init,
};
