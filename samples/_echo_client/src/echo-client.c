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

char send_buffer[105] = "this is client, i am sending migration data";
#endif

void main(void)
{
#if 0
	int sockListen;
	if((sockListen = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		printf("socket fail\n");
		return -1;
	}
	int set = 1;
	setsockopt(sockListen, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(int));
	struct sockaddr_in recvAddr;
	memset(&recvAddr, 0, sizeof(struct sockaddr_in));
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(PORT);
	recvAddr.sin_addr.s_addr = INADDR_ANY;
	// 必须绑定，否则无法监听
	if(bind(sockListen, (struct sockaddr *)&recvAddr, sizeof(struct sockaddr)) == -1){
		printf("bind fail\n");
		return -1;
	}
	int recvbytes;
	char recvbuf[128];
	int addrLen = sizeof(struct sockaddr_in);
	if((recvbytes = recvfrom(sockListen, recvbuf, 128, 0,
		(struct sockaddr *)&recvAddr, &addrLen)) != -1){
		recvbuf[recvbytes] = '\0';
		printf("receive a broadCast messgse:%s\n", recvbuf);
	}else{
		printf("recvfrom fail\n");
	}
	close(sockListen);
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
    inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif

#ifdef CONFIG_NET_IPV6
    struct sockaddr_in6 server_addr = {
    .sin6_family = AF_INET6,
    .sin6_port = htons(PORT),
    };
    inet_pton(AF_INET6, SERVER_ADDR, &server_addr.sin6_addr);
    int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
#endif

    if (sock < 0) {
        printk("Failed to create socket\n");
        return;
    }
    int ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

	if(ret == 0){
		printk("connect!\n");
	}else {
		printk("error:%d %s\n", errno, strerror(errno));
	}

	data.a = 10,data.b = 'b';

	ret = send(sock, send_buffer, sizeof(send_buffer), 0);

    if (ret < 0) {
        printk("Failed to send migration data\n");
		return;
    }

	memset(send_buffer, 0, sizeof(send_buffer));
	memcpy(send_buffer, &data, sizeof(data));

	ret = send(sock, send_buffer, sizeof(send_buffer), 0);

	close(sock);
    // printf("0x001327e0:%d\n", *datas);
#endif

}
