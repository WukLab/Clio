#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <uapi/net_header.h>

#define FPGA_PORT 1234
#define PACKET_SIZE 4

union ip_addr {
	uint32_t ip;
	char ip_byte[4];
};

int main(int argc, char *argv[])
{
	int socketfd;
	struct sockaddr_in host_addr;

	unsigned int seqnum;
	unsigned long buf[PACKET_SIZE];
	char *host = "www.google.com";
	int i, send_size;

	if (argc != 2) {
		perror("arg error");
		exit(EXIT_FAILURE);
	}
	seqnum = atoi(argv[1]);

	if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket error");
		exit(EXIT_FAILURE);
	}

	union ip_addr host_ip;
	host_ip.ip_byte[0] = 192;
	host_ip.ip_byte[1] = 168;
	host_ip.ip_byte[2] = 1;
	host_ip.ip_byte[3] = 128;

	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons(FPGA_PORT);
	host_addr.sin_addr.s_addr = host_ip.ip;

	struct gbn_header header;
	memcpy(&header.seqnum, &seqnum, SEQ_SIZE);
	header.type = pkt_type_data;
	memcpy(&buf[0], &header, sizeof(header));
	buf[1] = 0;
	buf[2] = 1;
	buf[3] = 2;
	for (i = 0; i < PACKET_SIZE;i++) {
		printf("buf[%d]: %lx\n", i, buf[i]);
	}

	send_size = sendto(socketfd, buf, sizeof(buf), 0, (struct sockaddr *)&host_addr,
	       sizeof(struct sockaddr));

	if (send_size < 0) {
		perror("send error");
		exit(EXIT_FAILURE);
	}
	printf("send %d bytes\n", send_size);

	close(socketfd);

	return 0;
}
