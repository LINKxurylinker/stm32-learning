#ifndef __BSP_CAPTURE_H
#define __BSP_CAPTURE_H

#include "stm32f4xx.h"

/* 输入捕获的对外状态。
 * 中断只负责把 CCR1/CCR2 抄下来, 真正的频率/占空比计算在主循环 CAP_Update 里做。 */
typedef struct
{
    u32 freq_hz;        /* 测到的频率 */
    u16 duty_permille;  /* 占空比 x1000 (0..1000), 避免浮点 */
    u32 period_ticks;   /* 一个周期的计数值 */
    u32 high_ticks;     /* 高电平计数值 */
    u32 last_edge_ms;   /* 最后一个边沿的系统时刻 */
    u32 seq;            /* 捕获序号, 主循环用来判断"有没有新数据" */
    u8  valid;          /* 1=当前读数可信 */
} CAP_Info;

extern CAP_Info g_cap;

void CAP_Init(void);
void CAP_Update(void);      /* 主循环里每 50~100ms 调一次 */
u8   CAP_IsValid(void);

#endif
