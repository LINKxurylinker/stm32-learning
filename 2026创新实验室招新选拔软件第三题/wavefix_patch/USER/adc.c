#include "adc.h"
#include "delay.h"

/* TIM3 TRGO -> ADC1_IN4 (PA4), DMA2 Stream0 circular.
 * TIM3: 84MHz / (419 + 1) = 200kHz.
 * ADC: 15 + 12 cycles at 21MHz = 1.29us < 5us. */
volatile u16 g_adc_dma_buf[ADC_DMA_BUF_LEN];
static volatile u8  s_frame_ready = 0;
static volatile u16 s_frame_offset = 0;
static u16 s_frame[ADC_FRAME_LEN];
static u16 s_raw_frame[ADC_FRAME_LEN];
static u32 s_sample_rate = 200000U;

void DMA2_Stream0_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA2_Stream0, DMA_IT_HTIF0) != RESET)
    {
        DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_HTIF0);
        s_frame_offset = 0;
        s_frame_ready = 1;
    }

    if (DMA_GetITStatus(DMA2_Stream0, DMA_IT_TCIF0) != RESET)
    {
        DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0);
        s_frame_offset = ADC_FRAME_LEN;
        s_frame_ready = 1;
    }
}

static u16 adc_median3(u16 a, u16 b, u16 c)
{
    if (a > b) { u16 t = a; a = b; b = t; }
    if (b > c) { u16 t = b; b = c; c = t; }
    if (a > b) { u16 t = a; a = b; b = t; }
    return b;
}

u8 ADC1_CopyFrame(void)
{
    u16 i;
    u16 offset;

    /* A late read sets OVR; once set the ADC/DMA stream can stop.
     * Clearing it here keeps the acquisition running without a reset. */
    if (ADC_GetFlagStatus(ADC1, ADC_FLAG_OVR) != RESET)
    {
        ADC_ClearFlag(ADC1, ADC_FLAG_OVR);
    }

    if (s_frame_ready == 0) return 0;

    s_frame_ready = 0;
    offset = s_frame_offset;

    for (i = 0; i < ADC_FRAME_LEN; i++)
    {
        s_raw_frame[i] = g_adc_dma_buf[offset + i];
    }

    /* 3-point median: removes single-sample spikes without the
     * bandwidth loss of a wide moving average. */
    s_frame[0] = s_raw_frame[0];
    s_frame[ADC_FRAME_LEN - 1] = s_raw_frame[ADC_FRAME_LEN - 1];
    for (i = 1; i < ADC_FRAME_LEN - 1; i++)
    {
        s_frame[i] = adc_median3(s_raw_frame[i - 1],
                                 s_raw_frame[i],
                                 s_raw_frame[i + 1]);
    }

    return 1;
}

const u16 *ADC1_Frame(void)
{
    return s_frame;
}

u8 ADC1_FindTrigger(u8 edge_rise, u8 manual, u16 level, u16 *idx)
{
    u16 i;
    u16 min = 4095;
    u16 max = 0;
    u32 span;

    if (idx != 0) *idx = 0;

    if (manual)
    {
        if (edge_rise)
        {
            for (i = 1; i < ADC_FRAME_LEN; i++)
                if (s_frame[i - 1] < level && s_frame[i] >= level)
                {
                    if (idx != 0) *idx = i;
                    return 1;
                }
        }
        else
        {
            for (i = 1; i < ADC_FRAME_LEN; i++)
                if (s_frame[i - 1] > level && s_frame[i] <= level)
                {
                    if (idx != 0) *idx = i;
                    return 1;
                }
        }
        return 0;
    }

    for (i = 0; i < ADC_FRAME_LEN; i++)
    {
        if (s_frame[i] < min) min = s_frame[i];
        if (s_frame[i] > max) max = s_frame[i];
    }

    span = (u32)max - min;
    if (span < 80U) return 0;

    /* Arm below/above a mid-level plus hysteresis, then trigger on the
     * actual mid-level crossing.  The old "one sample must jump from
     * 1/4 to 3/4 of Vpp" test missed filtered square edges and always
     * missed sine waves, which made AUTO hold/refresh alternate. */
    {
        u16 mid  = (u16)((min + max) / 2U);
        u16 hyst = (u16)(span / 8U);
        u8  armed = 0U;

        if (hyst < 4U) hyst = 4U;

        if (edge_rise)
        {
            for (i = 1; i < ADC_FRAME_LEN; i++)
            {
                if ((u32)s_frame[i - 1] + hyst <= mid) armed = 1U;
                if (armed && s_frame[i - 1] < mid && s_frame[i] >= mid)
                {
                    if (idx != 0) *idx = i;
                    return 1;
                }
            }
        }
        else
        {
            for (i = 1; i < ADC_FRAME_LEN; i++)
            {
                if (s_frame[i - 1] >= (u16)(mid + hyst)) armed = 1U;
                if (armed && s_frame[i - 1] > mid && s_frame[i] <= mid)
                {
                    if (idx != 0) *idx = i;
                    return 1;
                }
            }
        }
    }

    return 0;
}

u16 ADC1_TriggerIndex(u8 edge_rise, u8 manual, u16 level)
{
    u16 idx = 0;

    if (ADC1_FindTrigger(edge_rise, manual, level, &idx))
        return idx;

    return 0;
}
/* ADC clock = 84MHz / 4 = 21MHz.
 * Conversion time = (sample cycles + 12) / 21MHz.
 * Choose a longer sample window at low sample rates; keep enough
 * margin at 200kSa/s and above. */
static void adc_apply_sample_time(void)
{
    u8 sample_time;

    if (s_sample_rate <= 40000U)
        sample_time = ADC_SampleTime_480Cycles;
    else if (s_sample_rate <= 100000U)
        sample_time = ADC_SampleTime_84Cycles;
    else
        sample_time = ADC_SampleTime_15Cycles;

    ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, sample_time);
}

void ADC1_Init(void)
{
    s_sample_rate = 200000U;
    GPIO_InitTypeDef      GPIO_InitStructure;
    ADC_CommonInitTypeDef ADC_CommonInitStructure;
    ADC_InitTypeDef       ADC_InitStructure;
    DMA_InitTypeDef       DMA_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef      NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* PA4 = ADC1_IN4, analog. */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_CommonInitStructure.ADC_Mode             = ADC_Mode_Independent;
    ADC_CommonInitStructure.ADC_Prescaler        = ADC_Prescaler_Div4;
    ADC_CommonInitStructure.ADC_DMAAccessMode    = ADC_DMAAccessMode_Disabled;
    ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&ADC_CommonInitStructure);

    ADC_InitStructure.ADC_Resolution           = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode         = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode   = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising;
    ADC_InitStructure.ADC_ExternalTrigConv     = ADC_ExternalTrigConv_T3_TRGO;
    ADC_InitStructure.ADC_DataAlign            = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion      = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    adc_apply_sample_time();

    /* DMA2 Stream0 Channel0 = ADC1. */
    DMA_DeInit(DMA2_Stream0);
    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_Channel = DMA_Channel_0;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&ADC1->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr = (u32)g_adc_dma_buf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize = ADC_DMA_BUF_LEN;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream0, &DMA_InitStructure);

    DMA_ITConfig(DMA2_Stream0, DMA_IT_HT | DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA2_Stream0, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    ADC_ClearFlag(ADC1, ADC_FLAG_OVR);
    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    delay_ms(1);

    /* TIM3 TRGO = update event, 200kHz. */
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_Period = 419;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);
    TIM_Cmd(TIM3, ENABLE);
}

void ADC1_SetSampleRate(u32 fs)
{
    u32 arr;

    if (fs < 1000U) fs = 1000U;
    if (fs > 500000U) fs = 500000U;

    arr = (84000000U + fs / 2U) / fs;
    if (arr < 2U) arr = 2U;
    arr -= 1U;

    /* Changing TIM3 while the ADC is converting can inject a trigger in
     * the middle of a conversion; the resulting overrun stops the DMA
     * stream, which looked like "only one frame after reset".  Stop the
     * ADC/DMA first, change the timer, clear OVR, then restart cleanly. */
    ADC_Cmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);
    TIM_Cmd(TIM3, DISABLE);

    TIM3->PSC = 0;
    TIM3->ARR = arr;
    TIM3->EGR = 0x0001;                 /* UG: load new ARR */
    TIM3->SR  = (u16)~TIM_FLAG_Update;  /* clear the UG update flag */

    ADC_ClearFlag(ADC1, ADC_FLAG_OVR);
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    DMA_ClearFlag(DMA2_Stream0,
                  DMA_FLAG_TCIF0 | DMA_FLAG_HTIF0 | DMA_FLAG_TEIF0 |
                  DMA_FLAG_DMEIF0 | DMA_FLAG_FEIF0);
    s_frame_ready = 0;

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    /* tSTAB after ADON: a few microseconds are enough. */
    delay_us(10);

    TIM_Cmd(TIM3, ENABLE);

    s_sample_rate = fs;
}

u32 ADC1_GetSampleRate(void)
{
    return s_sample_rate;
}
