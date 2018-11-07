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

struct rt_cpu *rt_cpu_self(void)
{
    return &rt_cpus[rt_hw_cpu_id()];
}

struct rt_cpu *rt_cpu_index(int index)
{
    return &rt_cpus[index];
}

/**
 * This function will lock all cpus's scheduler and disable local irq.
 */
rt_base_t rt_cpus_lock(void)
{
    rt_base_t level;
    struct rt_cpu* pcpu;

    level = rt_hw_local_irq_disable();

    pcpu = rt_cpu_self();
    if (pcpu->current_thread != RT_NULL)
    {
        if (pcpu->current_thread->cpus_lock_nest++ == 0)
        {
            pcpu->current_thread->scheduler_lock_nest++;
            rt_hw_cpus_lock();
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
    struct rt_cpu* pcpu = rt_cpu_self();

    if (pcpu->current_thread != RT_NULL)
    {
        if (--pcpu->current_thread->cpus_lock_nest == 0)
        {
            pcpu->current_thread->scheduler_lock_nest--;
            rt_hw_cpus_unlock();
        }
    }
    rt_hw_local_irq_enable(level);
}
RTM_EXPORT(rt_cpus_unlock);

#endif
