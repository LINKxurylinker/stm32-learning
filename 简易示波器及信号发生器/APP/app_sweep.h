#ifndef __APP_SWEEP_H
#define __APP_SWEEP_H

#include "stm32f4xx.h"
#include "app_config.h"

typedef enum
{
    SWEEP_IDLE = 0,
    SWEEP_SET,        /* 设置 PWM 到当前频点 */
    SWEEP_WAIT,       /* 等捕获稳定 */
    SWEEP_MEASURE,    /* 读捕获, 记录一行 */
    SWEEP_NEXT,       /* 下一个频点 */
    SWEEP_DONE        /* 全部完成 */
} SWEEP_State;

typedef struct
{
    u32 set_hz;         /* 设定频率 */
    u32 meas_hz;        /* 实测频率 */
    u16 meas_duty_pm;   /* 实测占空比 x1000 */
    int32_t err_x100;   /* 频率误差 x100, 例如 0.35% -> 35 */
    u8  pass;           /* 1=该点达标 */
} SWEEP_Row;

void SWEEP_Init(void);
void SWEEP_Start(void);
void SWEEP_Abort(void);
void SWEEP_Poll(void);          /* 主循环里反复调用, 必须非阻塞 */

u8  SWEEP_IsActive(void);
u8  SWEEP_IsDone(void);
u8  SWEEP_GetPass(void);
u8  SWEEP_GetCount(void);
const SWEEP_Row *SWEEP_GetRows(void);

#endif
