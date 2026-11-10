#ifndef __APP_SNAKE_H
#define __APP_SNAKE_H

#include "stm32f4xx.h"
#include "app_config.h"

/*==============================================================================
 * 贪吃蛇 (最后一页 / 从 MAZE 页进入)
 *-----------------------------------------------------------------------------
 * 逻辑与显示分离:
 *   - 这个文件只管游戏状态机、碰撞、计分、自动模式;
 *   - 棋盘/按钮/分数在 ui_pages.c 的 SNAKE 页里画。
 *
 * 自动模式:
 *   20x20 网格上预生成一条哈密顿回路(每个格子恰好经过一次),
 *   蛇默认沿回路走; 每一步再尝试"安全捷径"去追食物:
 *   先虚拟地走一步捷径, 然后沿回路模拟 len+1 步(预留最多
 *   SNAKE_AUTO_FOOD_FOUND 次吃到食物导致尾巴不缩的情况),
 *   不撞到自己才真的走。这样既能吃到食物, 又不会把自己困死,
 *   最终 400 格全部吃满 -> SNAKE_WIN。
 *   自动模式必须从 SNAKE_StartAuto() 开始, 因为初始蛇身要
 *   按哈密顿回路对齐; 中途打开自动时页面会自动重开一局。
 *============================================================================*/

typedef enum
{
    SNAKE_IDLE = 0,
    SNAKE_RUNNING,
    SNAKE_PAUSED,
    SNAKE_OVER,
    SNAKE_WIN
} SNAKE_State;

typedef enum
{
    SNAKE_UP = 0,
    SNAKE_DOWN,
    SNAKE_LEFT,
    SNAKE_RIGHT
} SNAKE_Dir;

typedef struct
{
    u8  body_x[SNAKE_MAX_LEN];
    u8  body_y[SNAKE_MAX_LEN];
    u16 len;                /* 当前长度, body[0] 是蛇头 */
    u8  dir;                /* 当前方向 (SNAKE_Dir) */
    u8  next_dir;           /* 缓冲的下一个方向, 防止一帧转两次 */
    u8  food_x;
    u8  food_y;
    u32 score;              /* 吃到食物的个数 */
    u16 score_limit;        /* AUTO 模式得分上限, 1..400 */
    u8  state;              /* SNAKE_State */
    u8  auto_mode;          /* 1=自动模式 */
    u32 last_step_ms;       /* 上一次移动的时间 */
    u32 moves;              /* 累计移动步数 */
} SNAKE_Game;

extern SNAKE_Game g_snake;

void SNAKE_Init(void);          /* 建哈密顿回路/初始化按键, 不开始游戏 */
void SNAKE_Start(void);         /* 手动模式重开一局 */
void SNAKE_StartAuto(void);     /* 自动模式重开一局 (蛇身按回路对齐) */
void SNAKE_Reset(void);         /* 回到 IDLE, 清分数/长度 */
void SNAKE_TogglePause(void);   /* RUNNING <-> PAUSED */
void SNAKE_SetDir(SNAKE_Dir d); /* 手动转向; 自动模式下忽略 */
void SNAKE_SetAuto(u8 on);      /* on=1 重开一局自动; on=0 保留当前局面 */

/* 主循环里反复调用, 非阻塞。手动按 SNAKE_STEP_MS 走,
 * 自动按 SNAKE_AUTO_STEP_MS 时间片快速推进, 不阻塞。 */
void SNAKE_Tick(u32 now_ms);

u8   SNAKE_GetState(void);
u32  SNAKE_GetScore(void);
u16  SNAKE_GetScoreLimit(void);
void SNAKE_SetScoreLimit(u16 limit);

/* 增量绘制接口:
 * 游戏逻辑把"这一步发生变化的格子"记到队列里, 页面取走后只重画
 * 这些格子, 避免每走一步就整屏重画。返回值: 1=取到格子, 0=队列空。 */
u8   SNAKE_PopDirtyCell(u16 *cell);
void SNAKE_ClearDirty(void);

#endif
