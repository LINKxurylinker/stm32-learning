#include "bsp_capture.h"
#include "bsp_tick.h"
#include "app_config.h"

/*==============================================================================
 * bsp_capture.c
 *-----------------------------------------------------------------------------
 * 方案: TIM5_CH1 (PA0) 的 PWM 输入模式。
 *   - CH1 上升沿捕获: CCR1 = 一个完整周期
 *   - CH2 下降沿捕获: CCR2 = 高电平时间
 *   - 从模式 Reset: 每次上升沿把计数器清零, 所以不用自己算溢出
 *   - 32 位 TIM5: 1Hz 时周期是 8400万, 16位定时器早就溢出了
 *
 * 中断频率 = 信号频率 (每个周期一次), 200kHz 时 20万次/秒。
 * 所以中断里只允许"读两个寄存器 + 序号++", 所有除法/平均都放主循环。
 *
 * 时钟: TIM5 在 APB1 上, 计数时钟 84MHz, PSC=0 时 1 tick = 11.9ns。
 *   - 200kHz -> 420 ticks/周期
 *   - 1Hz    -> 84,000,000 ticks/周期
 * 分辨率: 200kHz 时 1 tick 约 0.238%, 测频率够用;
 *         测占空比时低频段占比越大越准, 高频小占空比要自己想办法(平均/插值)。
 *============================================================================*/

CAP_Info g_cap;

static volatile u32 s_period_ticks = 0;
static volatile u32 s_high_ticks   = 0;
static volatile u32 s_seq          = 0;
static volatile u32 s_last_edge_ms = 0;

/* 稳定判定: 高频测量要连续两次接近才更新, 防止无信号时噪声乱跳 */
static u32 s_cand_freq = 0;
static u8  s_cand_cnt  = 0;

void CAP_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef       TIM_ICInitStructure;
    NVIC_InitTypeDef        NVIC_InitStructure;

    /* 清零对外状态 */
    g_cap.freq_hz       = 0;
    g_cap.duty_permille = 0;
    g_cap.period_ticks  = 0;
    g_cap.high_ticks    = 0;
    g_cap.last_edge_ms  = 0;
    g_cap.seq           = 0;
    g_cap.valid         = 0;

    RCC_AHB1PeriphClockCmd(CAP_GPIO_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(CAP_TIM_RCC, ENABLE);

    /* 输入脚: 复用 + 下拉。下拉的作用是没接信号时读到低电平,
     * 不会因为浮空捡到噪声一直进中断。信号源是推挽输出, 不受影响。 */
    GPIO_PinAFConfig(CAP_GPIO, CAP_PIN_SRC, CAP_AF);
    GPIO_InitStructure.GPIO_Pin   = CAP_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN;
    GPIO_Init(CAP_GPIO, &GPIO_InitStructure);

    /* 32 位自由计数, 不分频 */
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Prescaler     = 0;
    TIM_TimeBaseStructure.TIM_Period        = 0xFFFFFFFF;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(CAP_TIM, &TIM_TimeBaseStructure);

    /* PWM 输入模式: 会自动把 IC1 配成上升沿、IC2 配成下降沿,
     * 并且两个通道都映射到 TI1 (也就是 PA0 这一根线)。 */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_Channel     = TIM_Channel_1;
    TIM_ICInitStructure.TIM_ICPolarity  = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
#if APP_ENABLE_EXT500K
    TIM_ICInitStructure.TIM_ICFilter    = 0x00;  /* 500kHz 下 5% 高电平只有 100ns, 不能重滤波 */
#else
    TIM_ICInitStructure.TIM_ICFilter    = 0x03;  /* 轻滤波, 抑制输入毛刺; 200kHz 仍可用 */
#endif
    TIM_PWMIConfig(CAP_TIM, &TIM_ICInitStructure);

    /* 触发源 TI1FP1, 从模式 Reset: 上升沿清零计数器 */
    TIM_SelectInputTrigger(CAP_TIM, TIM_TS_TI1FP1);
    TIM_SelectSlaveMode(CAP_TIM, TIM_SlaveMode_Reset);
    TIM_SelectMasterSlaveMode(CAP_TIM, TIM_MasterSlaveMode_Enable);

    TIM_ClearITPendingBit(CAP_TIM, TIM_IT_CC1);
    TIM_ITConfig(CAP_TIM, TIM_IT_CC1, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = CAP_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  /* 比 UI/节拍高 */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(CAP_TIM, ENABLE);
}

/* 每个上升沿进一次。这里绝对不要加 printf / 浮点 / 画屏。
 * 200kHz 时 5us 一次中断, 直接读寄存器, 不用库函数做标志判断。 */
void TIM5_IRQHandler(void)
{
    if (CAP_TIM->SR & TIM_SR_CC1IF)
    {
        /* 清 CC1 捕获标志, 同时清可能的过捕获标志 */
        CAP_TIM->SR = (u16)~(TIM_SR_CC1IF | TIM_SR_CC1OF);

        s_period_ticks = CAP_TIM->CCR1;
        s_high_ticks   = CAP_TIM->CCR2;
        s_last_edge_ms = TICK_Now();
        s_seq++;
    }
}

/*----------------------------------------------------------------------------
 * TODO-05 (核心算法, 留给你)
 *
 * 输入: s_period_ticks, s_high_ticks (84MHz 计数值)
 * 输出: g_cap.freq_hz, g_cap.duty_permille, g_cap.valid
 *
 * 你必须处理:
 *   1) 没有新边沿: seq 没变时不要重复算; 超过 CAP_TIMEOUT_MS 就 valid=0。
 *   2) 除零: period=0 直接判无效。
 *   3) 频率: f = CAP_TIM_HZ / period, 四舍五入; 64位运算防溢出。
 *   4) 占空比: duty = high * 1000 / period, 注意 high <= period,
 *      信号有毛刺时 high 可能略大于 period, 要 clamp。
 *   5) 平均: 最近 CAP_AVG_N 个周期做平均可以降噪, 但低频时周期长,
 *      平均太多会导致 2 秒内跟不上, 自己权衡。
 *   6) 200kHz 时 period 只有 420, 1 tick 就是 0.238%,
 *      占空比 10% 时 high=42, 量化误差已经不小; 想想怎么用平均/插值改善。
 *   7) 1Hz 时等一个周期就要 1 秒, 界面刷新不能比它快太多。
 *
 * 验收: 1Hz..200kHz, 频率误差 <=1%, 占空比误差 <=2%,
 *       信号源改动后 2 秒内屏幕跟随。
 *----------------------------------------------------------------------------*/
void CAP_Update(void)
{
    u32 seq = s_seq;
    u32 period;
    u32 high;
    u32 freq;
    u32 duty;
    u32 diff;

    if (seq == g_cap.seq)
    {
        /* 没有新边沿: 超时就判无效, 否则保持上一次的读数 */
        if ((u32)(TICK_Now() - s_last_edge_ms) > CAP_TIMEOUT_MS)
        {
            g_cap.valid = 0;
            s_cand_cnt = 0;
        }
        return;
    }

    period = s_period_ticks;
    high   = s_high_ticks;

    g_cap.last_edge_ms = s_last_edge_ms;
    g_cap.seq          = seq;

    if (period == 0)
    {
        g_cap.valid = 0;
        s_cand_cnt = 0;
        return;
    }

    /* f = 84MHz / period, 四舍五入 */
    freq = (u32)(((unsigned long long)CAP_TIM_HZ + period / 2UL) / period);

    /* 上限留 1% 余量: 200kHz 时周期只有 420 个计数,
     * 1 个计数的量化误差就可能让读数略高于 200000,
     * 不能因此把它判成无效。 */
    if (freq < CAP_FREQ_MIN_HZ ||
        freq > (CAP_FREQ_MAX_HZ + CAP_FREQ_MAX_HZ / 100UL))
    {
        g_cap.valid = 0;
        s_cand_cnt = 0;
        return;
    }

    /* 占空比 = 高电平时间 / 周期, 单位 0.1% (0..1000) */
    if (high > period) high = period;
    duty = (u32)(((unsigned long long)high * 1000ULL + period / 2UL) / period);
    if (duty > 1000UL) duty = 1000UL;

    /* 10Hz 以下直接认, 保证低频 2 秒内跟上。
     * 10Hz 以上要求连续两次读数接近, 防止无信号/噪声时乱跳。 */
    if (freq > 10UL)
    {
        if (s_cand_cnt > 0)
        {
            diff = (freq > s_cand_freq) ? (freq - s_cand_freq)
                                        : (s_cand_freq - freq);
            if (diff <= s_cand_freq / 20UL)
            {
                if (s_cand_cnt < 4) s_cand_cnt++;
            }
            else
            {
                s_cand_freq = freq;
                s_cand_cnt = 1;
            }
        }
        else
        {
            s_cand_freq = freq;
            s_cand_cnt = 1;
        }

        if (g_cap.valid)
        {
            diff = (freq > g_cap.freq_hz) ? (freq - g_cap.freq_hz)
                                          : (g_cap.freq_hz - freq);

            /* 和当前显示值接近: 立即更新 */
            if (diff <= g_cap.freq_hz / 20UL)
            {
                s_cand_cnt = 0;
            }
            /* 变化很大: 连续两次接近新值才更新, 中间保持旧值 */
            else if (s_cand_cnt < 2)
            {
                return;
            }
        }
        else if (s_cand_cnt < 2)
        {
            return;
        }
    }
    else
    {
        s_cand_cnt = 0;
    }

    g_cap.period_ticks  = period;
    g_cap.high_ticks    = high;
    g_cap.freq_hz       = freq;
    g_cap.duty_permille = (u16)duty;
    g_cap.valid         = 1;
}

u8 CAP_IsValid(void)
{
    return g_cap.valid;
}
