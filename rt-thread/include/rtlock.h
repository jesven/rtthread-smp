#ifndef  __RTLOCK_H__
#define  __RTLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RT_HAVE_SMP

#include <rtthread.h>
#include <pf_lock.h>

rt_base_t rt_hw_interrupt_disable(void);

void rt_hw_interrupt_enable(rt_base_t level);

void rt_hw_interrupt_disable_int(void);

void rt_hw_interrupt_enable_int(void);

void rt_enter_critical(void);

void rt_exit_critical(void);

#endif /*RT_HAVE_SMP*/

#ifdef __cplusplus
}
#endif

#endif  /*__RTLOCK_H__*/
