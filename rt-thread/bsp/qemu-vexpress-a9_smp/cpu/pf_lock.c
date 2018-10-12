#include "rtthread.h"
#include "rthw.h"
#include "pf_lock.h"

#ifdef RT_HAVE_SMP

raw_spinlock_t _rt_kernel_lock = {.slock = 0};

raw_spinlock_t _rt_scheduler_lock = {.slock = 0};

#endif /*RT_HAVE_SMP*/
