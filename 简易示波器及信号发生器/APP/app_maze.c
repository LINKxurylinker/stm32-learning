#include "app_maze.h"
#include "bsp_tick.h"
#include <string.h>

/*==============================================================================
 * app_maze.c
 *-----------------------------------------------------------------------------
 * 长按 START 3 秒进入的迷宫彩蛋。
 *
 * 数据结构:
 *   - 21x21 格, 奇数坐标是通道, 偶数坐标是墙;
 *   - 用迭代 DFS(递归回溯) 生成完美迷宫, 起点 (1,1), 出口 (W-2,H-2);
 *   - 玩家一格一格移动, 撞墙无效;
 *   - AUTO 用 A* 求最短路径, 然后按 MAZE_AUTO_STEP_MS 回放。
 *
 * 为什么用 A*:
 *   - 迷宫不大(441 格), 但分支多; A* 的曼哈顿启发在网格上可采纳,
 *     找到的就是最短路径, 回放步数最少;
 *   - open 表用线性扫描即可, 不用堆, 代码短、RAM 可控。
 *
 * 绘制:
 *   - 整个棋盘只在换页/新局时重画;
 *   - 正常走一步只把"旧格/新格"放进脏队列, 页面重画 2 个格子。
 *============================================================================*/

#define N_MAZE      (MAZE_GRID_W * MAZE_GRID_H)

/* s_maze[cell]: 0=通道, 1=墙 */
static u8  s_maze[N_MAZE];

/* 迷宫生成用的显式栈, 避免 1KB 栈上递归 */
static u16 s_stack[N_MAZE];

/* A* 工作区 */
static u16 s_open[N_MAZE];
static u16 s_g[N_MAZE];
static s16 s_parent[N_MAZE];
static u8  s_open_flag[N_MAZE];
static u8  s_closed[N_MAZE];
static u8  s_path[N_MAZE];

/* 脏格子队列 */
static u16 s_dirty[MAZE_DIRTY_MAX];
static u8  s_dirty_n;

/* 随机数/按键状态 */
static u32 s_rng;
static u32 s_game_seq;
static u8  s_key_prev;
static u32 s_key_lock_ms;

MAZE_Game g_maze;

static const s8 s_dx[4] = {  0, 1,  0, -1 };
static const s8 s_dy[4] = { -1, 0,  1,  0 };

/*------------------------------ 基础工具 ----------------------------------*/
static u16 Mz_Idx(u8 x, u8 y)
{
    return (u16)((u16)y * MAZE_GRID_W + x);
}

static u8 Mz_Walkable(u8 x, u8 y)
{
    if (x < 1 || x >= (MAZE_GRID_W - 1)) return 0;
    if (y < 1 || y >= (MAZE_GRID_H - 1)) return 0;
    return (u8)(s_maze[Mz_Idx(x, y)] == 0);
}

static u32 Mz_Rand(void)
{
    s_rng = s_rng * 1664525UL + 1013904223UL;
    return (s_rng >> 16);
}

static void Mz_Seed(void)
{
    s_game_seq++;
    s_rng = TICK_Now() + s_game_seq * 2654435761UL + 0x5A5A5A5AUL;
    if (s_rng == 0) s_rng = 1;
}

/*------------------------------ 迷宫生成 ----------------------------------*/
/* 迭代版递归回溯:
 *   从 (1,1) 出发, 每次随机挑一个"隔一格"的未访问通道,
 *   打通中间那面墙, 压栈继续; 走不动就弹栈。
 * 生成的迷宫连通、无环, 任意两点只有一条路。 */
static void Mz_Generate(void)
{
    u16 top;
    u16 cur;
    u16 nc;
    u16 cand[4];
    u8  cx;
    u8  cy;
    u8  nx;
    u8  ny;
    u8  n;
    u8  i;

    memset(s_maze, 1, N_MAZE);

    s_maze[Mz_Idx(1, 1)] = 0;
    top = 0;
    s_stack[top++] = Mz_Idx(1, 1);

    while (top > 0)
    {
        cur = s_stack[top - 1];
        cx = (u8)(cur % MAZE_GRID_W);
        cy = (u8)(cur / MAZE_GRID_W);
        n = 0;

        if (cx >= 3)
        {
            nc = Mz_Idx((u8)(cx - 2), cy);
            if (s_maze[nc] != 0) cand[n++] = nc;
        }
        if ((u8)(cx + 2) <= (MAZE_GRID_W - 2))
        {
            nc = Mz_Idx((u8)(cx + 2), cy);
            if (s_maze[nc] != 0) cand[n++] = nc;
        }
        if (cy >= 3)
        {
            nc = Mz_Idx(cx, (u8)(cy - 2));
            if (s_maze[nc] != 0) cand[n++] = nc;
        }
        if ((u8)(cy + 2) <= (MAZE_GRID_H - 2))
        {
            nc = Mz_Idx(cx, (u8)(cy + 2));
            if (s_maze[nc] != 0) cand[n++] = nc;
        }

        if (n == 0)
        {
            top--;
            continue;
        }

        i = (u8)(Mz_Rand() % (u32)n);
        nc = cand[i];
        nx = (u8)(nc % MAZE_GRID_W);
        ny = (u8)(nc / MAZE_GRID_W);

        /* 打通当前格和 nc 中间的墙 */
        s_maze[Mz_Idx((u8)((cx + nx) / 2), (u8)((cy + ny) / 2))] = 0;
        s_maze[nc] = 0;
        s_stack[top++] = nc;
    }

    /* 起点/出口必须是通道 */
    s_maze[Mz_Idx(1, 1)] = 0;
    s_maze[Mz_Idx((MAZE_GRID_W - 2), (MAZE_GRID_H - 2))] = 0;
}

/*------------------------------ A* ----------------------------------------*/
static u16 Mz_H(u16 cell)
{
    int dx = (int)(cell % MAZE_GRID_W) - (int)g_maze.goal_x;
    int dy = (int)(cell / MAZE_GRID_W) - (int)g_maze.goal_y;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (u16)(dx + dy);
}

static u8 Mz_DirBetween(u16 from, u16 to)
{
    u8 fx = (u8)(from % MAZE_GRID_W);
    u8 fy = (u8)(from / MAZE_GRID_W);
    u8 tx = (u8)(to % MAZE_GRID_W);
    u8 ty = (u8)(to / MAZE_GRID_W);

    if (tx > fx) return (u8)MAZE_RIGHT;
    if (tx < fx) return (u8)MAZE_LEFT;
    if (ty > fy) return (u8)MAZE_DOWN;
    return (u8)MAZE_UP;
}

/* 返回路径长度; 路径方向放在 s_path[]。0=当前就在出口/无路 */
static u16 Mz_AStar(void)
{
    u16 start;
    u16 goal;
    u16 open_n;
    u16 i;
    u16 best;
    u16 cur;
    u16 nxt;
    u16 ng;
    u16 cnt;
    u16 idx;
    u16 p;
    u32 fbest;
    u32 fnew;
    u8  d;
    s16 nx;
    s16 ny;

    start = Mz_Idx(g_maze.player_x, g_maze.player_y);
    goal  = Mz_Idx(g_maze.goal_x, g_maze.goal_y);

    if (start == goal) return 0;

    for (i = 0; i < N_MAZE; i++)
    {
        s_g[i] = 0xFFFF;
        s_parent[i] = -1;
        s_open_flag[i] = 0;
        s_closed[i] = 0;
    }

    s_g[start] = 0;
    s_open[0] = start;
    open_n = 1;
    s_open_flag[start] = 1;

    while (open_n > 0)
    {
        best = 0;
        fbest = (u32)s_g[s_open[0]] + (u32)Mz_H(s_open[0]);
        for (i = 1; i < open_n; i++)
        {
            fnew = (u32)s_g[s_open[i]] + (u32)Mz_H(s_open[i]);
            if (fnew < fbest)
            {
                fbest = fnew;
                best = i;
            }
        }

        cur = s_open[best];
        s_open[best] = s_open[open_n - 1];
        open_n--;
        s_open_flag[cur] = 0;

        if (cur == goal) break;
        s_closed[cur] = 1;

        for (d = 0; d < 4; d++)
        {
            nx = (s16)(cur % MAZE_GRID_W) + s_dx[d];
            ny = (s16)(cur / MAZE_GRID_W) + s_dy[d];

            if (nx < 0 || nx >= (s16)MAZE_GRID_W) continue;
            if (ny < 0 || ny >= (s16)MAZE_GRID_H) continue;
            if (!Mz_Walkable((u8)nx, (u8)ny)) continue;

            nxt = Mz_Idx((u8)nx, (u8)ny);
            if (s_closed[nxt]) continue;

            ng = (u16)(s_g[cur] + 1);
            if (ng < s_g[nxt])
            {
                s_g[nxt] = ng;
                s_parent[nxt] = (s16)cur;

                if (!s_open_flag[nxt])
                {
                    s_open[open_n] = nxt;
                    open_n++;
                    s_open_flag[nxt] = 1;
                }
            }
        }
    }

    if (s_parent[goal] == -1) return 0;

    /* 反向数出路径长度, 再反向填方向 */
    cnt = 0;
    cur = goal;
    while (cur != start)
    {
        cnt++;
        cur = (u16)s_parent[cur];
    }

    idx = cnt;
    cur = goal;
    while (cur != start)
    {
        p = (u16)s_parent[cur];
        s_path[--idx] = Mz_DirBetween(p, cur);
        cur = p;
    }

    return cnt;
}

/*------------------------------ 脏格子 ------------------------------------*/
static void Mz_MarkDirty(u16 cell)
{
    u8 i;

    if (cell >= (u16)N_MAZE) return;

    for (i = 0; i < s_dirty_n; i++)
    {
        if (s_dirty[i] == cell) return;
    }

    if (s_dirty_n < MAZE_DIRTY_MAX)
    {
        s_dirty[s_dirty_n] = cell;
        s_dirty_n++;
    }
}

void MAZE_ClearDirty(void)
{
    s_dirty_n = 0;
}

u8 MAZE_PopDirtyCell(u16 *cell)
{
    u8 i;

    if (s_dirty_n == 0) return 0;

    *cell = s_dirty[0];
    for (i = 1; i < s_dirty_n; i++)
    {
        s_dirty[i - 1] = s_dirty[i];
    }
    s_dirty_n--;
    return 1;
}

/*------------------------------ 按键 ---------------------------------------*/
#if MAZE_KEYS_ENABLE && !defined(MAZE_HOST_TEST)
static u8 Mz_KeyMask(void)
{
    u8 mask = 0;

    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)) mask |= 0x01; /* WK_UP */
    if (!GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4)) mask |= 0x02; /* KEY0 */
    if (!GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_3)) mask |= 0x04; /* KEY1 */
    if (!GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_2)) mask |= 0x08; /* KEY2 */
    return mask;
}

static u8 Mz_KeyPoll(void)
{
    u8  mask;
    u8  pressed;
    u32 now;

    mask = Mz_KeyMask();
    pressed = (u8)(mask & (u8)(~s_key_prev));
    s_key_prev = mask;

    if (pressed == 0) return 0xFF;

    now = TICK_Now();
    if ((u32)(now - s_key_lock_ms) < 150UL) return 0xFF;
    s_key_lock_ms = now;

    if (pressed & 0x01) return (u8)MAZE_UP;
    if (pressed & 0x02) return (u8)MAZE_LEFT;
    if (pressed & 0x04) return (u8)MAZE_DOWN;
    if (pressed & 0x08) return (u8)MAZE_RIGHT;
    return 0xFF;
}
#else
#define Mz_KeyPoll()  0xFF
#endif

/*------------------------------ 初始化/开局 --------------------------------*/
void MAZE_Init(void)
{
    memset(&g_maze, 0, sizeof(g_maze));
    g_maze.state = MAZE_IDLE;
    g_maze.player_x = 1;
    g_maze.player_y = 1;
    g_maze.goal_x = (u8)(MAZE_GRID_W - 2);
    g_maze.goal_y = (u8)(MAZE_GRID_H - 2);

    memset(s_maze, 1, N_MAZE);
    s_dirty_n = 0;
    s_key_prev = 0;
    s_key_lock_ms = 0;

#if MAZE_KEYS_ENABLE && !defined(MAZE_HOST_TEST)
    {
        GPIO_InitTypeDef gpio;

        /* KEY0/1/2 = PE4/PE3/PE2, 按下为低; 内部上拉。
         * PA0 已被 TIM5_CH1 捕获初始化, 这里只读 IDR, 不改配置。 */
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

        gpio.GPIO_Pin   = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
        gpio.GPIO_Mode  = GPIO_Mode_IN;
        gpio.GPIO_OType = GPIO_OType_PP;
        gpio.GPIO_Speed = GPIO_Speed_50MHz;
        gpio.GPIO_PuPd  = GPIO_PuPd_UP;
        GPIO_Init(GPIOE, &gpio);
    }
    s_key_prev = Mz_KeyMask();   /* 避免 WK_UP 空闲电平被当成第一次按下 */
#endif
}

void MAZE_Start(void)
{
    Mz_Seed();
    Mz_Generate();

    g_maze.state = MAZE_RUNNING;
    g_maze.auto_mode = 0;
    g_maze.player_x = 1;
    g_maze.player_y = 1;
    g_maze.goal_x = (u8)(MAZE_GRID_W - 2);
    g_maze.goal_y = (u8)(MAZE_GRID_H - 2);
    g_maze.steps = 0;
    g_maze.elapsed_ms = 0;
    g_maze.auto_len = 0;
    g_maze.auto_idx = 0;
    g_maze.last_tick_ms = TICK_Now();
    g_maze.last_step_ms = g_maze.last_tick_ms;

    MAZE_ClearDirty();
}

void MAZE_StartAuto(void)
{
    if (g_maze.state == MAZE_IDLE || g_maze.state == MAZE_WIN)
    {
        MAZE_Start();
    }

    g_maze.auto_mode = 1;
    g_maze.state = MAZE_RUNNING;
    g_maze.auto_idx = 0;
    g_maze.auto_len = Mz_AStar();
    g_maze.last_step_ms = TICK_Now();
    g_maze.last_tick_ms = g_maze.last_step_ms;

    MAZE_ClearDirty();

    /* 理论上完美迷宫一定有路; 没有就退回手动, 防止卡死 */
    if (g_maze.auto_len == 0 &&
        !(g_maze.player_x == g_maze.goal_x && g_maze.player_y == g_maze.goal_y))
    {
        g_maze.auto_mode = 0;
    }
}

void MAZE_SetAuto(u8 on)
{
    if (on)
    {
        MAZE_StartAuto();
    }
    else
    {
        g_maze.auto_mode = 0;
        g_maze.auto_len = 0;
        g_maze.auto_idx = 0;
        g_maze.last_step_ms = TICK_Now();
        g_maze.last_tick_ms = g_maze.last_step_ms;
    }
}

void MAZE_TogglePause(void)
{
    if (g_maze.state == MAZE_RUNNING)
    {
        g_maze.state = MAZE_PAUSED;
    }
    else if (g_maze.state == MAZE_PAUSED)
    {
        g_maze.state = MAZE_RUNNING;
        g_maze.last_tick_ms = TICK_Now();
        g_maze.last_step_ms = g_maze.last_tick_ms;
    }
}

/*------------------------------ 走一步 ------------------------------------*/
u8 MAZE_Move(MAZE_Dir d)
{
    s16 nx;
    s16 ny;
    u16 old_cell;
    u16 new_cell;

    if (g_maze.state != MAZE_RUNNING) return 0;
    if ((u8)d > (u8)MAZE_LEFT) return 0;

    nx = (s16)g_maze.player_x + s_dx[d];
    ny = (s16)g_maze.player_y + s_dy[d];

    if (nx < 0 || nx >= (s16)MAZE_GRID_W) return 0;
    if (ny < 0 || ny >= (s16)MAZE_GRID_H) return 0;
    if (!Mz_Walkable((u8)nx, (u8)ny)) return 0;

    old_cell = Mz_Idx(g_maze.player_x, g_maze.player_y);
    new_cell = Mz_Idx((u8)nx, (u8)ny);

    Mz_MarkDirty(old_cell);

    g_maze.player_x = (u8)nx;
    g_maze.player_y = (u8)ny;
    g_maze.steps++;

    Mz_MarkDirty(new_cell);

    if (g_maze.player_x == g_maze.goal_x &&
        g_maze.player_y == g_maze.goal_y)
    {
        g_maze.state = MAZE_WIN;
    }

    return 1;
}

/*------------------------------ 主 tick -----------------------------------*/
void MAZE_Tick(u32 now_ms)
{
    u32 dt;
    u8  dir;
    u8  burst;

    if (g_maze.state != MAZE_RUNNING)
    {
        g_maze.last_tick_ms = now_ms;
        return;
    }

    dt = (u32)(now_ms - g_maze.last_tick_ms);
    if (dt > 1000UL) dt = 1000UL;   /* 卡顿/调试时不要一次加太多 */
    g_maze.elapsed_ms += dt;
    g_maze.last_tick_ms = now_ms;

    if (g_maze.auto_mode)
    {
        burst = 0;

        while (g_maze.state == MAZE_RUNNING &&
               g_maze.auto_mode &&
               g_maze.auto_idx < g_maze.auto_len &&
               (u32)(now_ms - g_maze.last_step_ms) >= MAZE_AUTO_STEP_MS &&
               burst < 4)
        {
            dir = s_path[g_maze.auto_idx];
            if (!MAZE_Move((MAZE_Dir)dir))
            {
                g_maze.auto_mode = 0;
                break;
            }
            g_maze.auto_idx++;
            g_maze.last_step_ms += MAZE_AUTO_STEP_MS;
            burst++;
        }

        if ((u32)(now_ms - g_maze.last_step_ms) > 500UL)
        {
            g_maze.last_step_ms = now_ms;
        }

        if (g_maze.state == MAZE_RUNNING && g_maze.auto_idx >= g_maze.auto_len)
        {
            g_maze.auto_mode = 0;
        }
    }
    else
    {
        dir = Mz_KeyPoll();
        if (dir != 0xFF)
        {
            MAZE_Move((MAZE_Dir)dir);
        }
    }
}

/*------------------------------ 查询 --------------------------------------*/
u8 MAZE_GetState(void)
{
    return g_maze.state;
}

u8 MAZE_GetAuto(void)
{
    return g_maze.auto_mode;
}

u16 MAZE_GetSteps(void)
{
    return g_maze.steps;
}

u32 MAZE_GetElapsedMs(void)
{
    return g_maze.elapsed_ms;
}

u8 MAZE_GetPlayerX(void)
{
    return g_maze.player_x;
}

u8 MAZE_GetPlayerY(void)
{
    return g_maze.player_y;
}

u8 MAZE_GetGoalX(void)
{
    return g_maze.goal_x;
}

u8 MAZE_GetGoalY(void)
{
    return g_maze.goal_y;
}

u8 MAZE_GetCell(u8 x, u8 y)
{
    if (x >= MAZE_GRID_W || y >= MAZE_GRID_H) return 1;
    return s_maze[Mz_Idx(x, y)];
}

u16 MAZE_GetAutoPathLen(void)
{
    return g_maze.auto_len;
}
