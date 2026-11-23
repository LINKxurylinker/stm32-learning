#ifndef __CMD_H
#define __CMD_H

#include "stm32f4xx.h"

/* Trigger modes (requirement: AUTO / NORMAL / SINGLE) */
#define CMD_MODE_AUTO      0
#define CMD_MODE_NORM      1
#define CMD_MODE_SINGLE    2

void CMD_Init(void);
void CMD_Task(void);

/* state read back */
u8  CMD_IsRunning(void);
u8  CMD_GetMode(void);
u8  CMD_GetSingleArmed(void);
u8  CMD_GetEdgeRise(void);
u8  CMD_GetTrigManual(void);
u16 CMD_GetTrigLevel(void);
u32 CMD_GetTimebaseUs(void);
u32 CMD_GetDisplayTimebaseUs(void);
u16 CMD_GetDisplaySpan(void);
u16 CMD_GetVdivMv(void);
u16 CMD_GetVcenterMv(void);
u16 CMD_GetVdivMvCh(u8 ch);
u16 CMD_GetVcenterMvCh(u8 ch);
u8  CMD_GetAutoTimebase(void);
u8  CMD_GetAutoVdiv(void);
void CMD_ToggleRun(void);
void CMD_EnableAutoRange(void);
void CMD_ToggleAutoRange(void);
void CMD_SetAutoPause(u8 pause);
u8   CMD_GetChannelEnabled(u8 ch);
void CMD_ToggleChannel(u8 ch);
void CMD_SetChannelEnabled(u8 ch, u8 en);

/* UI setters: the touch panel uses the same state as the serial commands */
void CMD_SetRunning(u8 run);
void CMD_ArmSingle(void);
void CMD_SetMode(u8 mode);
void CMD_CycleMode(void);                 /* AUTO -> NORM -> SINGLE  */
void CMD_CycleEdge(void);                 /* RISE <-> FALL          */
void CMD_StepTimebase(s8 dir);            /* +1 / -1 through table  */
void CMD_SetTimebaseUs(u32 us);
void CMD_StepVdiv(s8 dir);                /* +1 / -1 through table  */
void CMD_SetVdivMv(u16 mv);
void CMD_StepTrigLevel(s16 delta);        /* raw ADC counts         */
void CMD_SetTrigLevelRaw(u16 raw);        /* also forces manual      */
void CMD_EnableAutoTrigLevel(void);

/* Display frame: already gated by the trigger mode (NORM/SINGLE hold
 * the last accepted frame), so the UI and MEAS use exactly what is shown. */
const u16 *CMD_DisplayFrame(void);
const u16 *CMD_DisplayFrame2(void);
u16 CMD_DisplayTrig(void);
u8  CMD_DisplayFound(void);
u32 CMD_DisplaySeq(void);
u16 CMD_DisplaySpan(void);
u8  CMD_GetWaveType(void);

#endif
