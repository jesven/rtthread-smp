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

/**
 * This function will lock all cpus's scheduler and disable local irq.
 */
rt_base_t rt_cpus_lock(void)
{
    rt_base_t level;
    level = rt_hw_local_irq_disable();
    if (rt_current_thread != RT_NULL)
    {
        if (rt_current_thread->kernel_lock_nest++ == 0)
        {
            rt_current_thread->scheduler_lock_nest++;
            rt_hw_kernel_lock();
        }
    }
    return level;
}
RTM_EXPORT(rt_cpus_lock);

/**
 * This function will restore all cpus's scheduler and restore local irq.
 */
void rt_cpus_unlock(rt_base_t level)
{
    if (rt_current_thread != RT_NULL)
    {
        if (--rt_current_thread->kernel_lock_nest == 0)
        {
            rt_current_thread->scheduler_lock_nest--;
            rt_hw_kernel_unlock();
        }
    }
    rt_hw_local_irq_enable(level);
}
RTM_EXPORT(rt_cpus_unlock);

#endif
