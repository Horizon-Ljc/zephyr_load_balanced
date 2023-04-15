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

#define SERVER_ADDR CONFIG_NET_CONFIG_PEER_IPV6_ADDR
#define CLIENT_ADDR CONFIG_NET_CONFIG_MY_IPV6_ADDR

#define PORT 1080


K_THREAD_STACK_DEFINE(threadA_stack_area, STACKSIZE);
static struct k_thread threadA_data;

K_THREAD_STACK_DEFINE(threadB_stack_area, STACKSIZE);
static struct k_thread threadB_data;

K_THREAD_STACK_DEFINE(threadC_stack_area, STACKSIZE);
static struct k_thread threadC_data;

K_THREAD_STACK_DEFINE(threadD_stack_area, STACKSIZE);
static struct k_thread threadD_data;

struct _callee_saved register_save;
char *sp_addressm;
struct _thread_stack_info thread_stk;

size_t cpuload;
size_t stackload;

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
} thread_state, thread_state_r;

char send_buffer[sizeof(thread_state)+2];
char recv_buffer[sizeof(thread_state_r)+2];

void thread_migration_server(void *dummy1, void *dummy2, void *dummy3)
{
	while(1){
		ARG_UNUSED(dummy1);
		ARG_UNUSED(dummy2);
		ARG_UNUSED(dummy3);

		// thread_analyze_get(&threadA_data, &cpuload, &stackload);
		// printf("thread_migration_server, cpuload:%d stackload:%d\n", cpuload, stackload);

		printf("this is migration server\n");

		struct sockaddr_in6 server_addr = {
			.sin6_family = AF_INET6,
			.sin6_port = htons(PORT)
		};
		inet_pton(AF_INET6, CLIENT_ADDR, &server_addr.sin6_addr);

		int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
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

		struct sockaddr_in6 client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		printf("migration server waiting accept\n");
		int client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);


		if(client_sock == -1){
			printf("migration server: Failed to accept\n");
		}

		memset(&thread_state_r, 0, sizeof(thread_state_r));

		ret = recv(client_sock, recv_buffer, sizeof(thread_state_r), 0);

		if (ret < 0) {
			printf("Failed to send migration data\n");
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

		k_sched_unlock();

		printf("load stack space success\n");

		close(sock);
	}
}

void thread_migration_client()
{
	printf("this is migration client\n");

    struct sockaddr_in6 server_addr = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(PORT),
    };
    inet_pton(AF_INET6, SERVER_ADDR, &server_addr.sin6_addr);

    int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
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

// void thread_analyzer(void *dummy1, void *dummy2, void *dummy3)
// {
// 	while(1){
// 		thread_analyze_get(&threadA_data, &cpuload, &stackload);
// 		thread_analyze_get(&threadB_data, &cpuload, &stackload);
// 		thread_analyze_get(&threadC_data, &cpuload, &stackload);
// 		thread_analyze_get(&threadD_data, &cpuload, &stackload);

// 		k_sleep(K_MSEC(5000));
// 	}
// }

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

		// memcpy(thread_state.stack, (char *)thread_stk.start, thread_stk.size);
		memcpy(thread_state.stack, thread_state.psp - 8, thread_stk.start + thread_stk.size - thread_state.psp + 8);
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

	k_thread_create(&threadB_data, threadB_stack_area,
			K_THREAD_STACK_SIZEOF(threadB_stack_area),
			threadB, NULL, NULL, NULL,
			PRIORITY, 0, K_MSEC(5000));
	k_thread_name_set(&threadB_data, "thread_b");

	k_thread_create(&threadC_data, threadC_stack_area,
			K_THREAD_STACK_SIZEOF(threadC_stack_area),
			thread_migration_server, NULL, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&threadC_data, "thread_c");

	// k_thread_create(&threadD_data, threadD_stack_area,
	// 		K_THREAD_STACK_SIZEOF(threadD_stack_area),
	// 		thread_analyzer, NULL, NULL, NULL,
	// 		PRIORITY, 0, K_NO_WAIT);
	// k_thread_name_set(&threadD_data, "thread_d");
}