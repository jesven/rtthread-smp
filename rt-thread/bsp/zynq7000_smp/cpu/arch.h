#ifndef  __ARCH_H__
#define  __ARCH_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RT_USING_SMP
int rt_hw_cpu_id(void);
#endif

#ifdef __cplusplus
}
#endif

#endif  /*__ARCH_H__*/
