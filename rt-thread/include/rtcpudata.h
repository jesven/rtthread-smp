/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * File      : rtcpudata.h
 *
 * Change Logs:
 * Date           Author       Notes
 */

#ifndef  __RTCPUDATA_H__
#define  __RTCPUDATA_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RT_HAVE_SMP

#define RT_CPU_MASK ((1 << RT_CPUS_NR) - 1)

typedef struct {
    rt_uint8_t rt_cpu_interrupt_nest;
    rt_list_t rt_cpu_thread_priority_table[RT_THREAD_PRIORITY_MAX];
    struct rt_thread *rt_cpu_current_thread;
    rt_uint8_t rt_cpu_current_priority;
#if RT_THREAD_PRIORITY_MAX > 32
    rt_uint32_t rt_cpu_thread_ready_priority_group;
    rt_uint8_t rt_cpu_thread_ready_table[32];
#else
    rt_uint32_t rt_cpu_thread_ready_priority_group;
#endif
    rt_tick_t rt_cpu_tick;
} rt_precpu_t;

extern rt_precpu_t rt_percpu_data[RT_CPUS_NR];

#define rt_interrupt_nest rt_percpu_data[rt_cpuid()].rt_cpu_interrupt_nest

#define rt_thread_priority_table rt_percpu_data[rt_cpuid()].rt_cpu_thread_priority_table

#define rt_current_thread rt_percpu_data[rt_cpuid()].rt_cpu_current_thread

#define rt_current_priority rt_percpu_data[rt_cpuid()].rt_cpu_current_priority

#if RT_THREAD_PRIORITY_MAX > 32
/* Maximum priority level, 256 */

#define rt_thread_ready_priority_group rt_percpu_data[rt_cpuid()].rt_cpu_thread_ready_priority_group
#define rt_thread_ready_table rt_percpu_data[rt_cpuid()].rt_cpu_thread_ready_table
#else
/* Maximum priority level, 32 */
#define rt_thread_ready_priority_group rt_percpu_data[rt_cpuid()].rt_cpu_thread_ready_priority_group
#endif

#define rt_tick rt_percpu_data[rt_cpuid()].rt_cpu_tick

#else

extern  struct rt_thread *rt_current_thread;

#endif /*RT_HAVE_SMP*/

#ifdef __cplusplus
}
#endif

#endif  /*__RTCPUDATA_H__*/
