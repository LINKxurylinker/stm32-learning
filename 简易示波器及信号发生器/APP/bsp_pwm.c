#include "bsp_pwm.h"
#include "app_config.h"

/*==============================================================================
 * bsp_pwm.c
 *-----------------------------------------------------------------------------
 * CH1: PA3 / TIM2_CH4  (84MHz, 主定时器)
 * CH2: PA2 / TIM9_CH1  (168MHz, 从定时器)
 *
 * 相位方案:
 *   - TIM2 自由运行, 产生 CH1 PWM;
 *   - TIM2_CH2 不输出到引脚, 只作内部相位参考, PWM2 模式;
 *   - TIM2 TRGO = OC2REF, 在 CH2 的比较点产生上升沿;
 *   - TIM9 从模式 Reset, 触发源 ITR0 (TIM2), 每次 TRGO 复位计数器;
 *   - 所以 TIM9_CH1 的上升沿相对 CH1 延迟 = TIM2_CH2 比较点;
 *   - 调相位只改 TIM2->CCR2, 两路都保持输出。
 *
 * 频率:
 *   - TIM2 32 位: PSC=0, ARR+1 = round(84MHz/freq), 直接作为主周期;
 *   - TIM9 16 位: 周期取 2*TIM2周期 (168MHz), 再拆 PSC/ARR;
 *   - TIM9 每个主周期都被复位, 所以实际频率跟随 TIM2。
 *============================================================================*/

PWM_Info g_pwm;
u16      g_pwm2_phase_deg = 0;
u8       g_pwm2_enabled = 0;

/* 占空比 -> CCR, period = ARR+1 */
static u32 PWM_DutyToCcr(u16 duty_pct, u32 period)
{
    unsigned long long ccr;

    if (duty_pct == 0) return 0;
    if (duty_pct >= 100) return period;

    ccr = ((unsigned long long)duty_pct * period + 50ULL) / 100ULL;
    if (ccr < 1ULL) ccr = 1ULL;
    if (ccr >= period) ccr = (unsigned long long)(period - 1UL);
    return (u32)ccr;
}

/*----------------------------------------------------------------------------
 * CH1 主定时器: TIM2_CH4 / PA3, 自检 1kHz/50%
 *----------------------------------------------------------------------------*/
void PWM_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;
    u32 arr = (PWM_TIM_HZ / 1000UL);       /* 84MHz / 1kHz = 84000 */
    u32 ccr = arr / 2UL;

    RCC_AHB1PeriphClockCmd(PWM_GPIO_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(PWM_TIM_RCC, ENABLE);

    /* PA3 = TIM2_CH4 (AF1) */
    GPIO_PinAFConfig(PWM_GPIO, PWM_PIN_SRC, PWM_AF);
    GPIO_InitStructure.GPIO_Pin   = PWM_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(PWM_GPIO, &GPIO_InitStructure);

    /* TIM2 时基: 32 位, PSC=0, ARR=83999 -> 1kHz */
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Prescaler         = 0;
    TIM_TimeBaseStructure.TIM_Period            = arr - 1UL;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(PWM_TIM, &TIM_TimeBaseStructure);

    /* CH1 输出: TIM2_CH4, PWM1 */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = ccr;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC4Init(PWM_TIM, &TIM_OCInitStructure);
    TIM_OC4PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);

    /* 相位参考: TIM2_CH2, 不输出到引脚; PWM2 模式让 OC2REF 在 CCR2 处上升 */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;
    TIM_OCInitStructure.TIM_Pulse       = 0;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC2Init(PWM_TIM, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(PWM_TIM, ENABLE);

    /* 主从: TIM2 TRGO = OC2REF; TIM9 在 bsp_pwm 的 CH2 初始化里设从模式 */
    TIM_SelectOutputTrigger(PWM_TIM, TIM_TRGOSource_OC2Ref);
    TIM_SelectMasterSlaveMode(PWM_TIM, TIM_MasterSlaveMode_Enable);

    TIM_Cmd(PWM_TIM, ENABLE);

#if PWM_BRINGUP_TEST
    TIM_CCxCmd(PWM_TIM, TIM_Channel_4, ENABLE);
    g_pwm.freq_set_hz     = 1000;
    g_pwm.freq_actual_hz  = PWM_TIM_HZ / arr;
    g_pwm.duty_set_pct    = 50;
    g_pwm.duty_actual_pct = 50;
    g_pwm.psc             = 0;
    g_pwm.arr             = arr - 1UL;
    g_pwm.ccr             = ccr;
    g_pwm.running         = 1;
#else
    TIM_CCxCmd(PWM_TIM, TIM_Channel_4, DISABLE);
    g_pwm.freq_set_hz     = 0;
    g_pwm.freq_actual_hz  = 0;
    g_pwm.duty_set_pct    = 0;
    g_pwm.duty_actual_pct = 0;
    g_pwm.psc             = 0;
    g_pwm.arr             = arr - 1UL;
    g_pwm.ccr             = 0;
    g_pwm.running         = 0;
#endif

    g_pwm2_phase_deg = 0;
}

/*----------------------------------------------------------------------------
 * 频率 + CH1/CH2 占空比一起设置 (相位保持)
 *----------------------------------------------------------------------------*/
static u8 PWM_Apply(u32 freq_hz, u16 duty1, u16 duty2)
{
    u32 master_period;
    u32 slave_period;
    u32 psc9_plus;
    u32 arr9_plus;
    u32 ccr1;
    u32 ccr9;
    u32 phase;

    if (freq_hz < PWM_FREQ_MIN_HZ || freq_hz > PWM_FREQ_MAX_HZ) return APP_ERR_PARAM;
    if (duty1 > 100 || duty2 > 100) return APP_ERR_PARAM;

    /* TIM2: 32 位, PSC=0, 总计数就是 ARR+1 */
    master_period = (PWM_TIM_HZ + freq_hz / 2UL) / freq_hz;
    if (master_period < 2UL) master_period = 2UL;

    ccr1 = PWM_DutyToCcr(duty1, master_period);

    /* 相位: 0..360 -> TIM2 计数点; 360 等价 0 */
    if (g_pwm2_phase_deg >= 360)
    {
        phase = 0;
    }
    else
    {
        phase = (u32)(((unsigned long long)master_period * g_pwm2_phase_deg) / 360ULL);
    }
    if (phase >= master_period) phase = master_period - 1UL;

    /* TIM9: 168MHz = 2 * 84MHz, 所以周期计数是主周期的 2 倍 */
    slave_period = master_period * 2UL;

    psc9_plus = (slave_period + 65534UL) / 65535UL;
    if (psc9_plus == 0) psc9_plus = 1;

    arr9_plus = (slave_period + psc9_plus / 2UL) / psc9_plus;
    if (arr9_plus < 1) arr9_plus = 1;
    if (arr9_plus > 65535UL) arr9_plus = 65535UL;

    ccr9 = PWM_DutyToCcr(duty2, arr9_plus);

    /* 写主定时器 */
    TIM_Cmd(PWM_TIM, ENABLE);
    PWM_TIM->PSC  = 0;
    PWM_TIM->ARR  = master_period - 1UL;
    PWM_TIM->CCR4 = ccr1;      /* CH1 = PA3 = TIM2_CH4 */
    PWM_TIM->CCR2 = phase;     /* 相位参考 = TIM2_CH2 */

    /* 写从定时器 */
    TIM_Cmd(PWM2_TIM, ENABLE);
    PWM2_TIM->PSC  = (u16)(psc9_plus - 1UL);
    PWM2_TIM->ARR  = (u16)(arr9_plus - 1UL);
    PWM2_TIM->CCR1 = (u16)ccr9;   /* CH2 = PA2 = TIM9_CH1 */

    TIM_GenerateEvent(PWM_TIM, TIM_EventSource_Update);
    TIM_GenerateEvent(PWM2_TIM, TIM_EventSource_Update);

    g_pwm.freq_set_hz     = freq_hz;
    g_pwm.freq_actual_hz  = (PWM_TIM_HZ + master_period / 2UL) / master_period;
    g_pwm.duty_set_pct    = duty1;
    g_pwm.duty_actual_pct = (u16)(((unsigned long long)ccr1 * 100ULL + master_period / 2UL) / master_period);
    g_pwm.psc             = 0;
    g_pwm.arr             = master_period - 1UL;
    g_pwm.ccr             = ccr1;

    return APP_OK;
}

/* 两路同时设成同一个占空比 */
u8 PWM_Set(u32 freq_hz, u16 duty_pct)
{
    return PWM_Apply(freq_hz, duty_pct, duty_pct);
}

u8 PWM_SetDuty(u16 duty_pct)
{
    if (duty_pct > 100) return APP_ERR_PARAM;
    return PWM_Apply(g_pwm.freq_set_hz, duty_pct, duty_pct);
}

/* 只改 CH1 的占空比, CH2 占空比和相位保持不变 */
u8 PWM_SetDutyCH1(u16 duty_pct)
{
    if (duty_pct > 100) return APP_ERR_PARAM;
    return PWM_Apply(g_pwm.freq_set_hz, duty_pct, PWM2_GetDuty());
}

/* 改频率: 保留 CH1/CH2 当前占空比和相位 */
u8 PWM_SetFreq(u32 freq_hz)
{
    return PWM_Apply(freq_hz, PWM_GetDuty(), PWM2_GetDuty());
}

void PWM_Enable(u8 en)
{
    if (en)
    {
        TIM_Cmd(PWM_TIM, ENABLE);
        TIM_CCxCmd(PWM_TIM, TIM_Channel_4, ENABLE);
        g_pwm.running = 1;
    }
    else
    {
        TIM_CCxCmd(PWM_TIM, TIM_Channel_4, DISABLE);
        g_pwm.running = 0;
    }
}

u32 PWM_GetFreq(void)
{
    return g_pwm.freq_actual_hz;
}

u16 PWM_GetDuty(void)
{
    return g_pwm.duty_actual_pct;
}

/*----------------------------------------------------------------------------
 * CH2 从定时器: TIM9_CH1 / PA2, 由 TIM2 TRGO 复位
 *----------------------------------------------------------------------------*/
void PWM2_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;
    u32 arr = (PWM2_TIM_HZ / 1000UL);      /* 168MHz / 1kHz = 168000 */
    u16 psc = (u16)(arr / 1000UL - 1UL);   /* 167 */
    u16 period = 999;
    u16 ccr = 500;

    RCC_AHB1PeriphClockCmd(PWM2_GPIO_RCC, ENABLE);
    RCC_APB2PeriphClockCmd(PWM2_TIM_RCC, ENABLE);

    /* PA2 = TIM9_CH1 (AF3) */
    GPIO_PinAFConfig(PWM2_GPIO, PWM2_PIN_SRC, PWM2_AF);
    GPIO_InitStructure.GPIO_Pin   = PWM2_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(PWM2_GPIO, &GPIO_InitStructure);

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Prescaler         = psc;
    TIM_TimeBaseStructure.TIM_Period            = period;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(PWM2_TIM, &TIM_TimeBaseStructure);

    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = ccr;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(PWM2_TIM, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(PWM2_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(PWM2_TIM, ENABLE);

    /* 从模式 Reset, 触发源 ITR0 = TIM2 TRGO */
    TIM_SelectSlaveMode(PWM2_TIM, TIM_SlaveMode_Reset);
    TIM_SelectInputTrigger(PWM2_TIM, TIM_TS_ITR0);

    TIM_Cmd(PWM2_TIM, ENABLE);

#if PWM_BRINGUP_TEST
    TIM_CCxCmd(PWM2_TIM, TIM_Channel_1, ENABLE);
    g_pwm2_enabled = 1;
#else
    TIM_CCxCmd(PWM2_TIM, TIM_Channel_1, DISABLE);
    g_pwm2_enabled = 0;
#endif

    g_pwm2_phase_deg = 0;
}

void PWM2_Enable(u8 en)
{
    if (en)
    {
        TIM_Cmd(PWM2_TIM, ENABLE);
        TIM_CCxCmd(PWM2_TIM, TIM_Channel_1, ENABLE);
        g_pwm2_enabled = 1;
    }
    else
    {
        TIM_CCxCmd(PWM2_TIM, TIM_Channel_1, DISABLE);
        g_pwm2_enabled = 0;
    }
}

u16 PWM2_GetDuty(void)
{
    u32 arr_plus = (u32)PWM2_TIM->ARR + 1UL;
    if (arr_plus == 0) return 0;
    return (u16)(((u32)PWM2_TIM->CCR1 * 100UL + arr_plus / 2UL) / arr_plus);
}

u8 PWM2_SetDuty(u16 duty_pct)
{
    u32 arr_plus;
    u32 ccr;

    if (duty_pct > 100) return APP_ERR_PARAM;

    arr_plus = (u32)PWM2_TIM->ARR + 1UL;
    if (arr_plus < 2) return APP_ERR_PARAM;

    ccr = PWM_DutyToCcr(duty_pct, arr_plus);
    TIM_Cmd(PWM2_TIM, ENABLE);
    PWM2_TIM->CCR1 = (u16)ccr;
    TIM_GenerateEvent(PWM2_TIM, TIM_EventSource_Update);

    return APP_OK;
}

/* 相位 0..360, 5 度一档; 调相位期间两路都不停 */
u8 PWM2_SetPhaseDeg(u16 deg)
{
    u32 master_period;
    u32 phase;

    if (deg > 360 || (deg % 5U) != 0) return APP_ERR_PARAM;

    master_period = PWM_TIM->ARR + 1UL;
    if (master_period < 2UL) return APP_ERR_PARAM;

    if (deg >= 360)
    {
        phase = 0;
    }
    else
    {
        phase = (u32)(((unsigned long long)master_period * deg) / 360ULL);
    }
    if (phase >= master_period) phase = master_period - 1UL;

    TIM_Cmd(PWM_TIM, ENABLE);
    PWM_TIM->CCR2 = phase;
    TIM_GenerateEvent(PWM_TIM, TIM_EventSource_Update);

    g_pwm2_phase_deg = deg;
    return APP_OK;
}

u16 PWM2_GetPhaseDeg(void)
{
    return g_pwm2_phase_deg;
}
