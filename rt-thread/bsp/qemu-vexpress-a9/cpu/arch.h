#ifndef  __ARCH_H__
#define  __ARCH_H__

#ifdef __cplusplus
extern "C" {
#endif

static inline int rt_cpuid(void)
{
    int cpu_id;
    __asm__ volatile (
            "mrc p15, 0, %0, c0, c0, 5"
            :"=r"(cpu_id)
            );
    cpu_id &= 0xf;
    return cpu_id;
};

#ifdef __cplusplus
}
#endif

#endif  /*__ARCH_H__*/
