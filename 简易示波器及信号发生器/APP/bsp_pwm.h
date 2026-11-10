#ifndef __BSP_PWM_H
#define __BSP_PWM_H

#include "stm32f4xx.h"

/* 主 PWM 的"对外状态"。UI 只读这个结构, 不直接碰寄存器。
 * 这样算法改坏了, 界面层不用跟着改。 */
typedef struct
{
    u32 freq_set_hz;        /* 你设定/输入的频率 */
    u32 freq_actual_hz;     /* 按 PSC/ARR 反算出来的真实频率 */
    u16 duty_set_pct;       /* 设定占空比 0..100 */
    u16 duty_actual_pct;    /* 由 CCR/ARR 反算的真实占空比 */
    u16 psc;                /* 当前预分频 */
    u32 arr;                /* 当前重装载值 (TIM2 是 32 位) */
    u32 ccr;                /* 当前比较值 */
    u8  running;            /* 1=定时器在跑 */
} PWM_Info;

extern PWM_Info g_pwm;
extern u16      g_pwm2_phase_deg;
extern u8       g_pwm2_enabled;

void PWM_Init(void);                             /* 初始化 + 自检输出 */
u8   PWM_Set(u32 freq_hz, u16 duty_pct);         /* 核心: 算 PSC/ARR/CCR 并生效 */
u8   PWM_SetFreq(u32 freq_hz);
u8   PWM_SetDuty(u16 duty_pct);                  /* 两路同时设 */
u8   PWM_SetDutyCH1(u16 duty_pct);               /* 只改 CH1, CH2 保持 */
void PWM_Enable(u8 en);

u32  PWM_GetFreq(void);
u16  PWM_GetDuty(void);

/* 拓展1: 第二路 (TIM8_CH1 / PC6) */
void PWM2_Init(void);
u8   PWM2_SetDuty(u16 duty_pct);
u8   PWM2_SetPhaseDeg(u16 deg);                  /* 0..360, 5度一档 */
u16  PWM2_GetPhaseDeg(void);
void PWM2_Enable(u8 en);
u16  PWM2_GetDuty(void);

#endif
