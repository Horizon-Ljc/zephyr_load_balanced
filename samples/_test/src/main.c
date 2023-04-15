// #include <stdio.h>
// #include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys_clock.h>
// #include <zephyr/debug/thread_analyzer.h>
// #include <zephyr.h>
// #include <zephyr/net/socket.h>

// #include <zephyr/random/rand32.h>

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

/* delay between greetings (in ms) */
#define SLEEPTIME 500


K_THREAD_STACK_DEFINE(threadA_stack_area, STACKSIZE);
static struct k_thread threadA_data;

K_THREAD_STACK_DEFINE(threadB_stack_area, STACKSIZE);
static struct k_thread threadB_data;


void threadA(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	int cnt=1;
	while(1){
		// printk("threadA =========, %d\n", cnt++);	

		thread_analyze_get(&threadB_data);

		k_busy_wait(100000);
		k_msleep(SLEEPTIME);
	}

}

void threadB(void *dummy1, void *dummy2, void *dummy3){
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	while(1){
		// printk("threadB =========\n");

		// thread_analyzer_run();
		// thread_analyzer_print();

		
		thread_analyze_get(&threadA_data);

		k_busy_wait(100000);
		k_msleep(SLEEPTIME);

	}

}

void main(void)
{

	k_thread_create(&threadA_data, threadA_stack_area,
			K_THREAD_STACK_SIZEOF(threadA_stack_area),
			threadA, NULL, NULL, NULL,
			PRIORITY, 0, K_FOREVER);
	k_thread_name_set(&threadA_data, "thread_a");

	k_thread_create(&threadB_data, threadB_stack_area,
			K_THREAD_STACK_SIZEOF(threadB_stack_area),
			threadB, NULL, NULL, NULL,
			PRIORITY, 0, K_FOREVER);
	k_thread_name_set(&threadB_data, "thread_b");

	k_thread_start(&threadA_data);
	k_thread_start(&threadB_data);

#ifdef __aarch64__
	printk("aarch64\n");
#endif 

}
