#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <uapi/gbn.h>
#include <uapi/net_header.h>
#include <uapi/opcode.h>

#define FPGA_PORT 1234
#define PACKET_SIZE 16
#define SRC_SESSION_ID 3
#define DEST_SESSION_ID 3

extern char *optarg;

int gbn_connect(int sockfd, struct sockaddr *addr, unsigned src_sesid)
{
	int ret;
	struct legomem_open_close_session_req open_req;
	open_req.comm_headers.gbn.seqnum = 0;
	set_gbn_src_dst_session(&open_req.comm_headers.gbn, 2, 0);
	open_req.comm_headers.gbn.type = GBN_PKT_DATA;
	open_req.comm_headers.lego.opcode = OP_OPEN_SESSION;
	open_req.comm_headers.lego.pid = 0;
	open_req.op.session_id = src_sesid;

	ret = sendto(sockfd, &open_req.comm_headers.gbn,
		     sizeof(open_req) - GBN_HEADER_OFFSET, 0, addr,
		     sizeof(struct sockaddr));
	return ret;
}

int gbn_close(int sockfd, struct sockaddr *addr, unsigned dest_sesid)
{
	int ret;
	struct legomem_open_close_session_req close_req;
	close_req.comm_headers.gbn.seqnum = 0;
	set_gbn_src_dst_session(&close_req.comm_headers.gbn, 2, 0);
	close_req.comm_headers.gbn.type = GBN_PKT_DATA;
	close_req.comm_headers.lego.opcode = OP_CLOSE_SESSION;
	close_req.comm_headers.lego.pid = 0;
	close_req.op.session_id = dest_sesid;
	ret = sendto(sockfd, &close_req.comm_headers.gbn,
		     sizeof(close_req) - GBN_HEADER_OFFSET, 0, addr,
		     sizeof(struct sockaddr));
	return ret;
}

int main(int argc, char *argv[])
{
	int socketfd;
	struct sockaddr_in host_addr;

	unsigned int seqnum;
	unsigned long buf[PACKET_SIZE];
	int send_size;
	int operation_switch;

	struct gbn_header *header;

	/*
	 * usage:
	 * -c: initiate gbn connection
	 * -d i: send payload with seq# i
	 * -x: close gbn connection 
	 */
	int ch;
	while ((ch = getopt(argc, argv, "cxd:")) != -1) {
		switch (ch) {
		case 'c':
			operation_switch = 1;
			break;
		case 'x':
			operation_switch = 2;
			break;
		case 'd':
			operation_switch = 3;
			seqnum = atoi(optarg);
			break;
		default:
			printf("error args\n");
			exit(EXIT_FAILURE);
		}
	}

	if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket error");
		exit(EXIT_FAILURE);
	}

	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons(FPGA_PORT);
	host_addr.sin_addr.s_addr = inet_addr("192.168.1.128");

	switch (operation_switch) {
	case 1:
		if (gbn_connect(socketfd, (struct sockaddr *)&host_addr,
				DEST_SESSION_ID) < 0) {
			perror("gbn connect");
			exit(EXIT_FAILURE);
		}
		break;
	case 2:
		if (gbn_close(socketfd, (struct sockaddr *)&host_addr,
			      DEST_SESSION_ID) < 0) {
			perror("gbn close");
			exit(EXIT_FAILURE);
		}
		break;
	case 3:
		header = (struct gbn_header *)buf;
		header->type = GBN_PKT_DATA;
		header->seqnum = seqnum;
		set_gbn_src_dst_session(header, SRC_SESSION_ID, DEST_SESSION_ID);

		for (int i = 1; i < PACKET_SIZE; i++)
			buf[i] = i;
		for (int i = 0; i < PACKET_SIZE; i++) {
			printf("buf[%d]: %016lx\n", i, buf[i]);
		}

		send_size = sendto(socketfd, buf, sizeof(buf), 0,
				   (struct sockaddr *)&host_addr,
				   sizeof(struct sockaddr));

		if (send_size < 0) {
			perror("send error");
			exit(EXIT_FAILURE);
		}
		printf("send %d bytes\n", send_size);
		break;
	default:
		break;
	}

	close(socketfd);

	return 0;
}
