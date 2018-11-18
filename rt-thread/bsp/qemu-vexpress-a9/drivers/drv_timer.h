/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 */

#ifndef DRV_TIMER_H__
#define DRV_TIMER_H__

void timer_init(int timer, unsigned int preload);
void timer_clear_pending(int timer);

#endif
