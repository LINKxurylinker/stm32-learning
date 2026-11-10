#include "bsp_spwm.h"
#include "app_config.h"

/*==============================================================================
 * bsp_spwm.c
 *-----------------------------------------------------------------------------
 * 目标: TIM3_CH1 (PA6) 输出 SPWM, 载波 20kHz, 正弦 50Hz。
 *      外部接一个简易 RC 低通, 把 20kHz 载波滤掉, 留下 50Hz 正弦。
 *
 * 关系:
 *   载波频率 = 84MHz / (ARR+1) = 84MHz / 4200 = 20kHz
 *   正弦频率 = 载波频率 / 点数 = 20kHz / 400 = 50Hz
 *   每个载波周期更新一次 CCR, 表里放 400 个正弦幅值。
 *
 * 已实现: 正弦表、Start/Stop、20kHz 更新中断查表写 CCR。
 * 外部接 RC 低通, 把 20kHz 载波滤掉, 留下 50Hz 正弦。
 *
 * 注意:
 *   - 不要用 sin() 在 20kHz 中断里现算, 中断里只做查表 + 写 CCR。
 *   - 正弦频率误差要 <=1%, 所以载波和点数必须是整数倍关系,
 *     不要随便把 400 改成 401。
 *   - 高级定时器才需要 TIM_CtrlPWMOutputs; TIM3 不需要。
 *   - 输出幅度取决于电源和调制深度, 滤波电路自己算 (截止频率在
 *     50Hz 和 20kHz 之间, 例如 2 阶 RC 截止 500Hz 左右)。
 *============================================================================*/

SPWM_Info g_spwm;
u16       g_spwm_table[SPWM_POINTS];

/* 0..90 度的正弦, Q15 定点, 101 个点 (含峰值)。
 * 完整的 400 点表由四分之一表镜像生成, 不在初始化时调用 sin()。 */
static const u16 s_sin_q15[101] =
{
       0,   515,  1029,  1544,  2057,  2571,  3084,  3596,  4107,  4617,
    5126,  5634,  6140,  6645,  7148,  7649,  8149,  8646,  9142,  9635,
   10126, 10614, 11099, 11582, 12062, 12539, 13013, 13484, 13952, 14415,
   14876, 15333, 15786, 16235, 16680, 17121, 17557, 17990, 18418, 18841,
   19260, 19674, 20083, 20487, 20886, 21280, 21669, 22053, 22431, 22803,
   23170, 23531, 23886, 24235, 24579, 24916, 25247, 25572, 25891, 26203,
   26509, 26808, 27101, 27387, 27666, 27938, 28204, 28462, 28714, 28958,
   29196, 29426, 29648, 29864, 30072, 30273, 30466, 30652, 30830, 31000,
   31163, 31318, 31466, 31606, 31738, 31862, 31978, 32086, 32187, 32279,
   32364, 32440, 32509, 32569, 32622, 32666, 32702, 32731, 32751, 32763,
   32767
};

/* 中心值 50%, 幅度按 SPWM_AMP_PCT 缩放 */
static void SPWM_BuildTable(void)
{
    u32 arr_plus = SPWM_TIM_HZ / SPWM_CARRIER_HZ;
    u16 center;
    u32 amp;
    u16 i;
    int32_t s;
    int32_t v;
    int32_t off;

    if (arr_plus < 2) return;

    center = (u16)(arr_plus / 2UL);
    amp    = ((u32)center * (u32)SPWM_AMP_PCT) / 100UL;

    for (i = 0; i < SPWM_POINTS; i++)
    {
        if (i < 100U)
        {
            s = (int32_t)s_sin_q15[i];
        }
        else if (i < 200U)
        {
            s = (int32_t)s_sin_q15[200U - i];
        }
        else if (i < 300U)
        {
            s = -(int32_t)s_sin_q15[i - 200U];
        }
        else
        {
            s = -(int32_t)s_sin_q15[400U - i];
        }

        off = (s * (int32_t)amp) / 32767;
        v = (int32_t)center + off;

        if (v < 0) v = 0;
        if (v > (int32_t)arr_plus) v = (int32_t)arr_plus;

        g_spwm_table[i] = (u16)v;
    }
}

void SPWM_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;
    NVIC_InitTypeDef        NVIC_InitStructure;
    u32 arr;

    g_spwm.enabled    = 0;
    g_spwm.carrier_hz = SPWM_CARRIER_HZ;
    g_spwm.sine_hz    = SPWM_SINE_HZ;
    g_spwm.points     = SPWM_POINTS;
    g_spwm.amp_pct    = SPWM_AMP_PCT;
    g_spwm.index      = 0;

    RCC_AHB1PeriphClockCmd(SPWM_GPIO_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(SPWM_TIM_RCC, ENABLE);

    GPIO_PinAFConfig(SPWM_GPIO, SPWM_PIN_SRC, SPWM_AF);
    GPIO_InitStructure.GPIO_Pin   = SPWM_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(SPWM_GPIO, &GPIO_InitStructure);

    /* 载波周期: ARR+1 = 84MHz / 20kHz = 4200 */
    arr = (SPWM_TIM_HZ / SPWM_CARRIER_HZ) - 1;

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Prescaler     = 0;
    TIM_TimeBaseStructure.TIM_Period        = (u16)arr;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(SPWM_TIM, &TIM_TimeBaseStructure);

    /* 先给 50% 占空比, 防止 Start 前输出怪波形 */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = (arr + 1) / 2;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(SPWM_TIM, &TIM_OCInitStructure);

    TIM_OC1PreloadConfig(SPWM_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(SPWM_TIM, ENABLE);

    /* 更新中断: 每个载波周期一次 (20kHz) */
    NVIC_InitStructure.NVIC_IRQChannel                   = SPWM_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 注意: 这里先不使能定时器和中断。等 SPWM_Start() 打开。 */
    SPWM_BuildTable();
}

void SPWM_Start(void)
{
    if (g_spwm.enabled) return;

    SPWM_BuildTable();

    g_spwm.index = 0;
    SPWM_TIM->CCR1 = g_spwm_table[0];
    SPWM_TIM->CNT = 0;

    TIM_ClearITPendingBit(SPWM_TIM, TIM_IT_Update);
    TIM_ITConfig(SPWM_TIM, TIM_IT_Update, ENABLE);
    TIM_Cmd(SPWM_TIM, ENABLE);

    g_spwm.enabled = 1;
}

void SPWM_Stop(void)
{
    TIM_ITConfig(SPWM_TIM, TIM_IT_Update, DISABLE);
    TIM_Cmd(SPWM_TIM, DISABLE);
    TIM_SetCompare1(SPWM_TIM, 0);
    g_spwm.enabled = 0;
}

/* TIM3 更新中断, 20kHz。只做查表和写 CCR。 */
void TIM3_IRQHandler(void)
{
    u16 idx;

    if (SPWM_TIM->SR & TIM_SR_UIF)
    {
        SPWM_TIM->SR = (u16)~TIM_SR_UIF;

        if (!g_spwm.enabled) return;

        idx = (u16)(g_spwm.index + 1U);
        if (idx >= SPWM_POINTS) idx = 0;

        g_spwm.index = idx;
        SPWM_TIM->CCR1 = g_spwm_table[idx];
    }
}
