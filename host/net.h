#ifndef _HOST_LOCAL_NET_H_
#define _HOST_LOCAL_NET_H_

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>

/*
 * Raw socket
 */
int raw_socket_open(char *if_name, struct ifreq *if_idx, struct ifreq *if_mac);
int raw_socket_send(int sock_fd, char *buf, int buf_size, struct sockaddr_ll *saddr);
void test_raw_socket();

#endif /* _HOST_LOCAL_NET_H_ */
