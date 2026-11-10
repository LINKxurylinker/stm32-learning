#ifndef __BSP_TICK_H
#define __BSP_TICK_H

#include "stm32f4xx.h"

/* 1ms 系统节拍 (TIM6 中断)
 * 作用: 给 UI、秒表、扫频、贪吃蛇提供统一的时间基准。
 * 为什么不用 SysTick: 例程的 delay.c 把 SysTick 当查询式计数器用,
 * SysTick_Handler 是空的; 再开 SysTick 中断会和 delay_us 抢寄存器。 */
void TICK_Init(void);
u32  TICK_Now(void);
u8   TICK_Elapsed(u32 *last_ms, u32 period_ms);
void TICK_DelayMs(u32 ms);      /* 忙等, 只建议初始化阶段使用 */

#endif
