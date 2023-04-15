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
#define SYSTEM_NUM 5

K_THREAD_STACK_DEFINE(threadA_stack_area, STACKSIZE);
static struct k_thread threadA_data;

K_THREAD_STACK_DEFINE(threadB_stack_area, STACKSIZE);
static struct k_thread threadB_data;

K_THREAD_STACK_DEFINE(threadC_stack_area, STACKSIZE);
static struct k_thread threadC_data;

K_THREAD_STACK_DEFINE(threadD_stack_area, STACKSIZE);
static struct k_thread threadD_data;

K_THREAD_STACK_DEFINE(threadE_stack_area, STACKSIZE);
static struct k_thread threadE_data;

struct _callee_saved register_save;
char *sp_addressm;
struct _thread_stack_info thread_stk;

size_t cpuload;
size_t stackload;

size_t system_cpu_load;
size_t system_stack_load;

struct load_datas {
	size_t no;
	size_t cpuloads;
	size_t stackloads;
} array_load[SYSTEM_NUM], load_temp;

char send_buffer_2[sizeof(load_temp)+2];
char recv_buffer_2[sizeof(load_temp)+2];

int i;

struct Thread_state {
    /* 线程栈 */

	// Reduce the size of the stack first, if the program is too large for some reason it will crash
	// Crash when the value of the array size is close to the size of the stack
	// char stack[STACKSIZE+2];
	char stack[1024];

    /* 线程寄存器 */
#ifdef CONFIG_X86
	unsigned long esp;
#endif

#ifdef CONFIG_ARM
	uint32_t v1;
	uint32_t v2;
	uint32_t v3;
	uint32_t v4;
	uint32_t v5;
	uint32_t v6;
	uint32_t v7;
	uint32_t v8;
	uint32_t psp;
#endif

#ifdef __aarch64__
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t sp_el0;
	uint64_t sp_elx;
	uint64_t lr;
#endif

} thread_state, thread_state_r;

char send_buffer[sizeof(thread_state)+2];
char recv_buffer[sizeof(thread_state_r)+2];



void thread_migration_server(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);
	while(1){
		printf("this is migration server\n");

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
			printf("Failed to create socket\n");
			return;
		}	

		int ret = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if (ret < 0) {
			printf("Failed to bind socket\n");
			close(sock);
			return;
		}

		listen(sock, 5);

		printf("migration server waiting accept\n");
		int client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);


		if(client_sock == -1){
			printf("migration server: Failed to accept\n");
		}

		memset(&thread_state_r, 0, sizeof(thread_state_r));

		ret = recv(client_sock, recv_buffer, sizeof(thread_state_r), 0);

		if (ret < 0) {
			printf("Failed to recv migration data\n");
			return;
		}
		printf("recive data, size is %d\n", ret);

		memcpy(&thread_state_r, recv_buffer, sizeof(thread_state_r));

		// Thread Recovery; need to preemption prevention
		k_sched_lock(); 
	#ifdef CONFIG_X86
		threadA_data.callee_saved.esp=thread_state_r.esp;
	
		memcpy(thread_state_r.esp - 8, thread_state_r.stack, thread_stk.start + thread_stk.size - thread_state.esp + 8);
	#endif

	#ifdef CONFIG_ARM	
		threadA_data.callee_saved.v1=thread_state_r.v1;
		threadA_data.callee_saved.v2=thread_state_r.v2;
		threadA_data.callee_saved.v3=thread_state_r.v3;
		threadA_data.callee_saved.v4=thread_state_r.v4;
		threadA_data.callee_saved.v5=thread_state_r.v5;
		threadA_data.callee_saved.v6=thread_state_r.v6;
		threadA_data.callee_saved.v7=thread_state_r.v7;
		threadA_data.callee_saved.v8=thread_state_r.v8;
		threadA_data.callee_saved.psp=thread_state_r.psp;

		memcpy(thread_state_r.psp - 8, thread_state_r.stack, thread_stk.start + thread_stk.size - thread_state.psp + 8);
	#endif

	#ifdef __aarch64__	
		threadA_data.callee_saved.x19=thread_state_r.x19;
		threadA_data.callee_saved.x20=thread_state_r.x20;
		threadA_data.callee_saved.x21=thread_state_r.x21;
		threadA_data.callee_saved.x22=thread_state_r.x22;
		threadA_data.callee_saved.x23=thread_state_r.x23;
		threadA_data.callee_saved.x24=thread_state_r.x24;
		threadA_data.callee_saved.x25=thread_state_r.x25;
		threadA_data.callee_saved.x26=thread_state_r.x26;
		threadA_data.callee_saved.x27=thread_state_r.x27;
		threadA_data.callee_saved.x28=thread_state_r.x28;
		threadA_data.callee_saved.x29=thread_state_r.x29;
		threadA_data.callee_saved.sp_el0=thread_state_r.sp_el0;
		threadA_data.callee_saved.sp_elx=thread_state_r.sp_elx;
		threadA_data.callee_saved.lr=thread_state_r.lr;

		memcpy(thread_state_r.sp_elx - 16, thread_state_r.stack, thread_stk.start + thread_stk.size - thread_state.sp_elx + 16);
	#endif

		k_sched_unlock();

		printf("load stack space success\n");

		close(sock);
	}
}

void thread_migration_client()
{
	printf("this is migration client\n");

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
        printf("Failed to create socket\n");
        return;
    }

	printf("migration client waiting connect, size of thread_state to be sent is %d\n", sizeof(thread_state));
    int ret = -1;

	ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

	// Needs to be blocked all the time, Now the connection timeout will end automatically
	if(ret < 0){
		printf("migration client: Failed to connect\n");
	}

    // send thread status
	memset(send_buffer, 0, sizeof(send_buffer));
	memcpy(send_buffer, &thread_state, sizeof(thread_state));

	ret = send(sock, send_buffer, sizeof(thread_state), 0);

    if (ret < 0) {
        printf("Failed to send migration data\n");
		return;
    }

	printf("send migration data success, size of data is %d\n", ret);

	close(sock);
}

void calc_system_load(){

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
	system_cpu_load /= cnt;
	system_stack_load /= cnt;
	
	printf("update system load data success, system cpuload:%u %% stackload:%u %%\n", system_cpu_load
	, system_stack_load);

	k_sched_unlock();

	return ;
}

void system_loadget(void *dummy1, void *dummy2, void *dummy3)
{
	#ifdef CONFIG_NET_IPV6
		struct sockaddr_in6 client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		struct sockaddr_in6 server_addr = {
			.sin6_family = AF_INET6,
			.sin6_port = htons(PORT_2)
		};
		inet_pton(AF_INET6, CLIENT_ADDR, &server_addr.sin6_addr);

		int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	#endif

	#ifdef CONFIG_NET_IPV4
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		struct sockaddr_in server_addr = {
			.sin_family = AF_INET,
			.sin_port = htons(PORT_2)
		};
		inet_pton(AF_INET, CLIENT_ADDR, &server_addr.sin_addr);

		int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	#endif

	if (sock < 0) {
		printf("Failed to create socket\n");
		return;
	}	

	int ret = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		printf("Failed to bind socket\n");
		close(sock);
		return;
	}

	listen(sock, 5);

	printf("load server waiting accept\n");
	int client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);

	if(client_sock == -1){
		printf("load server: Failed to accept\n");
		return ;
	}

	while(1){

		printf("this is load server\n");

		memset(&load_temp, 0, sizeof(load_temp));

		ret = recv(client_sock, recv_buffer_2, sizeof(load_temp), 0);

		if (ret < 0) {
			printf("Failed to recv load data\n");
			return;
		}

		memcpy(&load_temp, recv_buffer_2, sizeof(load_temp));

		printf("recive data, size is %d\n", ret);
		printf("system_no %u, cpuload %u %% , stackload %u %%\n", load_temp.no, load_temp.cpuloads, load_temp.stackloads);

		array_load[load_temp.no-1].cpuloads = load_temp.cpuloads;
		array_load[load_temp.no-1].stackloads = load_temp.stackloads;

		calc_system_load();

		k_sleep(K_MSEC(5000));
	}
	close(sock);
}

void thread_loadget(void *dummy1, void *dummy2, void *dummy3)
{
	int system_no = 2;
	struct load_datas data_send;
	data_send.no = system_no;

	#ifdef CONFIG_NET_IPV6
		struct sockaddr_in6 server_addr = {
			.sin6_family = AF_INET6,
			.sin6_port = htons(PORT_2),
		};
		inet_pton(AF_INET6, SERVER_ADDR, &server_addr.sin6_addr);

		int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	#endif

	#ifdef CONFIG_NET_IPV4
		struct sockaddr_in server_addr = {
			.sin_family = AF_INET,
			.sin_port = htons(PORT_2),
		};
		inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);

		int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	#endif

	if (sock < 0) {
		printf("Failed to create socket\n");
		return;
	}

	int ret = -1;

	ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

	// Needs to be blocked all the time, Now the connection timeout will end automatically
	if(ret < 0){
		printf("load client: Failed to connect\n");
		return ;
	}

	while(1){
		thread_analyze_get(&threadA_data, &cpuload, &stackload);
		printf("this is thread_loadget %u, cpuload:%u %% stackload:%u %%\n", system_no, cpuload, stackload);

		array_load[system_no - 1].cpuloads = cpuload;
		array_load[system_no - 1].stackloads = stackload;

		data_send.cpuloads = cpuload;
		data_send.stackloads = stackload;

		calc_system_load();

	// #if 1

		printf("size of load_datas to be sent is %d\n", sizeof(data_send));

		// send thread status
		memset(send_buffer_2, 0, sizeof(send_buffer_2));
		memcpy(send_buffer_2, &data_send, sizeof(data_send));

		ret = send(sock, send_buffer_2, sizeof(data_send), 0);

		if (ret < 0) {
			printf("Failed to send load data\n");
			return;
		}

		printf("send load data success, size of data is %d\n", ret);

	// #endif
		k_sleep(K_MSEC(5000));

	}
	close(sock);
}

void threadB(void *dummy1, void *dummy2, void *dummy3)
{
	// while(1){
		// thread_analyze_get(&threadA_data, &cpuload, &stackload);
		// printf("thread B, cpuload:%d stackload:%d\n", cpuload, stackload);

		k_sched_lock();
		
		ARG_UNUSED(dummy1);
		ARG_UNUSED(dummy2);
		ARG_UNUSED(dummy3);

		printf("this is threadB, save and send threadA infomation\n");

		memset(&thread_state, 0, sizeof(thread_state));

		register_save = threadA_data.callee_saved;

		thread_stk = threadA_data.stack_info;
		printf("k_thread var, stack start address:%lu stack size:%d\n", thread_stk.start, thread_stk.size);

	#ifdef CONFIG_X86
		thread_state.esp=register_save.esp;
		printf("threadA esp:%ld\n", thread_state.esp);

		// memcpy(thread_state.stack, (char *)thread_stk.start, thread_stk.size);
		memcpy(thread_state.stack, thread_state.esp - 8, thread_stk.start + thread_stk.size - thread_state.esp + 8);
	#endif

	#ifdef CONFIG_ARM
		thread_state.v1=register_save.v1;
		thread_state.v2=register_save.v2;
		thread_state.v3=register_save.v3;
		thread_state.v4=register_save.v4;
		thread_state.v5=register_save.v5;
		thread_state.v6=register_save.v6;
		thread_state.v7=register_save.v7;
		thread_state.v8=register_save.v8;
		thread_state.psp=register_save.psp;

		printf("threadA psp:%u\n", thread_state.psp);
		printf("k_thread var, stack start address:%lu stack size:%d\n", thread_stk.start, thread_stk.size);

		// memcpy(thread_state.stack, (char *)thread_stk.start, thread_stk.size);
		memcpy(thread_state.stack, thread_state.psp - 8, thread_stk.start + thread_stk.size - thread_state.psp + 8);
	#endif

	#ifdef __aarch64__
		thread_state.x19=register_save.x19;
		thread_state.x20=register_save.x20;
		thread_state.x21=register_save.x21;
		thread_state.x22=register_save.x22;
		thread_state.x23=register_save.x23;
		thread_state.x24=register_save.x24;
		thread_state.x25=register_save.x25;
		thread_state.x26=register_save.x26;
		thread_state.x27=register_save.x27;
		thread_state.x28=register_save.x28;
		thread_state.x29=register_save.x29;
		thread_state.sp_el0=register_save.sp_el0;
		thread_state.sp_elx=register_save.sp_elx;
		thread_state.lr=register_save.lr;

		printf("threadA sp_el0:%llu sp_elx:%llu\n", thread_state.sp_el0, thread_state.sp_elx);
		printf("k_thread var, stack start address:%lu stack size:%d\n", thread_stk.start, thread_stk.size);

		// memcpy(thread_state.stack, (char *)thread_stk.start, thread_stk.size);
		memcpy(thread_state.stack, thread_state.sp_elx - 16, thread_stk.start + thread_stk.size - thread_state.sp_elx + 16);
	#endif

		thread_migration_client();

		k_sched_unlock();
	// }
}

void threadA(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	int cnt = 1;
	int ok = 1; 

	printf("threadA start\n");

	while (1){
		printf("this is threadA, cycle counting\n");

		printf("%d\n", cnt++);
		
		k_busy_wait(1000000);

		// meet the migration requirement, run threadB
		if (cnt > 10 && ok) {
			printf("condition is met, call threadB to migrate\n");

			ok--;
		}
	}
}

void main(void)
{
	k_thread_create(&threadA_data, threadA_stack_area,
			K_THREAD_STACK_SIZEOF(threadA_stack_area),
			threadA, NULL, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&threadA_data, "thread_a");

	// k_thread_create(&threadB_data, threadB_stack_area,
	// 		K_THREAD_STACK_SIZEOF(threadB_stack_area),
	// 		threadB, NULL, NULL, NULL,
	// 		PRIORITY, 0, K_MSEC(5000));
	// k_thread_name_set(&threadB_data, "thread_b");

	// k_thread_create(&threadC_data, threadC_stack_area,
	// 		K_THREAD_STACK_SIZEOF(threadC_stack_area),
	// 		thread_migration_server, NULL, NULL, NULL,
	// 		PRIORITY, 0, K_NO_WAIT);
	// k_thread_name_set(&threadC_data, "thread_c");

	k_thread_create(&threadD_data, threadD_stack_area,
			K_THREAD_STACK_SIZEOF(threadD_stack_area),
			thread_loadget, NULL, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&threadD_data, "thread_d");

	k_thread_create(&threadE_data, threadE_stack_area,
			K_THREAD_STACK_SIZEOF(threadE_stack_area),
			system_loadget, NULL, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&threadE_data, "thread_e");
}