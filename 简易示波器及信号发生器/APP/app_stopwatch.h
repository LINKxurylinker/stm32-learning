#ifndef __APP_STOPWATCH_H
#define __APP_STOPWATCH_H

#include "stm32f4xx.h"

#define STOPWATCH_MAX_RECORDS 12

/* 秒表状态。 */
typedef struct
{
    u8  running;
    u8  lap_count;
    u8  record_count;
    u32 base_ms;
    u32 start_ms;
    u32 current_lap_ms;
    u32 lap_start_ms;
    u32 lap_ms;
    u32 split_ms;
    u32 records[STOPWATCH_MAX_RECORDS]; /* 成绩, 从小到大 */
} STOPWATCH_Info;

extern STOPWATCH_Info g_sw;

void STOPWATCH_Init(void);
void STOPWATCH_Start(void);
void STOPWATCH_Pause(void);
void STOPWATCH_Reset(void);
void STOPWATCH_Lap(void);
void STOPWATCH_Record(void);

u32  STOPWATCH_ElapsedMs(void);
u32  STOPWATCH_CurrentLapMs(void);
u32  STOPWATCH_LapMs(void);
u32  STOPWATCH_SplitMs(void);
u8   STOPWATCH_IsRunning(void);
u8   STOPWATCH_RecordCount(void);
u32  STOPWATCH_RecordMs(u8 index);

#endif
