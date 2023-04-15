#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <zephyr/random/rand32.h>

#include <zephyr/net/socket.h>
#include <poll.h>
#if !defined(__ZEPHYR__) || defined(CONFIG_POSIX_API)

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#endif

/* size of stack area used by each thread */
#define STACKSIZE 2048

/* scheduling priority used by each thread */
#define PRIORITY 7

/* delay between greetings (in ms) */
#define SLEEPTIME 500

// todo: extend
#ifdef CONFIG_NET_IPV6
    #define SERVER_ADDR CONFIG_NET_CONFIG_PEER_IPV6_ADDR
    #define SERVER_ADDR_2 "2001:db8:100::7"
    #define CLIENT_ADDR CONFIG_NET_CONFIG_MY_IPV6_ADDR
#endif

// migration and load data
#define PORT 1080
// in reserve
#define PORT_2 1081

// todo: extend
#define MACHINE_NUM 1
// todo: extend
#define SYSTEM_NUM 3
#define BUFFER_SIZE_SOCKET1 256

#ifdef CONFIG_NET_SOCKETS_POLL_MAX
#define POLL_NUM CONFIG_NET_SOCKETS_POLL_MAX
#else
#define POLL_NUM 128
#endif

K_THREAD_STACK_DEFINE(threadC_stack_area, STACKSIZE);
static struct k_thread threadC_data;

K_THREAD_STACK_DEFINE(thread_client1_stack, STACKSIZE);
static struct k_thread thread_client1_data;

K_THREAD_STACK_DEFINE(thread_client2_stack, STACKSIZE);
static struct k_thread thread_client2_data;

char migration_ip[SYSTEM_NUM + 2][30];

struct Thread_state {
    int cond_migration;

    int no;
    int cpuloads;
    int stackloads;
    /* 线程栈 */
    // todo: reduce the size of the stack first, if the program is too large for some reason it will crash
    char stack[BUFFER_SIZE_SOCKET1];

    /* 线程寄存器 */
#ifdef CONFIG_X86
    unsigned long esp;
#endif
} thread_state, thread_state_r;

int i;
// todo: extend
int cli_ok_1, cli_ok_2;

void migration_server(void *dummy1, void *dummy2, void *dummy3)
{
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);
    
    printf("[migration server]: start\n");

#ifdef CONFIG_NET_IPV6
    struct sockaddr_in6 client_addr;

    struct sockaddr_in6 server_addr = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(PORT)
    };
    inet_pton(AF_INET6, CLIENT_ADDR, &server_addr.sin6_addr);

    int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
#endif

    if (sock < 0) {
        printf("[migration server]: Failed to create socket\n");
        close(sock);
        return;
    }   

    int ret = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        printf("[migration server]: Failed to bind socket\n");
        close(sock);
        return;
    }

    listen(sock, SYSTEM_NUM + 100);

    // tag accepted client number, todo: if the connection is close, client_no should reduce one
    int client_no = 0;

    // 初始化检测的文件描述符数组
    struct pollfd fds[POLL_NUM];
    for(int i = 0; i < POLL_NUM; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
    }
    fds[0].fd = sock;
    int nfds = 0;

    while(1) {
        // 调用poll系统函数，让内核帮检测哪些文件描述符有数据
        int ret = poll(fds, nfds + 1, -1);
        if(ret == -1) {
            perror("poll");
            exit(-1);
        } else if(ret == 0) {
            continue;
        } else if(ret > 0) {
            // 说明检测到了有文件描述符的对应的缓冲区的数据发生了改变
            if(fds[0].revents & POLLIN) {
                // 表示有新的客户端连接进来了
                struct sockaddr_in cliaddr;
                int len = sizeof(cliaddr);
                int cfd = accept(sock, (struct sockaddr *)&cliaddr, &len);
                client_no++;
                printf("[migration server]: new connection fd is %d\n", cfd);

                if(cfd == -1){
                    printf("[migration server]: Failed to accept\n");
                    printf("[migration server]: end\n");
                    perror("accept");
                    continue ;
                }

                // 将新的文件描述符加入到集合中
                for(int i = 1; i < POLL_NUM; i++) {
                    if(fds[i].fd == -1) {
                        fds[i].fd = cfd;
                        fds[i].events = POLLIN;
                        break;
                    }
                }

                // 更新最大的文件描述符的索引
                nfds = nfds > cfd ? nfds : cfd;
                printf("[migration server]: max fd indexis %d\n", nfds);
            }

        }

    }
    close(sock);
}

void migration_client_socket(void *dummy1, void *dummy2, void *dummy3)
{
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);
    while(1){
        printf("[migration client socket 1]: start\n");

    #ifdef CONFIG_NET_IPV6
        struct sockaddr_in6 server_addr = {
            .sin6_family = AF_INET6,
            .sin6_port = htons(PORT),
        };
        inet_pton(AF_INET6, SERVER_ADDR, &server_addr.sin6_addr);

        int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    #endif

        if (sock < 0) {
            printf("[migration client socket 1]: Failed to create socket\n");
            close(sock);
            return;
        }

        // if(0>fcntl(sock,F_SETFL,fcntl(sock,F_GETFL,0)| O_NONBLOCK))
        // {
        //     printf("fcntl failed/n"); 
        //     return ; 
        // }

        printf("[migration client socket 1]: waiting connect\n");

        int ret = -1;
        ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

        if(ret < 0){
            printf("[migration client socket 1]: failed to connect\n");
            printf("[migration client socket 1]: start to reconnect\n");
            perror("connect");
        
            close(sock);
            k_msleep(100);
            continue;

        }

        printf("[migration client socket 1]: connected\n");
        cli_ok_1 = 1;
        // todo: extend 
        if(cli_ok_1 && cli_ok_2){
            printf("[migration client socket 1]: all connected!\n");
        }
        
        // todo: extend
        while(1){
            k_busy_wait(100);
        }
        printf("[migration client socket 1]: end\n");

        close(sock);
    }
}

void migration_client_socket_2(void *dummy1, void *dummy2, void *dummy3)
{
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);
    while(1){
        printf("[migration client socket 2]: start\n");

    #ifdef CONFIG_NET_IPV6
        struct sockaddr_in6 server_addr = {
            .sin6_family = AF_INET6,
            .sin6_port = htons(PORT),
        };
        inet_pton(AF_INET6, SERVER_ADDR_2, &server_addr.sin6_addr);

        int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    #endif

        if (sock < 0) {
            printf("[migration client socket 2]: Failed to create socket\n");
            close(sock);
            return;
        }

        // if(0>fcntl(sock,F_SETFL,fcntl(sock,F_GETFL,0)| O_NONBLOCK))
        // {
        //     printf("fcntl failed/n"); 
        //     return ; 
        // }

        printf("[migration client socket 2]: waiting connect\n");
        int ret = -1;
        ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

        if(ret < 0){
            printf("[migration client socket 2]: failed to connect\n");
            printf("[migration client socket 2]: start to reconnect\n");
            perror("connect");
        
            close(sock);
            k_msleep(100);
            continue;

        }

        printf("[migration client socket 2]: connected\n");
        cli_ok_2 = 1;
        // todo: extend
        if(cli_ok_1 && cli_ok_2){
            printf("[migration client socket 1]: all connected!\n");
        }
        
        // todo: extend
        while(1){
            k_busy_wait(1000);
        }
        printf("[migration client socket 2]: end\n");

        close(sock);
    }
}

void init()
{
    // todo: extend
    strcpy(migration_ip[0], CLIENT_ADDR);

    strcpy(migration_ip[1], SERVER_ADDR);

    strcpy(migration_ip[2], SERVER_ADDR_2);

    // strcpy(migration_ip[2], SERVER_ADDR_2);
    for(i = 0; i < SYSTEM_NUM; i++){
        if(i == 0){
            printf("[main] host ip:%s\n", migration_ip[i]);
        }else {
            printf("[main] client ip:%s\n", migration_ip[i]);
        }
    }

}

void main(void)
{
    init();

    k_thread_create(&threadC_data, threadC_stack_area,
            K_THREAD_STACK_SIZEOF(threadC_stack_area),
            migration_server, NULL, NULL, NULL,
            PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&threadC_data, "thread_c");

    k_thread_create(&thread_client1_data, thread_client1_stack,
            K_THREAD_STACK_SIZEOF(thread_client1_stack),
            migration_client_socket, NULL, NULL, NULL,
            PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&thread_client1_data, "thread_client_1");

    k_thread_create(&thread_client2_data, thread_client2_stack,
            K_THREAD_STACK_SIZEOF(thread_client2_stack),
            migration_client_socket_2, NULL, NULL, NULL,
            PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&thread_client2_data, "thread_client_2");
}