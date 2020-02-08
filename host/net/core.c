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

int net_send(void *buf, size_t buf_size)
{

}

int net_receive(void *buf, size_t buf_size)
{

}

void net_send_and_receive(void *send_buf, size_t send_buf_size,
			  void *recv_buf, size_t recv_buf_size)
{

}

int init_net(void)
{
	/* XXX more automatic or use config file */
	struct endpoint_info ei_wuklab02 = {
		.mac		= { 0xe4, 0x1d, 0x2d, 0xb2, 0xba, 0x51 },
		.ip		= 0xc0a80102, /* 192.168.1.2 */
		.udp_port	= 8888,
	};
	struct endpoint_info ei_wuklab05 = {
		.mac		= { 0xe4, 0x1d, 0x2d, 0xe4, 0x81, 0x51 },
		.ip		= 0xc0a80105, /* 192.168.1.2 */
		.udp_port	= 8888,
	};
	struct endpoint_info board_0 = {
		.mac		= { 0xe4, 0x1d, 0x2d, 0x88, 0x77, 0x51 },
		.ip		= 0xc0a801c8, /* 192.168.1.200 */
		.udp_port	= 1234,
	};

	struct endpoint_info *local_ei, *remote_ei;
	local_ei	= &ei_wuklab02;
	remote_ei	= &board_0;

	struct session_net *ses;

	ses = init_ib_raw_packet(local_ei, remote_ei);
	if (!ses) {
		printf("Fail to create net session\n");
		exit(1);
	}
	printf("%#lx\n", ses);
	test_ib_raw_packet(ses);

	return 0;
}
