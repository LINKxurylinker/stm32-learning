#include "bsp_tick.h"
#include "app_config.h"

/*==============================================================================
 * bsp_tick.c
 *-----------------------------------------------------------------------------
 * TIM6 每 1ms 产生一次更新中断, 中断里只做一件最简单的事: 计数器 +1。
 * 不允许在中断里做任何耗时操作 (printf、画屏、浮点运算都不行)。
 *
 * 时钟: TIM6 在 APB1 上, 计数时钟 84MHz。
 *       84MHz / (83+1) = 1MHz
 *       1MHz / (999+1) = 1kHz -> 1ms
 *============================================================================*/

static volatile u32 s_tick_ms = 0;

void TICK_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef        NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(TICK_TIM_RCC, ENABLE);

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Prescaler     = (u16)((TICK_TIM_HZ / 1000000UL) - 1); /* 1MHz */
    TIM_TimeBaseStructure.TIM_Period        = 999;                                  /* 1000 counts = 1ms */
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TICK_TIM, &TIM_TimeBaseStructure);

    TIM_ClearITPendingBit(TICK_TIM, TIM_IT_Update);
    TIM_ITConfig(TICK_TIM, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = TICK_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;   /* 最低, 不能抢捕获/SPWM */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TICK_TIM, ENABLE);
}

u32 TICK_Now(void)
{
    return s_tick_ms;
}

/* 非阻塞定时: 到达 period_ms 返回1, 并把 last_ms 更新为当前时间。
 * 用法:
 *     static u32 last = 0;
 *     if (TICK_Elapsed(&last, 100)) { ... 每100ms执行一次 ... }
 * 注意: 无符号减法自动处理 32 位回绕。 */
u8 TICK_Elapsed(u32 *last_ms, u32 period_ms)
{
    u32 now = s_tick_ms;

    if ((u32)(now - *last_ms) >= period_ms)
    {
        *last_ms = now;
        return 1;
    }
    return 0;
}

/* 忙等延时。只建议在初始化阶段用; 主循环里一律用 TICK_Elapsed 做非阻塞调度。 */
void TICK_DelayMs(u32 ms)
{
    u32 start = TICK_Now();

    while ((u32)(TICK_Now() - start) < ms)
    {
        /* busy wait */
    }
}

/* TIM6 的向量名在 startup 文件里是 TIM6_DAC_IRQHandler */
void TIM6_DAC_IRQHandler(void)
{
    if (TIM_GetITStatus(TICK_TIM, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TICK_TIM, TIM_IT_Update);
        s_tick_ms++;
    }
}
