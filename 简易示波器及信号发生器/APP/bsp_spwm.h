#ifndef __BSP_SPWM_H
#define __BSP_SPWM_H

#include "stm32f4xx.h"
#include "app_config.h"

/* SPWM 状态: 载波 20kHz, 正弦 50Hz, 400 点。
 * 50Hz = 20kHz / 400, 所以载波和正弦是严格整数关系。 */
typedef struct
{
    u8  enabled;
    u32 carrier_hz;
    u32 sine_hz;
    u16 points;
    u16 amp_pct;
    u16 index;
} SPWM_Info;

extern SPWM_Info g_spwm;
extern u16       g_spwm_table[SPWM_POINTS];

void SPWM_Init(void);
void SPWM_Start(void);
void SPWM_Stop(void);

#endif
