#ifndef  __SMP_H__
#define  __SMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#define RT_CPUS_NR 2
#define RT_CPU_MASK 3

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

extern volatile rt_uint8_t rt_percpu_interrupt_nest[];
#define rt_interrupt_nest rt_percpu_interrupt_nest[rt_cpuid()]

extern rt_list_t rt_global_thread_priority_table[RT_THREAD_PRIORITY_MAX];

extern rt_list_t rt_percpu_thread_priority_table[RT_CPUS_NR][RT_THREAD_PRIORITY_MAX];
#define rt_thread_priority_table rt_percpu_thread_priority_table[rt_cpuid()]

extern struct rt_thread *rt_percpu_current_thread[RT_CPUS_NR];
#define rt_current_thread rt_percpu_current_thread[rt_cpuid()]

extern rt_uint8_t rt_percpu_current_priority[RT_CPUS_NR];
#define rt_current_priority rt_percpu_current_priority[rt_cpuid()]

#if RT_THREAD_PRIORITY_MAX > 32
/* Maximum priority level, 256 */
extern rt_uint32_t rt_global_thread_ready_priority_group;
extern rt_uint8_t rt_global_thread_ready_table[32];                                    

extern rt_uint32_t rt_percpu_thread_ready_priority_group[RT_CPUS_NR];                              
#define rt_thread_ready_priority_group rt_percpu_thread_ready_priority_group[rt_cpuid()]

extern rt_uint8_t rt_percpu_thread_ready_table[RT_CPUS_NR][32];                                    
#define rt_thread_ready_table rt_percpu_thread_ready_table[rt_cpuid()]
#else
/* Maximum priority level, 32 */
extern rt_uint32_t rt_global_thread_ready_priority_group;

extern rt_uint32_t rt_percpu_thread_ready_priority_group[RT_CPUS_NR];                              
#define rt_thread_ready_priority_group rt_percpu_thread_ready_priority_group[rt_cpuid()]
#endif

#ifdef __cplusplus
}
#endif

#endif  /*__SMP_H__*/
