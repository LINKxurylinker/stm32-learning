#ifndef __ADC_H
#define __ADC_H

#include "stm32f4xx.h"

/* Dual ADC regular simultaneous: PA4/ADC1_IN4 (CH1) + PA5/ADC2_IN5 (CH2).
 * PA4 is also a DAC output pin; its output buffer is disabled in ADC1_Init.
 * One TIM3 trigger samples BOTH channels at the same instant; DMA mode 2
 * packs the two 16-bit results into one 32-bit word. */
#define ADC_DMA_BUF_LEN   4096U   /* words: 2 frames x 2048 pairs */
#define ADC_FRAME_LEN     2048U

#define ADC_CH1           0U
#define ADC_CH2           1U

void ADC1_Init(void);
u8   ADC1_CopyFrame(void);
const u16 *ADC1_Frame(void);
const u16 *ADC2_Frame(void);
u16  ADC1_TriggerIndex(u8 edge_rise, u8 manual, u16 level);
u16  ADC2_TriggerIndex(u8 edge_rise, u8 manual, u16 level);
/* Same search, but reports whether a trigger was really found.
 * Returns 1 and stores the index in *idx; returns 0 when not found. */
u8   ADC1_FindTrigger(u8 edge_rise, u8 manual, u16 level, u16 *idx);
u8   ADC2_FindTrigger(u8 edge_rise, u8 manual, u16 level, u16 *idx);
void ADC1_SetSampleRate(u32 fs);
u32  ADC1_GetSampleRate(void);

#endif
