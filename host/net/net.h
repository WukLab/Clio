/*
 * Copyright (c) 2020 Wuklab, UCSD. All rights reserved.
 */
#ifndef _HOST_NET_NET_H_
#define _HOST_NET_NET_H_

#include <errno.h>
#include <stdio.h>
#include <uapi/hashtable.h>
#include <uapi/net_header.h>
#include <uapi/net_session.h>

extern int sysctl_link_mtu;

int init_net(struct endpoint_info *local_ei);
void dump_packet_headers(void *packet, char *str_buf);

struct session_net *net_open_session(struct endpoint_info *local_ei,
				     struct endpoint_info *remote_ei);
int net_close_session(struct session_net *ses);

#endif /* _HOST_NET_NET_H_ */
