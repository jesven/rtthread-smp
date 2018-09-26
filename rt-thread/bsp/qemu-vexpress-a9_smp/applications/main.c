#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "rtthread.h"
#include "rthw.h"

int main(void)
{
    rt_enter_critical();
    rt_kprintf("hello rt-thread\n");
    rt_exit_critical();
    return 0;
}

