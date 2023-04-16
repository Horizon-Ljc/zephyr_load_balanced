#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/socket.h>

#include <zephyr/random/rand32.h>

/* size of stack area used by each thread */
#define STACKSIZE 2048

#define RECV_BUFFER_SIZE_2 1080

/* scheduling priority used by each thread */
#define PRIORITY 7

/* delay between greetings (in ms) */
#define SLEEPTIME 500

#ifdef CONFIG_NET_IPV6
    #define SERVER_ADDR CONFIG_NET_CONFIG_PEER_IPV6_ADDR
    #define SERVER_ADDR_2 "2001:db8:100::7"
    #define CLIENT_ADDR CONFIG_NET_CONFIG_MY_IPV6_ADDR
#endif

#ifdef CONFIG_NET_IPV4
    #define SERVER_ADDR CONFIG_NET_CONFIG_PEER_IPV4_ADDR
    #define CLIENT_ADDR CONFIG_NET_CONFIG_MY_IPV4_ADDR
    #define NETMASK "255.255.255.0"
    #define GATEWAY CONFIG_NET_CONFIG_MY_IPV4_GW
#endif

// migration data
#define PORT 1080

// load data
#define PORT_2 1081

// todo: extend
#define MACHINE_NUM 1
#define SYSTEM_NUM 5
#define LOAD_ALPHA 114514
// #define MACHINE_NO CONFIG_MACHINE_NO
#define BUFFER_SIZE_SOCKET1 1024
#define BUFFER_SIZE_SOCKET2 21

K_THREAD_STACK_DEFINE(threadA_stack_area, STACKSIZE);
static struct k_thread threadA_data;

K_THREAD_STACK_DEFINE(threadB_stack_area, STACKSIZE);
static struct k_thread threadB_data;

K_THREAD_STACK_DEFINE(threadC_stack_area, STACKSIZE);
static struct k_thread threadC_data;

K_THREAD_STACK_DEFINE(thread_client1_stack, STACKSIZE);
static struct k_thread thread_client1_data;

K_THREAD_STACK_DEFINE(migraton_handle_stack_1, STACKSIZE);
static struct k_thread migraton_handle_data_1;

K_THREAD_STACK_DEFINE(migraton_handle_stack_2, STACKSIZE);
static struct k_thread migraton_handle_data_2;

struct _callee_saved register_save;
char *sp_addressm;
struct _thread_stack_info thread_stk;

char migration_ip[SYSTEM_NUM + 1][20];
int weight[SYSTEM_NUM + 1];

size_t cpuload;
size_t stackload;

size_t system_cpu_load;
size_t system_stack_load;

struct load_datas {
    size_t no;
    size_t cpuloads;
    size_t stackloads;
} array_load[SYSTEM_NUM + 1], load_temp, data_send;

char send_buffer_2[BUFFER_SIZE_SOCKET2];
char recv_buffer_2[BUFFER_SIZE_SOCKET2];

int i;

struct Thread_state {
    int cond_migration;

    int no;
    int cpuloads;
    int stackloads;
    /* 线程栈 */
    // Reduce the size of the stack first, if the program is too large for some reason it will crash
    // Crash when the value of the array size is close to the size of the stack
    // char stack[STACKSIZE+2];
    char stack[BUFFER_SIZE_SOCKET1];

    /* 线程寄存器 */
#ifdef CONFIG_X86
    unsigned long esp;
#endif
} thread_state, thread_state_r;

char send_buffer[sizeof(thread_state)+5];
char recv_buffer[sizeof(thread_state_r)+5];

void calc_system_load();

void thread_analyze_get(struct k_thread *cthread, size_t *cpuload, size_t *stackload);

K_SEM_DEFINE(migration_server_handle_sem, 0, 1);

unsigned int fnv1a_hash(char *data, int len)
{
    const unsigned int FNV_offset_basis = 2166136261;
    const unsigned int FNV_prime = 16777619;

    unsigned int hash = FNV_offset_basis;
    for (int i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= FNV_prime;
    }
    return hash;
}

void system_loadget_handle()
{
    k_sched_lock();
    // memset(&load_temp, 0, sizeof(load_temp));

    // memcpy(&load_temp, recv_buffer_2, sizeof(load_temp));

    printf("[system_loadget handle]: system_no %u, cpuload %u %% , stackload %u %%\n", thread_state_r.no, 
        thread_state_r.cpuloads, thread_state_r.stackloads);

    array_load[thread_state_r.no].cpuloads = thread_state_r.cpuloads;
    array_load[thread_state_r.no].stackloads = thread_state_r.stackloads;

    // printf("[system_loadget handle]: ");
    // calc_system_load();
    k_sched_unlock();
    // k_msleep(3000);
}

void system_migration_handle()
{
    k_sched_lock(); 

    // Thread Recovery; need to preemption prevention
#ifdef CONFIG_X86
    printf("[migration server handle]: old threadA's esp is %ld\n", thread_state_r.esp);
    printf("[migration server handle]: old threadA's stk start is %lu size is %d\n", thread_stk.start, thread_stk.size);

    thread_stk = threadA_data.stack_info;
    printf("[migration server handle]: new threadA's esp is %ld\n", threadA_data.callee_saved.esp);
    printf("[migration server handle]: new threadA's stk start is %lu size is %d\n", thread_stk.start, thread_stk.size);

    threadA_data.callee_saved.esp = thread_state_r.esp;

    memcpy((void *)((size_t)thread_state_r.esp - 20), thread_state_r.stack,
            thread_stk.start + thread_stk.size - (size_t)thread_state_r.esp + 20);
    printf("[migration server handle]: para1 is %d para2 is %d para3 is %d\n", (void *)((size_t)thread_state_r.esp - 20),
            thread_state_r.stack, thread_stk.start + thread_stk.size - (size_t)thread_state_r.esp + 20);
#endif

    printf("\n[migration server handle]: load stack space success\n");

    printf("[migration server handle]: end\n");
    k_sched_unlock();
    // k_msleep(3000);
}

// tag accepted client number
int client_no = 0;

void migration_server_handle(void *client_sock, void *dummy2, void *dummy3)
{
    int client_sock_2 = *(int *)(client_sock);
    while(1){
        memset(recv_buffer, 0 ,sizeof(recv_buffer)); // test
        // memset(&thread_state_r, 0, sizeof(thread_state_r));

        int ret;
        ret = recv(client_sock_2, (char *)&thread_state_r, sizeof(thread_state_r), 0);

        if (ret < 0) {
            printf("[migration server handle]: Failed to recv migration data\n");
            printf("[migration server handle]: end\n");
            // close(sock);
            // continue;
            // client_no--;
            return ;
        }else if(ret == 0){
            printf("[migration server handle]: no data received yet\n");
            k_msleep(3000); 
            continue;       
        }

        printf("[migration server handle]: recive data, size is %d\n", ret);

		// memcpy(&thread_state_r, recv_buffer, sizeof(thread_state_r));

		printf("[migration server handle]: recive load data\n");
		// printf("[migration server handle]: recive load data's hash is %u\n",
		// fnv1a_hash(recv_buffer, sizeof(load_temp)));

		// memcpy(recv_buffer_2, recv_buffer, sizeof(load_temp));// test

		// system_loadget_handle();

        if(thread_state_r.cond_migration != 0){
            printf("[migration server handle]: recive migration data\n");

            // printf("[migration server handle]: recive migration data's hash is %u\n",
            // fnv1a_hash(recv_buffer, sizeof(thread_state)));

            system_migration_handle();
        }
        
        // if(k_sem_take(&migration_client_socket_1, K_MSEC(500)) == 0)

    }
}

void migration_server(void *dummy1, void *dummy2, void *dummy3)
{
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);
    
    printf("[migration server]: start\n");

#ifdef CONFIG_NET_IPV6
    struct sockaddr_in6 client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    struct sockaddr_in6 server_addr = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(PORT)
    };
    inet_pton(AF_INET6, CLIENT_ADDR, &server_addr.sin6_addr);

    int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
#endif

#ifdef CONFIG_NET_IPV4
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };
    inet_pton(AF_INET, CLIENT_ADDR, &server_addr.sin_addr);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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

    listen(sock, SYSTEM_NUM + 1);

    while(1){
        k_msleep(1000);
        printf("[migration server]: migration server waiting accept\n");

        // todo :poll(); if a client connection request is made, a new thread is created for processing
        int client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);

        if(client_sock == -1){
            printf("[migration server]: Failed to accept\n");
            printf("[migration server] end\n");
            close(sock);
            continue ;
        }
        printf("[migration server]: migration server accept %d client\n", client_no + 1);

        if(client_no == 0){
            client_no ++;
            k_thread_create(&migraton_handle_data_1, migraton_handle_stack_1,
                    K_THREAD_STACK_SIZEOF(migraton_handle_stack_1),
                    migration_server_handle, (void *)&client_sock, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);
            k_thread_name_set(&migraton_handle_data_1, "migraton_handle_data_1");
        }else if(client_no == 1){
            client_no ++;
            // k_thread_create(&migraton_handle_data_2, migraton_handle_stack_2,
            //      K_THREAD_STACK_SIZEOF(migraton_handle_stack_2),
            //      migration_server_handle, client_sock, NULL, NULL,
            //      PRIORITY, 0, K_NO_WAIT);
            // k_thread_name_set(&migraton_handle_data_2, "migraton_handle_data_2");
        }else {
            printf("client connection is full\n");
        }
        k_msleep(1000);
    }
    close(sock);
}

void thread_loadget(int sock)
{
    // thread_state.no = MACHINE_NUM;
    thread_state.no = MACHINE_NUM + 1;

    thread_analyze_get(&threadA_data, &cpuload, &stackload);
    printf("[migration client socket 1]: machine number %u, cpuload:%u %% stackload:%u %%\n",
		MACHINE_NUM, cpuload, stackload);

    array_load[MACHINE_NUM].cpuloads = cpuload;
    array_load[MACHINE_NUM].stackloads = stackload;

    thread_state.cpuloads = cpuload;
    thread_state.stackloads = stackload;

	thread_state.cond_migration = 0;// No migration by default
}

K_SEM_DEFINE(migration_client_socket_1, 0, 1);

void migration_client_socket()
{
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

    #ifdef CONFIG_NET_IPV4
        struct sockaddr_in server_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(PORT),
        };
        inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    #endif

        if (sock < 0) {
            printf("[migration client socket 1]: Failed to create socket\n");
            close(sock);
            return;
        }

        printf("[migration client socket 1]: waiting connect\n");
        int ret = -1;

        ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

        if(ret < 0){
            printf("[migration client socket 1]: Failed to connect\n");
            printf("[migration client socket 1]: reconnect\n");
            k_msleep(5000);
            close(sock);
            //return ;
            continue;
        }else {
            printf("[migration client socket 1]: connected\n");
        }

        while(1){
            k_msleep(5000);

            thread_loadget(sock);

            // k_msleep(5000);
            if(k_sem_take(&migration_client_socket_1, K_MSEC(50)) == 0){
                //k_sem_take(&migration_client_1, K_FOREVER);
                // send thread status
				thread_state.cond_migration = 1;
            }
			memset(send_buffer, 0, sizeof(send_buffer));
			memcpy(send_buffer, &thread_state, sizeof(thread_state));

			ret = send(sock, (char *)&thread_state, sizeof(send_buffer), 0);

			printf("[migration client socket 1]: send data's hash is %u, sizeof(send_buffer) is %d\n",
				fnv1a_hash(send_buffer, sizeof(send_buffer)), sizeof(send_buffer));

			if (ret < 0) {
				printf("[migration client socket 1]: Failed to send data\n");
				printf("[migration client socket 1]: end\n");
				break;                  
			}

			printf("[migration client socket 1]: send data success, size of data is %d\n", ret);
        }
        printf("[migration client socket 1] end\n");

        close(sock);
    }
}

void checkpoint(void *dummy1, void *dummy2, void *dummy3)
{
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);
    while(1){
        thread_stk = threadA_data.stack_info;
        
        k_msleep(20000);
        k_sched_lock();
        // k_thread_suspend(&threadA_data);// prevent threadA's integrity

        thread_analyze_get(&threadA_data, &cpuload, &stackload);
        printf("\n[checkpoint]: start, threadA cpuload:%d stackload:%d\nsave threadA's infomation\n", cpuload, stackload);

        // memset(&thread_state, 0, sizeof(thread_state));

        register_save = threadA_data.callee_saved;

        printf("[checkpoint]: threadA's stack start address:%lu stack size:%d\n", thread_stk.start, thread_stk.size);

    #ifdef CONFIG_X86
        thread_state.esp=register_save.esp;
        printf("[checkpoint]: threadA's esp %ld\n", thread_state.esp);

        // memcpy(thread_state.stack, (char *)thread_stk.start, thread_stk.size);
        memcpy(thread_state.stack, (void *)((size_t)thread_state.esp - 20),
                thread_stk.start + thread_stk.size - (size_t)thread_state.esp + 20);
    #endif

        k_sched_unlock();
        // todo: if need to migrate
        int ok = 1;
        if(ok != 0){
            // todo: extend
            // int migration_num = get_load_blance();
            int migration_num = 0;

            printf("[checkpoint]: the migration machine number is %d\n", migration_num);

            int pos_num = migration_num < MACHINE_NUM ? migration_num : (migration_num - 1);

            if(pos_num == 0){
                printf("[checkpoint]: the migration machine pos_num is %d\n", pos_num);
                k_sem_give(&migration_client_socket_1);
                // printf("[checkpoint]: the migration machine pos_num is %d\n")
            }else if(pos_num == 1){
                printf("[checkpoint]: the migration machine pos_num is %d\n", pos_num);
                //k_sem_give(&migration_client_socket_2);
            }
            ok--;
        }

        printf("[checkpoint]: end\n\n");
        // k_thread_resume(&threadA_data);// prevent threadA's integrity

        k_msleep(10000);
    }
}

void calc_system_load()
{
    k_sched_lock();
    
    system_cpu_load = 0, system_stack_load = 0;

    int cnt=0;
    for(int i = 0; i < SYSTEM_NUM; i++){
        system_cpu_load += array_load[i].cpuloads;
        system_stack_load += array_load[i].stackloads;
        if(array_load[i].cpuloads && array_load[i].stackloads){
            cnt++;
        }
    }
    if(cnt != 0){
        system_cpu_load /= cnt;
        system_stack_load /= cnt;
    }
    
    printf("update system load data success, system cpuload:%u %% stackload:%u %%\n", system_cpu_load
    , system_stack_load);

    k_sched_unlock();

    return ;
}

void threadA(void *dummy1, void *dummy2, void *dummy3)
{
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);

    // int machine_num = MACHINE_NUM;
    int cnt = 1;
    // int ok = 1; 

    // printf("[threadA] start, machine num is %d\n", machine_num);

    while (1){
        printf("[threadA] cycle counting: %d\n", cnt++);
        
        k_busy_wait(1000000);
    }
}

void init()
{
    // todo: extend
    strcpy(migration_ip[0], CLIENT_ADDR);

    strcpy(migration_ip[1], SERVER_ADDR);

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

    k_thread_create(&threadA_data, threadA_stack_area,
            K_THREAD_STACK_SIZEOF(threadA_stack_area),
            threadA, NULL, NULL, NULL,
            PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&threadA_data, "thread_a");

    k_thread_create(&threadB_data, threadB_stack_area,
            K_THREAD_STACK_SIZEOF(threadB_stack_area),
            checkpoint, NULL, NULL, NULL,
            PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&threadB_data, "thread_b");

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
}