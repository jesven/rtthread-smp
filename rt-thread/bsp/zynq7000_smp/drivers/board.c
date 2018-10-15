/*
 * File      : board.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2012, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2012-11-20     Bernard    the first version
 */

#include <rthw.h>
#include <rtthread.h>

#include "board.h"

typedef struct
{
    volatile rt_uint32_t LOAD;
    volatile rt_uint32_t COUNTER;
    volatile rt_uint32_t CONTROL;
    volatile rt_uint32_t ISR;
} ptimer_reg_t;

/* Values for control register */
#define PRIVATE_TIMER_CONTROL_PRESCALER_MASK     0x0000FF00
#define PRIVATE_TIMER_CONTROL_PRESCALER_SHIFT    8
#define PRIVATE_TIMER_CONTROL_IRQ_ENABLE_MASK    0x00000004
#define PRIVATE_TIMER_CONTROL_AUTO_RELOAD_MASK   0x00000002
#define PRIVATE_TIMER_CONTROL_ENABLE_MASK        0x00000001

/* Values for ISR register */
#define PRIVATE_TIMER_ISR_EVENT_FLAG_MASK        0x00000001

#define PRIVATE_TIMER_BASE            0xF8F00600
#define PRIVATE_TIMER                ((ptimer_reg_t*)PRIVATE_TIMER_BASE)

static void rt_hw_timer_isr(int vector, void *param)
{
    rt_tick_increase();
    /* clear interrupt */
    PRIVATE_TIMER->ISR = PRIVATE_TIMER_ISR_EVENT_FLAG_MASK;
}

int rt_hw_timer_init(void)
{
    PRIVATE_TIMER->CONTROL &= ~PRIVATE_TIMER_CONTROL_ENABLE_MASK;
    {
        /* Clear the prescaler. */
        rt_uint32_t ctrl = PRIVATE_TIMER->CONTROL;
        ctrl &= ~PRIVATE_TIMER_CONTROL_PRESCALER_MASK;
        PRIVATE_TIMER->CONTROL = ctrl;
    }
    /* The processor timer is always clocked at 1/2 CPU frequency(CPU_3x2x). */
    PRIVATE_TIMER->COUNTER = APU_FREQ/2/RT_TICK_PER_SECOND;
    /* Set reload value. */
    PRIVATE_TIMER->LOAD = APU_FREQ/2/RT_TICK_PER_SECOND;
    PRIVATE_TIMER->CONTROL |= PRIVATE_TIMER_CONTROL_AUTO_RELOAD_MASK;

    PRIVATE_TIMER->CONTROL |= PRIVATE_TIMER_CONTROL_IRQ_ENABLE_MASK;
    PRIVATE_TIMER->ISR = PRIVATE_TIMER_ISR_EVENT_FLAG_MASK;

    rt_hw_interrupt_install(IRQ_Zynq7000_PTIMER, rt_hw_timer_isr, RT_NULL, "tick");
    rt_hw_interrupt_umask(IRQ_Zynq7000_PTIMER);

    PRIVATE_TIMER->CONTROL |= PRIVATE_TIMER_CONTROL_ENABLE_MASK;

    return 0;
}
INIT_BOARD_EXPORT(rt_hw_timer_init);

/**
 * This function will initialize beaglebone board
 */
void rt_hw_board_init(void)
{
    /* initialzie hardware interrupt */
    rt_hw_interrupt_init();
    rt_system_heap_init(HEAP_BEGIN, HEAP_END);

    rt_components_board_init();
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
}

extern void set_secondy_cpu_boot_address(void);
//extern void flush_cache_all(void);
extern void dist_ipi_send(int irq, int cpu);

void secondy_cpu_up(void)
{
    set_secondy_cpu_boot_address();
//    flush_cache_all();
    __asm__ volatile ("dsb":::"memory");
//    dist_ipi_send(0, 1);
}

#include "gic.h"
#include "stdint.h"

typedef struct
{
    volatile rt_uint32_t COUNTER_LOW;
    volatile rt_uint32_t COUNTER_HIGH;
    volatile rt_uint32_t CONTROL;
    volatile rt_uint32_t ISR_STATUS;
    volatile rt_uint32_t COMPARE_LOW;
    volatile rt_uint32_t COMPARE_HIGH;
    volatile rt_uint32_t AUTO_INC;
} gtimer_reg_t;

#define GLOBAL_TIMER                ((gtimer_reg_t*)Zynq7000_TIMER_GLOBAL_BASE)

#define GT_CTRL_TIMER_ENABLE             (1 << 0)
#define GT_CTRL_COMP_ENABLE              (1 << 1)
#define GT_CTRL_IRQ_ENABLE               (1 << 2)
#define GT_CTRL_AUTO_INC_ENABLE          (1 << 3)

#define GT_ISR_STATUS_CLEAR              (1 << 0)

#ifdef RT_HAVE_SMP
static void gtimer_init(void)
{
    GLOBAL_TIMER->CONTROL = 0;

    GLOBAL_TIMER->COMPARE_LOW = 0;
    GLOBAL_TIMER->COMPARE_HIGH = 0;

    GLOBAL_TIMER->COMPARE_LOW = APU_FREQ/2/RT_TICK_PER_SECOND;
    GLOBAL_TIMER->COMPARE_HIGH = 0;

    GLOBAL_TIMER->AUTO_INC = APU_FREQ/2;
    GLOBAL_TIMER->AUTO_INC = APU_FREQ/2/RT_TICK_PER_SECOND;

    GLOBAL_TIMER->CONTROL = GT_CTRL_AUTO_INC_ENABLE |\
                            GT_CTRL_IRQ_ENABLE |\
                            GT_CTRL_COMP_ENABLE |\
                            GT_CTRL_TIMER_ENABLE;
}

static void gtimer_isr(int vector, void *param)
{
    rt_tick_increase();
    /* clear interrupt */
    GLOBAL_TIMER->ISR_STATUS = GT_ISR_STATUS_CLEAR;
}
#endif

void secondy_cpu_c_start(void)
{
#ifdef RT_HAVE_SMP
    rt_hw_vector_init();

    rt_pf_kernel_lock();

    arm_gic_cpu_init(0, Zynq7000_GIC_CPU_BASE);
    arm_gic_set_cpu(0, IRQ_Zynq7000_GTIMER, 0x2); //指定到cpu1

    gtimer_init();
    rt_hw_interrupt_install(IRQ_Zynq7000_GTIMER, gtimer_isr, RT_NULL, "tick");
    rt_hw_interrupt_umask(IRQ_Zynq7000_GTIMER);

    rt_system_scheduler_start();
#endif /*RT_HAVE_SMP*/
}

#ifdef RT_HAVE_SMP
void plat_secondy_cpu_idle(void)
{
    asm volatile ("wfe":::"memory", "cc");
}
#endif /*RT_HAVE_SMP*/
