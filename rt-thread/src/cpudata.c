/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 */

#include <rtthread.h>
#include <rthw.h>

#ifdef RT_HAVE_SMP

rt_precpu_t rt_percpu_data[RT_CPUS_NR];

#else

volatile rt_uint8_t rt_interrupt_nest;
rt_list_t rt_thread_priority_table[RT_THREAD_PRIORITY_MAX];
struct rt_thread *rt_current_thread;
rt_uint8_t rt_current_priority;
rt_tick_t rt_tick;

#endif /*RT_HAVE_SMP*/
