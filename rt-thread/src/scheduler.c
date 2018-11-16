/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-03-17     Bernard      the first version
 * 2006-04-28     Bernard      fix the scheduler algorthm
 * 2006-04-30     Bernard      add SCHEDULER_DEBUG
 * 2006-05-27     Bernard      fix the scheduler algorthm for same priority
 *                             thread schedule
 * 2006-06-04     Bernard      rewrite the scheduler algorithm
 * 2006-08-03     Bernard      add hook support
 * 2006-09-05     Bernard      add 32 priority level support
 * 2006-09-24     Bernard      add rt_system_scheduler_start function
 * 2009-09-16     Bernard      fix _rt_scheduler_stack_check
 * 2010-04-11     yi.qiu       add module feature
 * 2010-07-13     Bernard      fix the maximal number of rt_scheduler_lock_nest
 *                             issue found by kuronca
 * 2010-12-13     Bernard      add defunct list initialization even if not use heap.
 * 2011-05-10     Bernard      clean scheduler debug log.
 * 2013-12-21     Grissiom     add rt_critical_level
 */

#include <rtthread.h>
#include <rthw.h>

#ifdef RT_USING_SMP
rt_hw_spinlock_t _rt_critical_lock;
static void _rt_schedule_insert_thread(struct rt_thread *thread, int send_ipi);
#endif /*RT_USING_SMP*/

rt_list_t rt_thread_priority_table[RT_THREAD_PRIORITY_MAX];
rt_uint32_t rt_thread_ready_priority_group;
#if RT_THREAD_PRIORITY_MAX > 32
/* Maximum priority level, 256 */
rt_uint8_t rt_thread_ready_table[32];
#endif

#ifndef RT_USING_SMP
extern volatile rt_uint8_t rt_interrupt_nest;
static rt_int16_t rt_scheduler_lock_nest;
struct rt_thread *rt_current_thread;
rt_uint8_t rt_current_priority;
#endif /*RT_USING_SMP*/

rt_list_t rt_thread_defunct;

#ifdef RT_USING_HOOK
static void (*rt_scheduler_hook)(struct rt_thread *from, struct rt_thread *to);

/**
 * @addtogroup Hook
 */

/**@{*/

/**
 * This function will set a hook function, which will be invoked when thread
 * switch happens.
 *
 * @param hook the hook function
 */
void
rt_scheduler_sethook(void (*hook)(struct rt_thread *from, struct rt_thread *to))
{
    rt_scheduler_hook = hook;
}

/**@}*/
#endif

#ifdef RT_USING_OVERFLOW_CHECK
static void _rt_scheduler_stack_check(struct rt_thread *thread)
{
    RT_ASSERT(thread != RT_NULL);

    if (*((rt_uint8_t *)thread->stack_addr) != '#' ||
        (rt_ubase_t)thread->sp <= (rt_ubase_t)thread->stack_addr ||
        (rt_ubase_t)thread->sp >
        (rt_ubase_t)thread->stack_addr + (rt_ubase_t)thread->stack_size)
    {
        rt_ubase_t level;

        rt_kprintf("thread:%s stack overflow\n", thread->name);
#ifdef RT_USING_FINSH
        {
            extern long list_thread(void);
            list_thread();
        }
#endif
        level = rt_hw_interrupt_disable();
        while (level);
    }
    else if ((rt_ubase_t)thread->sp <= ((rt_ubase_t)thread->stack_addr + 32))
    {
        rt_kprintf("warning: %s stack is close to end of stack address.\n",
                   thread->name);
    }
}
#endif

#ifdef RT_USING_SMP
static void _ipi_handler(int vector, void *param)
{
    rt_schedule();
}
#endif

/**
 * @ingroup SystemInit
 * This function will initialize the system scheduler
 */
void rt_system_scheduler_init(void)
{
    register rt_base_t offset;
#ifdef RT_USING_SMP
    int cpu;
#endif /*RT_USING_SMP*/

#ifndef RT_USING_SMP
    rt_scheduler_lock_nest = 0;
#endif /*RT_USING_SMP*/

    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("start scheduler: max priority 0x%02x\n",
                                      RT_THREAD_PRIORITY_MAX));

    for (offset = 0; offset < RT_THREAD_PRIORITY_MAX; offset ++)
    {
        rt_list_init(&rt_thread_priority_table[offset]);
    }
#ifdef RT_USING_SMP
    for (cpu = 0; cpu < RT_CPUS_NR; cpu++)
    {
        struct rt_cpu *pcpu =  rt_cpu_index(cpu);
        for (offset = 0; offset < RT_THREAD_PRIORITY_MAX; offset ++)
        {
            rt_list_init(&pcpu->priority_table[offset]);
        }

        pcpu->irq_switch_flag = 0;
        pcpu->current_priority = RT_THREAD_PRIORITY_MAX - 1;
        pcpu->current_thread = RT_NULL;
        pcpu->priority_group = 0;

#if RT_THREAD_PRIORITY_MAX > 32
        rt_memset(pcpu->ready_table, 0, sizeof(pcpu->ready_table));
#endif
    }
    rt_hw_interrupt_install(RT_IPI_SCHEDULE, _ipi_handler, 0, "IPI_HANDLER");
#endif /*RT_USING_SMP*/

    /* initialize ready priority group */
    rt_thread_ready_priority_group = 0;

#if RT_THREAD_PRIORITY_MAX > 32
    /* initialize ready table */
    rt_memset(rt_thread_ready_table, 0, sizeof(rt_thread_ready_table));
#endif

    /* initialize thread defunct */
    rt_list_init(&rt_thread_defunct);
}

/**
 * This function find the highest priority thread struct
 * note:
 *      This function must be invoked with local irq is disabled
 * return:
 *      the point to the highest priority thread struct
 */
#ifdef RT_USING_SMP
static struct rt_thread* _get_highest_priority_thread(rt_ubase_t *highest_prio)
{
    register struct rt_thread *highest_priority_thread;
    register rt_ubase_t highest_ready_priority, local_highest_ready_priority;
    struct rt_cpu* pcpu = rt_cpu_self();

#if RT_THREAD_PRIORITY_MAX > 32
    register rt_ubase_t number;

    if (rt_thread_ready_priority_group == 0 && pcpu->priority_group == 0)
    {
        *highest_prio = pcpu->current_thread->current_priority;
        /* only local IDLE is readly */
        return pcpu->current_thread;
    }

    number = __rt_ffs(rt_thread_ready_priority_group) - 1;
    highest_ready_priority = (number << 3) + __rt_ffs(rt_thread_ready_table[number]) - 1;
    number = __rt_ffs(pcpu->priority_group) - 1;
    local_highest_ready_priority = (number << 3) + __rt_ffs(pcpu->ready_table[number]) - 1;
#else
    highest_ready_priority = __rt_ffs(rt_thread_ready_priority_group) - 1;
    local_highest_ready_priority = __rt_ffs(pcpu->priority_group) - 1;
#endif

    /* get highest ready priority thread */
    if (highest_ready_priority < local_highest_ready_priority)
    {
        *highest_prio = highest_ready_priority;
        highest_priority_thread = rt_list_entry(rt_thread_priority_table[highest_ready_priority].next,
                                  struct rt_thread,
                                  tlist);
    }
    else
    {
        *highest_prio = local_highest_ready_priority;
        highest_priority_thread = rt_list_entry(pcpu->priority_table[local_highest_ready_priority].next,
                                  struct rt_thread,
                                  tlist);
    }

    return highest_priority_thread;
}
#else
static struct rt_thread* _get_highest_priority_thread(rt_ubase_t *highest_prio)
{
    register struct rt_thread *highest_priority_thread;
    register rt_ubase_t highest_ready_priority;

#if RT_THREAD_PRIORITY_MAX > 32
    register rt_ubase_t number;

    number = __rt_ffs(rt_thread_ready_priority_group) - 1;
    highest_ready_priority = (number << 3) + __rt_ffs(rt_thread_ready_table[number]) - 1;
#else
    highest_ready_priority = __rt_ffs(rt_thread_ready_priority_group) - 1;
#endif

    /* get highest ready priority thread */
    highest_priority_thread = rt_list_entry(rt_thread_priority_table[highest_ready_priority].next,
                              struct rt_thread,
                              tlist);

    *highest_prio = highest_ready_priority;

    return highest_priority_thread;
}
#endif

/**
 * @ingroup SystemInit
 * This function will startup scheduler. It will select one thread
 * with the highest priority level, then switch to it.
 */
void rt_system_scheduler_start(void)
{
    register struct rt_thread *to_thread;
    rt_ubase_t highest_ready_priority;

    to_thread = _get_highest_priority_thread(&highest_ready_priority);

#ifdef RT_USING_SMP
    to_thread->oncpu = rt_hw_cpu_id();
#else
    rt_current_thread = to_thread;
#endif /*RT_USING_SMP*/

    rt_schedule_remove_thread(to_thread);

    /* switch to new thread */
#ifdef RT_USING_SMP
    rt_hw_context_switch_to((rt_uint32_t)&to_thread->sp, to_thread);
#else
    rt_hw_context_switch_to((rt_uint32_t)&to_thread->sp);
#endif /*RT_USING_SMP*/

    /* never come back */
}

/**
 * @addtogroup Thread
 */

/**@{*/

/**
 * This function will perform one schedule. It will select one thread
 * with the highest priority level, then switch to it.
 */
#ifdef RT_USING_SMP
void rt_schedule(void)
{
    rt_base_t level;
    struct rt_thread *to_thread;
    struct rt_thread *from_thread;
    struct rt_cpu    *pcpu;
    struct rt_thread *current_thread;

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    pcpu = rt_cpu_self();
    current_thread = pcpu->current_thread;

    if (pcpu->irq_nest)
    {
        pcpu->irq_switch_flag = 1;

        /* enable interrupt */
        rt_hw_interrupt_enable(level);
        return;
    }

    /* check the scheduler is enabled or not */
    if (current_thread->scheduler_lock_nest == 1)
    {
        rt_ubase_t highest_ready_priority;

        if (rt_thread_ready_priority_group != 0 || pcpu->priority_group != 0)
        {
            current_thread->oncpu = RT_DETACH_CPU;
            if ((current_thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_READY)
            {
                /* insert to ready list and no ipi */
                _rt_schedule_insert_thread(current_thread, 0);
            }

            to_thread = _get_highest_priority_thread(&highest_ready_priority);

            if (to_thread != current_thread)
            {
                /* if the destination thread is not the same as current thread */
                pcpu->current_priority = (rt_uint8_t)highest_ready_priority;
                from_thread            = current_thread;

                RT_OBJECT_HOOK_CALL(rt_scheduler_hook, (from_thread, to_thread));

                to_thread->oncpu = rt_hw_cpu_id();
                rt_schedule_remove_thread(to_thread);

                if ((current_thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_READY)
                {
                    if (current_thread->bind_cpu == RT_CPUS_NR)
                    {
                        int cpu_id;
                        rt_uint32_t cpu_mask;

                        cpu_id = rt_hw_cpu_id();
                        cpu_mask = RT_CPU_MASK ^ (1 << cpu_id);
                        rt_hw_ipi_send(RT_IPI_SCHEDULE, cpu_mask);
                    }
                }

                /* switch to new thread */
                RT_DEBUG_LOG(RT_DEBUG_SCHEDULER,
                        ("[%d]switch to priority#%d "
                         "thread:%.*s(sp:0x%08x), "
                         "from thread:%.*s(sp: 0x%08x)\n",
                         pcpu->irq_nest, highest_ready_priority,
                         RT_NAME_MAX, to_thread->name, to_thread->sp,
                         RT_NAME_MAX, from_thread->name, from_thread->sp));

#ifdef RT_USING_OVERFLOW_CHECK
                _rt_scheduler_stack_check(to_thread);
#endif

                {
                    extern void rt_thread_handle_sig(rt_bool_t clean_state);

                    rt_hw_context_switch((rt_ubase_t)&from_thread->sp,
                                         (rt_ubase_t)&to_thread->sp, to_thread);

                    /* enable interrupt */
                    rt_hw_interrupt_enable(level);

#ifdef RT_USING_SIGNALS
                    /* check signal status */
                    rt_thread_handle_sig(RT_TRUE);
#endif
                }
            }
            else
            {
                current_thread->oncpu = rt_hw_cpu_id();
                rt_schedule_remove_thread(current_thread);
                /* enable interrupt */
                rt_hw_interrupt_enable(level);
            }
        }
        else
        {
            /* enable interrupt */
            rt_hw_interrupt_enable(level);
        }
    }
    else
    {
        /* enable interrupt */
        rt_hw_interrupt_enable(level);
    }
}
#else
void rt_schedule(void)
{
    rt_base_t level;
    struct rt_thread *to_thread;
    struct rt_thread *from_thread;

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    /* check the scheduler is enabled or not */
    if (rt_scheduler_lock_nest == 0)
    {
        rt_ubase_t highest_ready_priority;

        if (rt_thread_ready_priority_group != 0)
        {
            if ((rt_current_thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_READY)
            {
                rt_schedule_insert_thread(rt_current_thread);
            }

            to_thread = _get_highest_priority_thread(&highest_ready_priority);

            if (to_thread != rt_current_thread)
            {
                /* if the destination thread is not the same as current thread */
                rt_current_priority = (rt_uint8_t)highest_ready_priority;
                from_thread         = rt_current_thread;
                rt_current_thread   = to_thread;

                RT_OBJECT_HOOK_CALL(rt_scheduler_hook, (from_thread, to_thread));

                rt_schedule_remove_thread(to_thread);

                /* switch to new thread */
                RT_DEBUG_LOG(RT_DEBUG_SCHEDULER,
                        ("[%d]switch to priority#%d "
                         "thread:%.*s(sp:0x%p), "
                         "from thread:%.*s(sp: 0x%p)\n",
                         rt_interrupt_nest, highest_ready_priority,
                         RT_NAME_MAX, to_thread->name, to_thread->sp,
                         RT_NAME_MAX, from_thread->name, from_thread->sp));

#ifdef RT_USING_OVERFLOW_CHECK
                _rt_scheduler_stack_check(to_thread);
#endif

                if (rt_interrupt_nest == 0)
                {
                    extern void rt_thread_handle_sig(rt_bool_t clean_state);

                    rt_hw_context_switch((rt_ubase_t)&from_thread->sp,
                            (rt_ubase_t)&to_thread->sp);

                    /* enable interrupt */
                    rt_hw_interrupt_enable(level);

#ifdef RT_USING_SIGNALS
                    /* check signal status */
                    rt_thread_handle_sig(RT_TRUE);
#endif
                }
                else
                {
                    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("switch in interrupt\n"));

                    rt_hw_context_switch_interrupt((rt_ubase_t)&from_thread->sp,
                            (rt_ubase_t)&to_thread->sp);
                    /* enable interrupt */
                    rt_hw_interrupt_enable(level);
                }
            }
            else
            {
                rt_schedule_remove_thread(rt_current_thread);
                /* enable interrupt */
                rt_hw_interrupt_enable(level);
            }
        }
        else
        {
            /* enable interrupt */
            rt_hw_interrupt_enable(level);
        }
    }
    else
    {
        /* enable interrupt */
        rt_hw_interrupt_enable(level);
    }
}
#endif /*RT_USING_SMP*/

/**
 * This function checks if a schedule is needed in interrupt. If yes, it will select one thread
 * with the highest priority level, add save this information.
 */
#ifdef RT_USING_SMP
void rt_interrupt_check_schedule(void)
{
    struct rt_thread *to_thread;
    struct rt_thread *from_thread;
    struct rt_cpu* pcpu = rt_cpu_self();
    struct rt_thread *current_thread = pcpu->current_thread;

    if (pcpu->irq_switch_flag == 0)
    {
        return;
    }

    if (current_thread->scheduler_lock_nest == 1 && pcpu->irq_nest == 0)
    {
        rt_ubase_t highest_ready_priority;

        pcpu->irq_switch_flag = 0;

        if (rt_thread_ready_priority_group != 0 || pcpu->priority_group != 0)
        {
            current_thread->oncpu = RT_DETACH_CPU;
            if ((current_thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_READY)
            {
                /* insert to ready list and no ipi */
                _rt_schedule_insert_thread(current_thread, 0);
            }

            to_thread = _get_highest_priority_thread(&highest_ready_priority);

            if (to_thread != current_thread)
            {
                /* if the destination thread is not the same as current thread */
                pcpu->current_priority = (rt_uint8_t)highest_ready_priority;
                from_thread         = current_thread;

                RT_OBJECT_HOOK_CALL(rt_scheduler_hook, (from_thread, to_thread));
                to_thread->oncpu = rt_hw_cpu_id();
                rt_schedule_remove_thread(to_thread);

                if ((current_thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_READY)
                {
                    if (current_thread->bind_cpu == RT_CPUS_NR)
                    {
                        int cpu_id;
                        rt_uint32_t cpu_mask;

                        cpu_id = rt_hw_cpu_id();
                        cpu_mask = RT_CPU_MASK ^ (1 << cpu_id);
                        rt_hw_ipi_send(RT_IPI_SCHEDULE, cpu_mask);
                    }
                }

#ifdef RT_USING_OVERFLOW_CHECK
                _rt_scheduler_stack_check(to_thread);
#endif
                RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("switch in interrupt\n"));

                rt_hw_context_switch_interrupt((rt_ubase_t)&from_thread->sp,
                        (rt_ubase_t)&to_thread->sp, to_thread);
            }
            else
            {
                current_thread->oncpu = rt_hw_cpu_id();
                rt_schedule_remove_thread(current_thread);
            }
        }
    }
}
#endif /*RT_USING_SMP*/

/*
 * This function will insert a thread to system ready queue. The state of
 * thread will be set as READY and remove from suspend queue.
 *
 * @param thread the thread to be inserted
 * @param send_ipi need notify to other CPUs
 * @note Please do not invoke this function in user application.
 */
#ifdef RT_USING_SMP
static void _rt_schedule_insert_thread(struct rt_thread *thread, int send_ipi)
{
    int cpu_id;
    register rt_base_t temp;
    rt_uint32_t cpu_mask;

    RT_ASSERT(thread != RT_NULL);

    /* disable interrupt */
    temp = rt_hw_interrupt_disable();

    /* change stat */
    thread->stat = RT_THREAD_READY | (thread->stat & ~RT_THREAD_STAT_MASK);

    if (thread->oncpu != RT_DETACH_CPU)
    {
        rt_hw_interrupt_enable(temp);
        return;
    }

    /* insert thread to ready list */
    if (thread->bind_cpu == RT_CPUS_NR)
    {
        rt_list_insert_before(&(rt_thread_priority_table[thread->current_priority]),
                              &(thread->tlist));
        if (send_ipi)
        {
            cpu_id = rt_hw_cpu_id();
            cpu_mask = RT_CPU_MASK ^ (1 << cpu_id);
            rt_hw_ipi_send(RT_IPI_SCHEDULE, cpu_mask);
        }
    }
    else
    {
        rt_list_insert_before(&(rt_cpu_index(thread->bind_cpu)->priority_table[thread->current_priority]),
                              &(thread->tlist));
        if (send_ipi)
        {
            cpu_id = rt_hw_cpu_id();
            if (cpu_id != thread->bind_cpu)
            {
                cpu_mask = 1 << thread->bind_cpu;
                rt_hw_ipi_send(RT_IPI_SCHEDULE, cpu_mask);
            }
        }
    }

    /* set priority mask */
#if RT_THREAD_PRIORITY_MAX <= 32
    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("insert thread[%.*s], the priority: %d\n",
                                      RT_NAME_MAX, thread->name, thread->current_priority));
#else
    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER,
                 ("insert thread[%.*s], the priority: %d 0x%x %d\n",
                  RT_NAME_MAX,
                  thread->name,
                  thread->number,
                  thread->number_mask,
                  thread->high_mask));
#endif

    if (thread->bind_cpu == RT_CPUS_NR)
    {
#if RT_THREAD_PRIORITY_MAX > 32
        rt_thread_ready_table[thread->number] |= thread->high_mask;
#endif
        rt_thread_ready_priority_group |= thread->number_mask;
    }
    else
    {
        struct rt_cpu *pcpu = rt_cpu_index(thread->bind_cpu);
#if RT_THREAD_PRIORITY_MAX > 32
        pcpu->ready_table[thread->number] |= thread->high_mask;
#endif
        pcpu->priority_group |= thread->number_mask;
    }

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);
}

/*
 * This function call _rt_schedule_insert_thread with needs notify other CPUs
 *
 * @param thread the thread to be inserted
 * @note Please do not invoke this function in user application.
 */
void rt_schedule_insert_thread(struct rt_thread *thread)
{
    _rt_schedule_insert_thread(thread, 1);
}
#else
/*
 * This function will insert a thread to system ready queue. The state of
 * thread will be set as READY and remove from suspend queue.
 *
 * @param thread the thread to be inserted
 * @note Please do not invoke this function in user application.
 */
void rt_schedule_insert_thread(struct rt_thread *thread)
{
    register rt_base_t temp;

    RT_ASSERT(thread != RT_NULL);

    /* disable interrupt */
    temp = rt_hw_interrupt_disable();

    /* change stat */
    thread->stat = RT_THREAD_READY | (thread->stat & ~RT_THREAD_STAT_MASK);

    /* insert thread to ready list */
    rt_list_insert_before(&(rt_thread_priority_table[thread->current_priority]),
                          &(thread->tlist));

    /* set priority mask */
#if RT_THREAD_PRIORITY_MAX <= 32
    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("insert thread[%.*s], the priority: %d\n",
                                      RT_NAME_MAX, thread->name, thread->current_priority));
#else
    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER,
                 ("insert thread[%.*s], the priority: %d 0x%x %d\n",
                  RT_NAME_MAX,
                  thread->name,
                  thread->number,
                  thread->number_mask,
                  thread->high_mask));
#endif

#if RT_THREAD_PRIORITY_MAX > 32
    rt_thread_ready_table[thread->number] |= thread->high_mask;
#endif
    rt_thread_ready_priority_group |= thread->number_mask;

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);
}
#endif /*RT_USING_SMP*/

/*
 * This function will remove a thread from system ready queue.
 *
 * @param thread the thread to be removed
 *
 * @note Please do not invoke this function in user application.
 */
#ifdef RT_USING_SMP
void rt_schedule_remove_thread(struct rt_thread *thread)
{
    register rt_base_t temp;

    RT_ASSERT(thread != RT_NULL);

    /* disable interrupt */
    temp = rt_hw_interrupt_disable();

#if RT_THREAD_PRIORITY_MAX <= 32
    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("remove thread[%.*s], the priority: %d\n",
                                      RT_NAME_MAX, thread->name,
                                      thread->current_priority));
#else
    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER,
                 ("remove thread[%.*s], the priority: %d 0x%x %d\n",
                  RT_NAME_MAX,
                  thread->name,
                  thread->number,
                  thread->number_mask,
                  thread->high_mask));
#endif

    /* remove thread from ready list */
    rt_list_remove(&(thread->tlist));
    if (thread->bind_cpu == RT_CPUS_NR)
    {
        if (rt_list_isempty(&(rt_thread_priority_table[thread->current_priority])))
        {
#if RT_THREAD_PRIORITY_MAX > 32
            rt_thread_ready_table[thread->number] &= ~thread->high_mask;
            if (rt_thread_ready_table[thread->number] == 0)
            {
                rt_thread_ready_priority_group &= ~thread->number_mask;
            }
#else
            rt_thread_ready_priority_group &= ~thread->number_mask;
#endif
        }
    }
    else
    {
        struct rt_cpu *pcpu = rt_cpu_index(thread->bind_cpu);
        if (rt_list_isempty(&(pcpu->priority_table[thread->current_priority])))
        {
#if RT_THREAD_PRIORITY_MAX > 32
            pcpu->ready_table[thread->number] &= ~thread->high_mask;
            if (rt_thread_ready_table[thread->number] == 0)
            {
                pcpu->priority_group &= ~thread->number_mask;
            }
#else
            pcpu->priority_group &= ~thread->number_mask;
#endif
        }
    }

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);
}
#else
void rt_schedule_remove_thread(struct rt_thread *thread)
{
    register rt_base_t temp;

    RT_ASSERT(thread != RT_NULL);

    /* disable interrupt */
    temp = rt_hw_interrupt_disable();

#if RT_THREAD_PRIORITY_MAX <= 32
    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("remove thread[%.*s], the priority: %d\n",
                                      RT_NAME_MAX, thread->name,
                                      thread->current_priority));
#else
    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER,
                 ("remove thread[%.*s], the priority: %d 0x%x %d\n",
                  RT_NAME_MAX,
                  thread->name,
                  thread->number,
                  thread->number_mask,
                  thread->high_mask));
#endif

    /* remove thread from ready list */
    rt_list_remove(&(thread->tlist));
    if (rt_list_isempty(&(rt_thread_priority_table[thread->current_priority])))
    {
#if RT_THREAD_PRIORITY_MAX > 32
        rt_thread_ready_table[thread->number] &= ~thread->high_mask;
        if (rt_thread_ready_table[thread->number] == 0)
        {
            rt_thread_ready_priority_group &= ~thread->number_mask;
        }
#else
        rt_thread_ready_priority_group &= ~thread->number_mask;
#endif
    }

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);
}
#endif /*RT_USING_SMP*/

/**
 * This function will lock the thread scheduler.
 */
#ifdef RT_USING_SMP
void rt_enter_critical(void)
{
    register rt_base_t level;
    struct rt_thread *current_thread;

    /* disable interrupt */
    level = rt_hw_local_irq_disable();

    current_thread = rt_cpu_self()->current_thread;
    /*
     * the maximal number of nest is RT_UINT16_MAX, which is big
     * enough and does not check here
     */

    /* lock scheduler for all cpus */
    if (current_thread->scheduler_lock_nest == !!current_thread->cpus_lock_nest)
    {
        rt_hw_spin_lock(&_rt_critical_lock);
    }

    /* lock scheduler for local cpu */
    current_thread->scheduler_lock_nest ++;

    /* enable interrupt */
    rt_hw_local_irq_enable(level);
}
#else
void rt_enter_critical(void)
{
    register rt_base_t level;

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    /*
     * the maximal number of nest is RT_UINT16_MAX, which is big
     * enough and does not check here
     */
    rt_scheduler_lock_nest ++;

    /* enable interrupt */
    rt_hw_interrupt_enable(level);
}
#endif /*RT_USING_SMP*/
RTM_EXPORT(rt_enter_critical);

/**
 * This function will unlock the thread scheduler.
 */
#ifdef RT_USING_SMP
void rt_exit_critical(void)
{
    register rt_base_t level;
    struct rt_thread *current_thread;

    /* disable interrupt */
    level = rt_hw_local_irq_disable();

    current_thread = rt_cpu_self()->current_thread;

    current_thread->scheduler_lock_nest --;

    if (current_thread->scheduler_lock_nest == !!current_thread->cpus_lock_nest)
    {
        rt_hw_spin_unlock(&_rt_critical_lock);
    }

    if (current_thread->scheduler_lock_nest <= 0)
    {
        current_thread->scheduler_lock_nest = 0;
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
#else
void rt_exit_critical(void)
{
    register rt_base_t level;

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    rt_scheduler_lock_nest --;

    if (rt_scheduler_lock_nest <= 0)
    {
        rt_scheduler_lock_nest = 0;
        /* enable interrupt */
        rt_hw_interrupt_enable(level);

        rt_schedule();
    }
    else
    {
        /* enable interrupt */
        rt_hw_interrupt_enable(level);
    }
}
#endif /*RT_USING_SMP*/
RTM_EXPORT(rt_exit_critical);

/**
 * Get the scheduler lock level
 *
 * @return the level of the scheduler lock. 0 means unlocked.
 */
rt_uint16_t rt_critical_level(void)
{
#ifdef RT_USING_SMP
    struct rt_thread *current_thread = rt_cpu_self()->current_thread;

    return current_thread->scheduler_lock_nest;
#else
	return rt_scheduler_lock_nest;
#endif /*RT_USING_SMP*/
}
RTM_EXPORT(rt_critical_level);

/**@}*/
