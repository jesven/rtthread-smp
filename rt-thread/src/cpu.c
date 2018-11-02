/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-10-30     Bernard      The first version
 */

#include <rtthread.h>
#include <rthw.h>

#ifdef RT_USING_SMP

struct rt_cpu rt_cpus[RT_CPUS_NR];
rt_hw_spinlock_t _cpus_lock;

/**
 * returns the current CPU id 
 */
int rt_cpu_id(void)
{
    return rt_cpuid();
}

struct rt_cpu *rt_cpu_self(void)
{
    return &rt_cpus[rt_cpu_id()];
}

void rt_cpus_lock(void)
{
    return ;
}

void rt_cpus_unlock(void)
{
    return ;
}

#endif
