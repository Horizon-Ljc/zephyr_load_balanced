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

// to get r4, r5, r6, r7, r8, r9, r10, r11, r13
struct _callee_saved register_save;
char *sp_addressm;
struct _thread_stack_info thread_stk;

int i;

struct Thread_state {
    /* 线程栈 */
    //int32_t stack[STACKSIZE/4];

	// Reduce the size of the stack first, if the program is too large for some reason it will crash
	// Crash when the value of the array size is close to the size of the stack
	// char stack[STACKSIZE+2];
	char stack[1024];

    /* 线程寄存器 */
	unsigned long esp;
} thread_state, thread_state_r;

char send_buffer[sizeof(thread_state)+2];
char recv_buffer[sizeof(thread_state_r)+2];

void thread_migration_server(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

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

	// Need to pay attention to the byte order problem !!!
	memset(&thread_state_r, 0, sizeof(thread_state_r));


	ret = recv(client_sock, recv_buffer, sizeof(thread_state_r), 0);

    if (ret < 0) {
        printf("Failed to send migration data\n");
		return;
    }
	printf("recive data, size is %d\n", ret);


	memcpy(&thread_state_r, recv_buffer, sizeof(thread_state_r));

	printf("thread esp:%ld\n", thread_state_r.esp);
	// for(i = 0; i < ((thread_stk.size + thread_stk.start - thread_state.esp + 8)/4); i++){
	// 	printf("%x\n", thread_state_r.stack[i]);
	// }

#if 1
	// Thread Recovery; need to preemption prevention
	k_sched_lock(); 
#ifdef CONFIG_X86
	threadA_data.callee_saved.esp=thread_state_r.esp;
#endif

	// sp_addressm = thread_stk.start;

	// for(i = 0; i < thread_stk.size/4; i++){
	// 	*sp_addressm=thread_state_r.stack[i];
	// 	sp_addressm++;
	// }

	// sp_addressm = thread_state_r.esp-8;

	// for(i = 0; i < ((thread_stk.size + thread_stk.start - thread_state.esp + 8)/4); i++){
	// 	*sp_addressm=thread_state_r.stack[i];
	// 	sp_addressm++;
	// }
	memcpy(thread_state_r.esp - 8, thread_state_r.stack, thread_stk.start + thread_stk.size - thread_state.esp + 8);

	k_sched_unlock();

	printf("load stack space success\n");
#endif

	close(sock);
}


// K_TIMER_DEFINE(migration_timer, thread_migration_server, NULL);

void thread_migration_client(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

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
// #endif

void threadB(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	printf("this is threadB, save and send thread infomation\n");

	memset(&thread_state, 0, sizeof(thread_state));

	register_save = threadA_data.callee_saved;

	thread_state.esp=register_save.esp;
	printf("threadA esp:%ld\n", thread_state.esp);

	thread_stk = threadA_data.stack_info;
	printf("k_thread var, stack start address:%lu stack size:%d\n", thread_stk.start, thread_stk.size);

	printf("K_THREAD_STACK_DEFINE, stack start %d\n", &threadA_stack_area->data);

#if 0
	// sp_addressm = thread_stk.start;	
	sp_addressm = thread_stk.start;

	for(i = 0; i < thread_stk.size; i++){
		printf("%u:%x ", sp_addressm, *sp_addressm);
		// thread_state.stack[i] = *sp_addressm;
		sp_addressm++;
	}
#endif

	// A simpler way to copy the stack
	// memcpy(thread_state.stack, (char *)thread_stk.start, thread_stk.size);
	memcpy(thread_state.stack, thread_state.esp - 8, thread_stk.start + thread_stk.size - thread_state.esp + 8);

	printf("sizeof(stack):%d sizeof(esp):%d\n", sizeof(thread_state.stack), sizeof(thread_state.esp));
	printf("sizeof(thread_state):%d\n", sizeof(thread_state));
	
	// printf("\nthis is data of memcpy\n");
	// for(i = 0; i < thread_stk.size; i++){
	// 	printf("%x ", thread_state.stack[i]);
	// }

	k_thread_create(&threadB_data, threadB_stack_area,
			K_THREAD_STACK_SIZEOF(threadB_stack_area),
			thread_migration_client, NULL, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);
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
			threadB(0, 0, 0);
			ok--;
		}
	}

}

void main(void)
{
	// k_timer_start(&migration_timer, K_SECONDS(5), K_SECONDS(5));

	k_thread_create(&threadA_data, threadA_stack_area,
			K_THREAD_STACK_SIZEOF(threadA_stack_area),
			threadA, NULL, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);

	k_thread_create(&threadC_data, threadC_stack_area,
			K_THREAD_STACK_SIZEOF(threadC_stack_area),
			thread_migration_server, NULL, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);


}
