#include <rtthread.h>
#include <rthw.h>

#ifdef RT_HAVE_SMP

/**
 * This function will lock the thread scheduler and disable local irq.
 */
rt_base_t rt_hw_interrupt_disable(void)
{
    rt_base_t level;
    level = rt_hw_local_irq_disable();
    if (rt_current_thread != RT_NULL)
    {
        if (rt_current_thread->kernel_lock_nest++ == 0)
        {
            rt_current_thread->scheduler_lock_nest++;
            rt_kernel_lock();
        }
    }
    return level;
}
RTM_EXPORT(rt_hw_interrupt_disable);

/**
 * This function will restore the thread scheduler and restore local irq.
 */
void rt_hw_interrupt_enable(rt_base_t level)
{
    if (rt_current_thread != RT_NULL)
    {
        if (--rt_current_thread->kernel_lock_nest == 0)
        {
            rt_current_thread->scheduler_lock_nest--;
            rt_kernel_unlock();
        }
    }
    rt_hw_local_irq_enable(level);
}
RTM_EXPORT(rt_hw_interrupt_enable);

/**
 * This function is the version of rt_hw_interrupt_disable in interrupt.
 */
void rt_hw_interrupt_disable_int(void)
{
    if (rt_current_thread != RT_NULL)
    {
        rt_current_thread->kernel_lock_nest++;
        rt_current_thread->scheduler_lock_nest++;
        rt_kernel_lock();
    }
}
RTM_EXPORT(rt_hw_interrupt_disable_int);

/**
 * This function is the version of rt_hw_interrupt_enable in interrupt.
 */
void rt_hw_interrupt_enable_int(void)
{
    if (rt_current_thread != RT_NULL)
    {
        rt_kernel_unlock();
        rt_current_thread->kernel_lock_nest--;
        rt_current_thread->scheduler_lock_nest--;
    }
}
RTM_EXPORT(rt_hw_interrupt_enable_int);

/**
 * This function will lock the thread scheduler.
 */
void rt_enter_critical(void)
{
    register rt_base_t level;

    /* disable interrupt */
    level = rt_hw_local_irq_disable();

    /*
     * the maximal number of nest is RT_UINT16_MAX, which is big
     * enough and does not check here
     */

    if (rt_current_thread->scheduler_lock_nest == rt_current_thread->kernel_lock_nest)
    {
        rt_scheduler_lock();
    }
    rt_current_thread->scheduler_lock_nest ++;

    /* enable interrupt */
    rt_hw_local_irq_enable(level);
}
RTM_EXPORT(rt_enter_critical);

/**
 * This function will unlock the thread scheduler.
 */
void rt_exit_critical(void)
{
    register rt_base_t level;

    /* disable interrupt */
    level = rt_hw_local_irq_disable();

    rt_current_thread->scheduler_lock_nest --;

    if (rt_current_thread->scheduler_lock_nest == rt_current_thread->kernel_lock_nest)
    {
        rt_scheduler_unlock();
    }

    if (rt_current_thread->scheduler_lock_nest <= 0)
    {
        rt_current_thread->scheduler_lock_nest = 0;
        /* enable interrupt */
        rt_hw_local_irq_enable(level);

        rt_schedule();
    }
    else
    {
        /* enable interrupt */
        rt_hw_local_irq_enable(level);
    }
}
RTM_EXPORT(rt_exit_critical);

#endif /*RT_HAVE_SMP*/
