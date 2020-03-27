#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fpga/rel_net.h>
#include <uapi/net_header.h>

#define FPGA_PORT 1234
#define PACKET_SIZE 16
#define SRC_SESSION_ID 20
#define DEST_SESSION_ID 10

extern char *optarg;

void make_sesid(char session_id[3], unsigned src, unsigned dest)
{
	unsigned tmp_sesid = 0;
	unsigned msk = (1 << SLOT_ID_WIDTH) - 1;
	tmp_sesid = src & msk;
	tmp_sesid |= (dest & msk) << SLOT_ID_WIDTH;
	memcpy(session_id, &tmp_sesid, 3);
}

int gbn_connect(int sockfd, struct sockaddr *addr, unsigned src_sesid)
{
	int ret;
	unsigned long buff[2];
	struct gbn_header_board *syn_header = (struct gbn_header_board *)buff;
	syn_header->type = pkt_type_data;
	syn_header->seqnum = 0;
	make_sesid(syn_header->session_id, SRC_SESSION_ID, 0);

	buff[1] = 0x0101010101010101;
	for (int i = 0; i < 2; i++) {
		printf("buf[%d]: %016lx\n", i, buff[i]);
	}
	ret = sendto(sockfd, buff, sizeof(buff), 0, addr,
		     sizeof(struct sockaddr));
	return ret;
}

int gbn_close(int sockfd, struct sockaddr *addr, unsigned dest_sesid)
{
	int ret;
	unsigned long buff[2];
	struct gbn_header_board *fin_header = (struct gbn_header_board *)buff;
	fin_header->type = pkt_type_data;
	fin_header->seqnum = 0;
	make_sesid(fin_header->session_id, 0, DEST_SESSION_ID);

	buff[1] = 0x0f0f0f0f0f0f0f0f;
	for (int i = 0; i < 2; i++) {
		printf("buf[%d]: %016lx\n", i, buff[i]);
	}
	ret = sendto(sockfd, buff, sizeof(buff), 0, addr,
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

	struct gbn_header_board *header;

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
				SRC_SESSION_ID) < 0) {
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
		header = (struct gbn_header_board *)buf;
		header->type = pkt_type_data;
		header->seqnum = seqnum;
		make_sesid(header->session_id, SRC_SESSION_ID, DEST_SESSION_ID);

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
