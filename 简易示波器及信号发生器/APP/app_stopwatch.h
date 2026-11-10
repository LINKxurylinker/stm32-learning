#ifndef __APP_STOPWATCH_H
#define __APP_STOPWATCH_H

#include "stm32f4xx.h"

/* 秒表状态。
 * 圈计时(lap) = 距上一次按"圈"的时间(本圈用时)
 * 分段计时(split) = 按"圈"那一刻的总时间(累计)
 * 两个都要显示, 题目写的是"可圈计时和分段计时"。 */
typedef struct
{
    u8  running;
    u8  lap_count;
    u32 base_ms;        /* 暂停前累计的时间 */
    u32 start_ms;       /* 本次启动时的 TICK_Now() */
    u32 lap_start_ms;   /* 本圈开始时刻 */
    u32 lap_ms;         /* 最近一圈用时 */
    u32 split_ms;       /* 最近一次按圈时的总时间 */
} STOPWATCH_Info;

extern STOPWATCH_Info g_sw;

void STOPWATCH_Init(void);
void STOPWATCH_Start(void);
void STOPWATCH_Pause(void);
void STOPWATCH_Reset(void);
void STOPWATCH_Lap(void);

u32  STOPWATCH_ElapsedMs(void);
u32  STOPWATCH_LapMs(void);
u32  STOPWATCH_SplitMs(void);
u8   STOPWATCH_IsRunning(void);

#endif
