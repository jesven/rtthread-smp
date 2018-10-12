#include "rtthread.h"
#include "rthw.h"
#include "spinlock.h"

#ifdef RT_HAVE_SMP
raw_spinlock_t _rt_kernel_lock = {.slock = 0};

rt_base_t rt_hw_interrupt_disable(void)
{
    rt_base_t level;
    level = rt_local_irq_disable();
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
    rt_local_irq_enable(level);
}

void rt_hw_interrupt_disable_int(void)
{
    if (rt_current_thread != RT_NULL)
    {
        rt_current_thread->kernel_lock_nest++;
        rt_current_thread->scheduler_lock_nest++;
        rt_kernel_lock();
    }
}

void rt_hw_interrupt_enable_int(void)
{
    if (rt_current_thread != RT_NULL)
    {
        rt_kernel_unlock();
        rt_current_thread->kernel_lock_nest--;
        rt_current_thread->scheduler_lock_nest--;
    }
}
#endif /*RT_HAVE_SMP*/
