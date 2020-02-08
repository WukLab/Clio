#ifndef _HOST_NET_NET_H_
#define _HOST_NET_NET_H_

#include <uapi/net_header.h>

struct session_net {
	struct endpoint_info local_ei, remote_ei;
	struct routing_info route;
	void *transport_private;
};

int init_net(void);

struct session_net *init_ib_raw_packet(struct endpoint_info *local_ei,
				       struct endpoint_info *remote_ei);

void test_ib_raw_packet(struct session_net *ses_net);

#endif
