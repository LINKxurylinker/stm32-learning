#include "app_stopwatch.h"
#include "bsp_tick.h"
#include "app_config.h"

/*==============================================================================
 * app_stopwatch.c
 *-----------------------------------------------------------------------------
 * 题目要求: 定时器正计时, 可暂停/重置, 精确到毫秒, 可圈计时和分段计时。
 *
 * 本框架用 TIM6 的 1ms 节拍做时间基准 (不要再自己开一个定时器)。
 * 状态机很简单, 但有几个坑:
 *   - 暂停时要把"已经过去的时间"累加到 base_ms, 恢复时重新记 start_ms;
 *   - 重置要同时清 running/base/start/lap;
 *   - 圈计时在暂停状态下按, 要有明确定义 (建议: 允许, 圈时间不变);
 *   - 显示不要每毫秒刷一次, 25Hz 足够, 且要用固定宽度防止残影。
 *
 * 已实现: Init 和几个 getter。
 * TODO-07: Start/Pause/Reset/Lap 的状态转换留给你写。
 *============================================================================*/

STOPWATCH_Info g_sw;

void STOPWATCH_Init(void)
{
    g_sw.running     = 0;
    g_sw.lap_count   = 0;
    g_sw.base_ms     = 0;
    g_sw.start_ms    = 0;
    g_sw.lap_start_ms = 0;
    g_sw.lap_ms      = 0;
    g_sw.split_ms    = 0;
}

/*----------------------------------------------------------------------------
 * TODO-07 (基本功能, 留给你)
 *
 * Start:
 *   - 如果已经在跑, 直接返回;
 *   - running=1; start_ms=TICK_Now(); lap_start_ms=start_ms;
 *
 * Pause:
 *   - 如果没在跑, 直接返回;
 *   - base_ms += TICK_Now() - start_ms; running=0;
 *   - 注意: 暂停时 elapsed 不再增长, 但 lap/split 保留。
 *
 * Reset:
 *   - running=0; base_ms=0; start_ms=0; lap_start_ms=0;
 *   - lap_ms=0; split_ms=0; lap_count=0;
 *
 * Lap:
 *   - 只有在 running 时才有意义 (暂停时怎么处理自己定, 注释写清楚);
 *   - now = TICK_Now();
 *   - lap_ms = now - lap_start_ms;
 *   - split_ms = base_ms + (now - start_ms);   // 总时间
 *   - lap_start_ms = now; lap_count++;
 *
 * 验收: 暂停/恢复不丢时间, 重置归零, 圈/分段读数正确, 毫秒位在走。
 *----------------------------------------------------------------------------*/
void STOPWATCH_Start(void)
{
    if (g_sw.running) return;

    g_sw.start_ms = TICK_Now();
    g_sw.lap_start_ms = g_sw.start_ms;
    g_sw.running = 1;
}

void STOPWATCH_Pause(void)
{
    u32 now;

    if (!g_sw.running) return;

    now = TICK_Now();
    g_sw.base_ms += (u32)(now - g_sw.start_ms);
    g_sw.running = 0;
}

void STOPWATCH_Reset(void)
{
    g_sw.running = 0;
    g_sw.lap_count = 0;
    g_sw.base_ms = 0;
    g_sw.start_ms = 0;
    g_sw.lap_start_ms = 0;
    g_sw.lap_ms = 0;
    g_sw.split_ms = 0;
}

void STOPWATCH_Lap(void)
{
    u32 now;

    /* 暂停时不允许圈计时, 避免圈时间在暂停期间继续增长 */
    if (!g_sw.running) return;

    now = TICK_Now();
    g_sw.lap_ms = (u32)(now - g_sw.lap_start_ms);
    g_sw.split_ms = g_sw.base_ms + (u32)(now - g_sw.start_ms);
    g_sw.lap_start_ms = now;
    g_sw.lap_count++;
}

/* 实时总时间: 运行中 = 暂停前累计 + 本次已跑; 暂停 = 累计值。
 * 这个 getter 已经写好, 状态转换由 TODO-07 负责。 */
u32 STOPWATCH_ElapsedMs(void)
{
    if (g_sw.running)
    {
        return g_sw.base_ms + (u32)(TICK_Now() - g_sw.start_ms);
    }
    return g_sw.base_ms;
}

u32 STOPWATCH_LapMs(void)
{
    return g_sw.lap_ms;
}

u32 STOPWATCH_SplitMs(void)
{
    return g_sw.split_ms;
}

u8 STOPWATCH_IsRunning(void)
{
    return g_sw.running;
}
