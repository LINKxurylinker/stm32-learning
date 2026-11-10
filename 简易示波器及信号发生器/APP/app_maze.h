#ifndef __APP_MAZE_H
#define __APP_MAZE_H

#include "stm32f4xx.h"
#include "app_config.h"

/* 迷宫页的脏格子队列长度: 手动/自动一步最多改 2 个格子 */
#define MAZE_DIRTY_MAX      8

typedef enum
{
    MAZE_IDLE = 0,
    MAZE_RUNNING,
    MAZE_PAUSED,
    MAZE_WIN
} MAZE_State;

/* 方向编号和 A* 的邻居顺序一致 */
typedef enum
{
    MAZE_UP = 0,
    MAZE_RIGHT,
    MAZE_DOWN,
    MAZE_LEFT
} MAZE_Dir;

typedef struct
{
    u8  state;          /* MAZE_State */
    u8  auto_mode;      /* 1=A* 自动回放中 */
    u8  player_x;       /* 当前所在格 */
    u8  player_y;
    u8  goal_x;         /* 出口 */
    u8  goal_y;
    u16 steps;          /* 玩家/A* 走过的步数 */
    u32 elapsed_ms;     /* 运行时间(暂停不计) */
    u16 auto_len;       /* A* 路径长度 */
    u16 auto_idx;       /* 下一步在 s_path[] 里的下标 */
    u32 last_step_ms;   /* 自动回放节拍 */
    u32 last_tick_ms;   /* 计算 elapsed_ms 用 */
} MAZE_Game;

extern MAZE_Game g_maze;

/* 生命周期 */
void MAZE_Init(void);
void MAZE_Start(void);          /* 新迷宫, 手动 */
void MAZE_StartAuto(void);      /* 从当前位置开始 A* 求解 */
void MAZE_SetAuto(u8 on);
void MAZE_TogglePause(void);
void MAZE_Tick(u32 now_ms);

/* 走一步: 1=移动成功, 0=撞墙/不在运行态 */
u8 MAZE_Move(MAZE_Dir d);

/* 查询接口 */
u8  MAZE_GetState(void);
u8  MAZE_GetAuto(void);
u16 MAZE_GetSteps(void);
u32 MAZE_GetElapsedMs(void);
u8  MAZE_GetPlayerX(void);
u8  MAZE_GetPlayerY(void);
u8  MAZE_GetGoalX(void);
u8  MAZE_GetGoalY(void);
u8  MAZE_GetCell(u8 x, u8 y);   /* 0=通道, 1=墙 */
u16 MAZE_GetAutoPathLen(void);

/* 增量绘制: app_maze.c 把变化的格子放进队列, 页面取走后重画 */
void MAZE_ClearDirty(void);
u8   MAZE_PopDirtyCell(u16 *cell);

#endif
