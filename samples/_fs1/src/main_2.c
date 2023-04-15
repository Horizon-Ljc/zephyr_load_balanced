#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/socket.h>

#include <zephyr/random/rand32.h>

/* size of stack area used by each thread */
#define STACKSIZE 1024

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

#ifdef CONFIG_X86
// to get r0, r1, r2, r3, r12, r14, r15, xpsr
uint32_t registerm[10];
char registerm_name[10][6]={"r0", "r1", "r2", "r3", "r12", "lr", "pc", "xpsr"};
#endif

// to get r4, r5, r6, r7, r8, r9, r10, r11, r13
struct _callee_saved register_save;
char *sp_addressm;
struct _thread_stack_info thread_stk;

int i;

struct Thread_state {
    /* 线程栈 */
    //int32_t stack[STACKSIZE/4];

// Reduce the size of the stack first
	char stack[STACKSIZE+5];

    /* 线程寄存器 */
#ifdef CONFIG_ARM
    uint32_t reg0;
    uint32_t reg1;
    uint32_t reg2;
	uint32_t reg3;
	uint32_t reg4;
	uint32_t reg5;
	uint32_t reg6;
	uint32_t reg7;
	uint32_t reg8;
	uint32_t reg9;
	uint32_t reg10;
	uint32_t reg11;
	uint32_t reg12;
	uint32_t reg13;
	uint32_t reg14;
	uint32_t reg15;
	uint32_t reg16;
#endif

#ifdef CONFIG_X86
	unsigned long esp;
#endif

} thread_state, thread_state_r;

char send_buffer[sizeof(thread_state)+5];
char recv_buffer[sizeof(thread_state_r)+5];

// #if 1
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
	
	// if the transform data is big
	// int offset = 0;
	// int received;
	// do {
	// 	received = recv(client_sock, recv_buffer + offset, sizeof(recv_buffer) - offset, 0);

	// 	if (received == 0) {
	// 		/* Connection closed */
	// 		printf("TCP Connection closed\n");
	// 		break;
	// 	} else if (received < 0) {
	// 		/* Socket error */
	// 		printf("TCP Connection error\n");
	// 		break;
	// 	}

	// 	offset += received;

	// 	/* To prevent fragmentation of the response, reply only if
	// 	 * buffer is full or there is no more data to read
	// 	 */
	// 	if (offset == sizeof(recv_buffer) ||
	// 	    (recv(client_sock, recv_buffer + offset, sizeof(recv_buffer) - offset,
	// 		  MSG_PEEK | MSG_DONTWAIT) < 0 &&
	// 	     (errno == EAGAIN || errno == EWOULDBLOCK))) {

	// 		printf("eceived and replied with %d bytes", offset);

	// 		offset = 0;
	// 	}
	// } while (true);
	

	/* The incoming data needs to be deserialized
    struct Thread_state state;
    ret = recv(client_sock, &state, sizeof(state), 0);

    if (ret < 0) {
        printk("Failed to receive migration data\n");
        close(sock);
        return;
    }
	*/

	// Need to pay attention to the byte order problem !!!
	memset(&thread_state_r, 0, sizeof(thread_state_r));

	ret = recv(client_sock, recv_buffer, sizeof(recv_buffer), 0);

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


	// Thread Recovery; need to preemption prevention
	k_sched_lock(); 

#ifdef CONFIG_ARM
    // 将state恢复到需要迁移的线程中
	threadA_data.callee_saved.v1=thread_state_r.reg4, threadA_data.callee_saved.v2=thread_state_r.reg5, threadA_data.callee_saved.v3=thread_state_r.reg6,
	threadA_data.callee_saved.v4=thread_state_r.reg7, threadA_data.callee_saved.v5=thread_state_r.reg8, threadA_data.callee_saved.v6=thread_state_r.reg9,
	threadA_data.callee_saved.v7=thread_state_r.reg10, threadA_data.callee_saved.v8=thread_state_r.reg11, threadA_data.callee_saved.psp=thread_state_r.reg13;
#endif

#ifdef CONFIG_X86
	threadA_data.callee_saved.esp=thread_state_r.esp;
#endif

	// sp_addressm = thread_stk.start;

	// for(i = 0; i < thread_stk.size/4; i++){
	// 	*sp_addressm=thread_state_r.stack[i];
	// 	sp_addressm++;
	// }

#if 0
	sp_addressm = thread_state_r.esp-8;

	for(i = 0; i < ((thread_stk.size + thread_stk.start - thread_state.esp + 8)/4); i++){
		*sp_addressm=thread_state_r.stack[i];
		sp_addressm++;
	}
#endif
	k_sched_unlock();

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

	printf("migration client waiting connect, size of data to be sent is %d\n", sizeof(thread_state));
    int ret = -1;

	// return ;

	ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
	// while(ret < 0){
	// 	ret = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
	// }

	// !!!
	// Needs to be blocked all the time, Now the connection timeout will end automatically
	if(ret < 0){
		printf("migration client: Failed to connect\n");
	}

    // send thread status
	/* The data sent needs to be serialized
    int ret = send(sock, &thread_state, sizeof(thread_state), 0);
	*/

	// Need to pay attention to the byte order problem !!!
	memset(send_buffer, 0, sizeof(send_buffer));
	memcpy(send_buffer, &thread_state, sizeof(thread_state));

	ret = send(sock, send_buffer, sizeof(send_buffer), 0);

    if (ret < 0) {
        printf("Failed to send migration data\n");
		return;
    }

	printf("send migration data success, size of data is %d\n", ret);

	close(sock);
}
// #endif

void threadB(void *dummy1, void *dummy2, void *dummy3){
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	printf("this is threadB, save and send thread infomation\n");

	memset(&thread_state, 0, sizeof(thread_state));

	register_save = threadA_data.callee_saved;

#ifdef CONFIG_ARM
	printf("B r4:%u r5:%u r6:%u r7:%u r8:%u r9:%u r10:%u r11:%u sp:%u\n", register_save.v1, register_save.v2, register_save.v3, register_save.v4,
	register_save.v5, register_save.v6, register_save.v7, register_save.v8, register_save.psp);

	thread_state.reg4=register_save.v1, thread_state.reg5=register_save.v2, thread_state.reg6=register_save.v3,
	thread_state.reg7=register_save.v4, thread_state.reg8=register_save.v5, thread_state.reg9=register_save.v6,
	thread_state.reg10=register_save.v7, thread_state.reg11=register_save.v8, thread_state.reg13=register_save.psp;

	sp_addressm = register_save.psp;

	for(i = 0; i < 8; i++){
		registerm[i] = *sp_addressm;
		sp_addressm++;
		printk("%s:%u ", registerm_name[i], registerm[i]);
	}
	printk("\n");

	thread_state.reg0=registerm[0], thread_state.reg1=registerm[1], thread_state.reg2=registerm[2],
	thread_state.reg3=registerm[3], thread_state.reg12=registerm[4], thread_state.reg14=registerm[5],
	thread_state.reg15=registerm[6], thread_state.reg16=registerm[7];
#endif

#ifdef CONFIG_X86
	thread_state.esp=register_save.esp;
	printf("threadA esp:%ld\n", thread_state.esp);
#endif

	thread_stk = threadA_data.stack_info;
	printf("k_thread var, stack start address:%lu stack size:%d\n", thread_stk.start, thread_stk.size);
	puts("");

	printf("K_THREAD_STACK_DEFINE, stack start %d\n", &threadA_stack_area->data);
	puts("");

#if 0
	// sp_addressm = thread_stk.start;	
	sp_addressm = thread_state.esp-8;

	for(i = 0; i < (thread_stk.size + thread_stk.start - thread_state.esp + 8); i++){
		printf("%u:%x\n", sp_addressm, *sp_addressm);
		thread_state.stack[i] = *sp_addressm;
		sp_addressm++;
	}
#endif

	// A simpler way to copy the stack
	memcpy(thread_state.stack, (char *)thread_stk.start, thread_stk.size);

	// thread_migration_client();

	k_thread_create(&threadB_data, threadB_stack_area,
			K_THREAD_STACK_SIZEOF(threadB_stack_area),
			thread_migration_client, NULL, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);

	// k_yield();

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

	// k_thread_create(&threadB_data, threadB_stack_area,
	// 		K_THREAD_STACK_SIZEOF(threadB_stack_area),
	// 		threadB, NULL, NULL, NULL,
	// 		PRIORITY, 0, K_NO_WAIT);

#if 1
	k_thread_create(&threadC_data, threadC_stack_area,
			K_THREAD_STACK_SIZEOF(threadC_stack_area),
			thread_migration_server, NULL, NULL, NULL,
			PRIORITY, 0, K_NO_WAIT);
#endif

}
