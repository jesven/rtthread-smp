/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-03-12     Bernard      first version
 * 2006-05-27     Bernard      add support for same priority thread schedule
 * 2006-08-10     Bernard      remove the last rt_schedule in rt_tick_increase
 * 2010-03-08     Bernard      remove rt_passed_second
 * 2010-05-20     Bernard      fix the tick exceeds the maximum limits
 * 2010-07-13     Bernard      fix rt_tick_from_millisecond issue found by kuronca
 * 2011-06-26     Bernard      add rt_tick_set function.
 */

#include <rthw.h>
#include <rtthread.h>

#ifndef RT_HAVE_SMP
static rt_tick_t rt_tick;
#endif

extern void rt_timer_check(void);

/**
 * This function will init system tick and set it to zero.
 * @ingroup SystemInit
 *
 * @deprecated since 1.1.0, this function does not need to be invoked
 * in the system initialization.
 */
void rt_system_tick_init(void)
{
}

/**
 * @addtogroup Clock
 */

/**@{*/

/**
 * This function will return current tick from operating system startup
 *
 * @return current tick
 */
rt_tick_t rt_tick_get(void)
{
#ifdef RT_HAVE_SMP
    /* return the global tick */
    return rt_percpu_data[0].rt_cpu_tick;
#else
    return rt_tick;
#endif /*RT_HAVE_SMP*/
}
RTM_EXPORT(rt_tick_get);

/**
 * This function will set current tick
 */
void rt_tick_set(rt_tick_t tick)
{
    rt_base_t level;

    level = rt_hw_interrupt_disable();
#ifdef RT_HAVE_SMP
    /* return the global tick */
    rt_percpu_data[0].rt_cpu_tick = tick;
#else
    rt_tick = tick;
#endif /*RT_HAVE_SMP*/
    rt_hw_interrupt_enable(level);
}

/**
 * This function will notify kernel there is one tick passed. Normally,
 * this function is invoked by clock ISR.
 */
void rt_tick_increase(void)
{
    struct rt_thread *thread;

    /* increase the global tick */
    ++ rt_tick;

    /* check time slice */
    thread = rt_thread_self();

    -- thread->remaining_tick;
    if (thread->remaining_tick == 0)
    {
        /* change to initialized tick */
        thread->remaining_tick = thread->init_tick;

        /* yield */
        rt_thread_yield();
    }

    /* check timer */
    rt_timer_check();
}

/**
 * This function will calculate the tick from millisecond.
 *
 * @param ms the specified millisecond
 *           - Negative Number wait forever
 *           - Zero not wait
 *           - Max 0x7fffffff
 *
 * @return the calculated tick
 */
int rt_tick_from_millisecond(rt_int32_t ms)
{
    int tick;

    if (ms < 0)
        tick = RT_WAITING_FOREVER;
    else
        tick = (RT_TICK_PER_SECOND * ms + 999) / 1000;

    /* return the calculated tick */
    return tick;
}
RTM_EXPORT(rt_tick_from_millisecond);

/**@}*/

