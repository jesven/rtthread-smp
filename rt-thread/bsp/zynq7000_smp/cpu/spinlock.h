#ifndef  __SPINLOCK_H__
#define  __SPINLOCK_H__

typedef struct {
    union {
        unsigned long slock;
        struct __raw_tickets {
            unsigned short owner;
            unsigned short next;
        } tickets;
    };
} raw_spinlock_t;

extern raw_spinlock_t rt_kernel_lock;

#define spin_lock() __raw_spin_lock(&rt_kernel_lock);
#define spin_unlock() __raw_spin_unlock(&rt_kernel_lock);

static inline void __raw_spin_lock(raw_spinlock_t *lock)
{
    unsigned long tmp;
    unsigned long newval;
    raw_spinlock_t lockval;

    __asm__ __volatile__(
            "pld [%0]"
            ::"r"(&lock->slock)
            );

    __asm__ __volatile__(
            "1: ldrex   %0, [%3]\n"
            "   add %1, %0, %4\n"
            "   strex   %2, %1, [%3]\n"
            "   teq %2, #0\n"
            "   bne 1b"
            : "=&r" (lockval), "=&r" (newval), "=&r" (tmp)
            : "r" (&lock->slock), "I" (1 << 16)
            : "cc");

    while (lockval.tickets.next != lockval.tickets.owner) {
        __asm__ __volatile__("wfe":::"memory");
        lockval.tickets.owner = *(volatile unsigned short *)(&lock->tickets.owner);
    }

    __asm__ volatile ("dmb":::"memory");
}

static inline void __raw_spin_unlock(raw_spinlock_t *lock)
{
    __asm__ volatile ("dmb":::"memory");
    lock->tickets.owner++;
    __asm__ volatile ("dsb ishst\nsev":::"memory");
}

#endif  /*__SPINLOCK_H__*/
