#include <zephyr/kernel.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/net/socket.h>

#include <zephyr/net/ethernet.h>

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_conn_mgr.h>

#define PORT 1080
#define BUFFER_SIZE 50

#ifdef CONFIG_NET_IPV4
    #define SERVER_ADDR CONFIG_NET_CONFIG_PEER_IPV4_ADDR
    #define CLIENT_ADDR CONFIG_NET_CONFIG_MY_IPV4_ADDR
    #define NETMASK "255.255.255.0"
    #define GATEWAY CONFIG_NET_CONFIG_MY_IPV4_GW
#endif

#ifdef CONFIG_NET_IPV6
    #define SERVER_ADDR CONFIG_NET_CONFIG_PEER_IPV6_ADDR
    #define CLIENT_ADDR CONFIG_NET_CONFIG_MY_IPV6_ADDR
#endif

#if 1
struct Data{
	uint32_t a;
	char b;
}data;

char recv_buffer[105];
#endif

void main(void)
{
#if 0
	char msg[128] = "I am broadCast message from server!";
	int brdcFd;
	if((brdcFd = socket(PF_INET, SOCK_DGRAM, 0)) == -1){
		printf("socket fail\n");
		return -1;
	}
	int optval = 1;//这个值一定要设置，否则可能导致sendto()失败
	setsockopt(brdcFd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int));
	struct sockaddr_in theirAddr;
	memset(&theirAddr, 0, sizeof(struct sockaddr_in));
	theirAddr.sin_family = AF_INET;
	// theirAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
    inet_pton(AF_INET, CLIENT_ADDR, &theirAddr.sin_addr);

	theirAddr.sin_port = htons(PORT);
	int sendBytes;
	if((sendBytes = sendto(brdcFd, msg, strlen(msg), 0,
			(struct sockaddr *)&theirAddr, sizeof(struct sockaddr))) == -1){
		printf("sendto fail, errno=%d\n", errno);
		return -1;
	}
	printf("msg=%s, msgLen=%d, sendBytes=%d\n", msg, strlen(msg), sendBytes);
	close(brdcFd);
#endif

#if 1
#ifdef CONFIG_NET_IPV4
    struct net_if *iface = net_if_get_default();
    struct in_addr addr, netmask, gw;

    inet_pton(AF_INET, CLIENT_ADDR, &addr);
    inet_pton(AF_INET, NETMASK, &netmask);
    inet_pton(AF_INET, GATEWAY, &gw);

    net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
    net_if_ipv4_set_netmask(iface, &netmask);
    net_if_ipv4_set_gw(iface, &gw);

    struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(PORT),
    };
    inet_pton(AF_INET, CLIENT_ADDR, &server_addr.sin_addr);
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
#endif

#ifdef CONFIG_NET_IPV6
    struct sockaddr_in6 server_addr = {
    .sin6_family = AF_INET6,
    .sin6_port = htons(PORT),
    };
    inet_pton(AF_INET6, CLIENT_ADDR, &server_addr.sin6_addr);
    int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in6 client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
#endif

    if (sock < 0) {
        printk("Failed to create socket\n");
        return;
    }	

    int ret = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        printk("Failed to bind socket\n");
        close(sock);
        return;
    }

    listen(sock, 2);

    int client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);

	printk("connect!\n");

	ret = recv(client_sock, recv_buffer, sizeof(recv_buffer), 0);

    if (ret < 0) {
        printk("Failed to send migration data\n");
		return;
    }else {
		printk("%s\n",recv_buffer);
	}
	memset(recv_buffer, 0, sizeof(recv_buffer));

	memset(&data, 0, sizeof(data));

	ret = recv(client_sock, recv_buffer, sizeof(recv_buffer), 0);
	memcpy(&data, recv_buffer, sizeof(data));

	printk("Data a:%d b:%c\n", data.a, data.b);

	close(sock);
#endif
}
