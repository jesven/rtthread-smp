#ifndef  __RTLOCK_H__
#define  __RTLOCK_H__

#include <rtthread.h>
#include <rthw.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RT_HAVE_SMP
#include <pf_lock.h>

typedef struct spinlock {
    raw_spinlock_t lock;
} rt_spinlock_t;

#define __RT_SPIN_LOCK_INITIALIZER(lockname) \
 { .lock = __RAW_SPIN_LOCK_INITIALIZER(lockname) }

#define __RT_SPIN_LOCK_UNLOCKED(lockname) \
 (rt_spinlock_t ) __RT_SPIN_LOCK_INITIALIZER(lockname)

#define RT_DEFINE_SPINLOCK(x)  rt_spinlock_t x = __RT_SPIN_LOCK_UNLOCKED(x)

static inline void rt_spin_lock(rt_spinlock_t *lock)
{
    __raw_spin_lock(&lock->lock);
}

static inline void rt_spin_unlock(rt_spinlock_t *lock)
{
    __raw_spin_unlock(&lock->lock);
}

extern rt_spinlock_t _rt_kernel_lock;
extern rt_spinlock_t _rt_critical_lock;

#define rt_pf_kernel_lock() __raw_spin_lock(&_rt_kernel_lock.lock);
#define rt_pf_kernel_unlock() __raw_spin_unlock(&_rt_kernel_lock.lock);

#define rt_pf_critical_lock() __raw_spin_lock(&_rt_critical_lock.lock);
#define rt_pf_critical_unlock() __raw_spin_unlock(&_rt_critical_lock.lock);

#endif

#ifdef __cplusplus
}
#endif

#endif  /*__RTLOCK_H__*/
