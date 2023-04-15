#include <zephyr/kernel.h>
#include <kernel_internal.h>
#include <zephyr/debug/thread_analyzer.h>
#include <zephyr/debug/stack.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(thread_analyzer, CONFIG_THREAD_ANALYZER_LOG_LEVEL);

void thread_analyze_get(struct k_thread *cthread)
{
    // thread analyze
    k_thread_runtime_stats_t rt_stats_all;
    struct thread_analyzer_info info;

    struct k_thread *thread = (struct k_thread *)cthread;

    int ret;
    size_t size = thread->stack_info.size;
    char *name;

    size_t unused;
    int err;

    name = k_thread_name_get((k_tid_t)thread);

    err = k_thread_stack_space_get(thread, &unused);
    if (err) {
        LOG_WRN("%s: unable to get stack space (%d)", name, err);

        unused = 0;
    }

    info.name = name;
    info.stack_size = size;
    info.stack_used = size - unused;

    ret = 0;

    if (k_thread_runtime_stats_get(thread, &info.usage) != 0) {
        ret++;
    }

    if (k_thread_runtime_stats_all_get(&rt_stats_all) != 0) {
        ret++;
    }
    if (ret == 0) {
        info.utilization = (info.usage.execution_cycles * 100U) /
            rt_stats_all.execution_cycles;
    }

    size_t pcnt = (info.stack_used * 100U) / info.stack_size;

    printk("=========\n");
    printk("%s: STACK: unused %zu usage %zu / %zu (%zu %%); CPU: %u %%\n", info.name,
        info.stack_size - info.stack_used, info.stack_used, info.stack_size, pcnt, info.utilization);
    printk("=========\n");

    printk("execution_cycles: %llu; total_cycles: %llu; idle_cycles: %llu\n", info.usage.execution_cycles, 
        info.usage.total_cycles, info.usage.idle_cycles);

    printk("Current Frame: %llu; Longest Frame: %llu; Average Frame: %llu\n", info.usage.current_cycles, 
        info.usage.peak_cycles, info.usage.average_cycles);
}

