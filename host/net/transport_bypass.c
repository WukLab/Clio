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
#include "../core.h"

static inline int bypass_send_one(struct session_net *net,
				  void *buf, size_t buf_size, void *route)
{
	return raw_net_send(net, buf, buf_size, route);
}

static inline int bypass_receive_one(struct session_net *net,
				  void *buf, size_t buf_size)
{
	return raw_net_receive(buf, buf_size);
}

static inline int bypass_receive_one_zerocopy(struct session_net *net,
				  void **buf, size_t *buf_size)
{
	return raw_net_receive_zerocopy(buf, buf_size);
}

static inline int bypass_receive_one_zerocopy_nb(struct session_net *net,
				  void **buf, size_t *buf_size)
{
	return raw_net_receive_zerocopy(buf, buf_size);
}

static inline int bypass_receive_one_nb(struct session_net *net,
				  void *buf, size_t buf_size)
{
	return raw_net_receive_nb(buf, buf_size);
}

static inline int
bypass_open_session(struct session_net *ses, struct endpoint_info *local_ei,
		    struct endpoint_info *remote_ei)
{
	return 0;
}

static inline int
bypass_close_session(struct session_net *ses)
{
	return 0;
}

static inline int bypass_init_once(struct endpoint_info *local_ei)
{
	return 0;
}

static inline void bypass_exit(void)
{
	return;
}

struct transport_net_ops transport_bypass_ops = {
	.name			= "transport_bypass",

	.init_once		= bypass_init_once,
	.exit			= bypass_exit,

	.open_session		= bypass_open_session,
	.close_session		= bypass_close_session,

	.send_one		= bypass_send_one,
	.receive_one		= bypass_receive_one,
	.receive_one_nb		= bypass_receive_one_nb,
	.receive_one_zerocopy	= bypass_receive_one_zerocopy,
	.receive_one_zerocopy_nb= bypass_receive_one_zerocopy_nb,

	.reg_send_buf		= default_transport_reg_send_buf,
};
