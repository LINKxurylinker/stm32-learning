#include "app_snake.h"
#include "bsp_tick.h"
#include "app_config.h"
#include <string.h>

/*==============================================================================
 * app_snake.c
 *-----------------------------------------------------------------------------
 * 最后一页: 贪吃蛇 (从 MAZE 页点 SNAKE 进入)。
 *
 * 分工:
 *   - 本文件: 游戏状态机 / 碰撞 / 食物 / 计分 / 自动模式;
 *   - ui_pages.c: 20x20 棋盘、分数、方向键、AUTO/PAUSE/NEW/BACK。
 *
 * 自动模式的核心:
 *   先在 20x20 网格上生成一条哈密顿回路(不重复地走遍每个格子后回到起点),
 *   蛇沿着回路走一定不会撞死; 每一步再尝试走"安全捷径"追食物 ——
 *   先虚拟走一步捷径, 再沿回路模拟 len+1 步(假设最多吃到
 *   SNAKE_AUTO_FOOD_FOUND 个食物, 尾巴暂时不缩), 不撞自己才采用。
 *   这样既快又能保证最后把 400 格全部吃满, 进入 SNAKE_WIN。
 *
 * 注意:
 *   - 自动模式必须从 SNAKE_StartAuto() 开始, 因为初始蛇身要沿回路对齐;
 *   - 手动模式可以随便玩, 打开 AUTO 时页面会自动重开一局;
 *   - 整块逻辑与显示解耦, 页面只读 g_snake + 增量脏格子队列。
 *============================================================================*/

#define N_CELL          (SNAKE_GRID_W * SNAKE_GRID_H)
#define SNAKE_DIRTY_MAX 32

#if ((SNAKE_GRID_W < 2) || (SNAKE_GRID_H < 2) || ((SNAKE_GRID_W % 2) != 0))
#error "SNAKE_GRID_W must be even and >=2; SNAKE_GRID_H must be >=2"
#endif

SNAKE_Game g_snake;

/*------------------------------ 内部数据 ----------------------------------*/
/* 哈密顿回路: order[i] 是第 i 个格子; next[cell] 是回路上的下一个格子;
 * idx[cell] 是格子 cell 在回路上的序号。 */
static u16 s_cyc_order[N_CELL];
static u16 s_cyc_next[N_CELL];
static u16 s_cyc_idx[N_CELL];

static u8  s_occ[N_CELL];        /* 当前蛇身占位, 1=被占 */
static u8  s_sim_occ[N_CELL];    /* 自动模式安全检查用的临时占位 */
static u16 s_free[N_CELL];       /* 放食物时收集空格 */

/* 增量绘制: 游戏逻辑把变化的格子塞进队列, 页面取走后只重画这些格子 */
static u16 s_dirty[SNAKE_DIRTY_MAX];
static u8  s_dirty_head = 0;
static u8  s_dirty_tail = 0;

static u32 s_rng = 0x12345678UL;
static u32 s_game_seq = 0;
static u8  s_cycle_ok = 0;

#if SNAKE_KEYS_ENABLE && !defined(SNAKE_HOST_TEST)
static u8  s_key_prev = 0;
static u32 s_key_lock_ms = 0;
#endif

/*------------------------------ 小工具 ------------------------------------*/
static u16 Snake_CellOf(u8 x, u8 y)
{
    return (u16)((u16)y * SNAKE_GRID_W + x);
}

/* 从 cell 沿方向 d 走一格; 出界返回 0 */
static u8 Snake_NextCell(u16 cell, u8 d, u16 *out)
{
    int x = (int)(cell % SNAKE_GRID_W);
    int y = (int)(cell / SNAKE_GRID_W);

    switch (d)
    {
        case SNAKE_UP:    if (y <= 0) return 0; y--; break;
        case SNAKE_DOWN:  if (y >= (int)SNAKE_GRID_H - 1) return 0; y++; break;
        case SNAKE_LEFT:  if (x <= 0) return 0; x--; break;
        case SNAKE_RIGHT: if (x >= (int)SNAKE_GRID_W - 1) return 0; x++; break;
        default: return 0;
    }

    *out = (u16)((u16)y * SNAKE_GRID_W + (u8)x);
    return 1;
}

/* 两个相邻格子之间的方向 */
static u8 Snake_DirBetween(u16 from, u16 to)
{
    int fx = (int)(from % SNAKE_GRID_W);
    int fy = (int)(from / SNAKE_GRID_W);
    int tx = (int)(to % SNAKE_GRID_W);
    int ty = (int)(to / SNAKE_GRID_W);

    if (tx > fx) return SNAKE_RIGHT;
    if (tx < fx) return SNAKE_LEFT;
    if (ty > fy) return SNAKE_DOWN;
    return SNAKE_UP;
}

static u32 Snake_Rand(void)
{
    /* 线性同余, 只用高位, 够用且不占 Flash */
    s_rng = s_rng * 1664525UL + 1013904223UL;
    return (s_rng >> 16);
}

/*------------------------------ 脏格子队列 --------------------------------*/
static void Snake_MarkDirty(u16 cell)
{
    u8 i;
    u8 next;

    if (cell >= N_CELL) return;

    /* 去重: 同一步里蛇头/食物可能重复 */
    i = s_dirty_tail;
    while (i != s_dirty_head)
    {
        if (s_dirty[i] == cell) return;
        i = (u8)((i + 1) % SNAKE_DIRTY_MAX);
    }

    next = (u8)((s_dirty_head + 1) % SNAKE_DIRTY_MAX);
    if (next == s_dirty_tail)
    {
        /* 队列满: 丢掉最旧的, 页面下一帧会全画, 不会花屏 */
        s_dirty_tail = (u8)((s_dirty_tail + 1) % SNAKE_DIRTY_MAX);
    }

    s_dirty[s_dirty_head] = cell;
    s_dirty_head = next;
}

u8 SNAKE_PopDirtyCell(u16 *cell)
{
    if (cell == 0) return 0;
    if (s_dirty_tail == s_dirty_head) return 0;

    *cell = s_dirty[s_dirty_tail];
    s_dirty_tail = (u8)((s_dirty_tail + 1) % SNAKE_DIRTY_MAX);
    return 1;
}

void SNAKE_ClearDirty(void)
{
    s_dirty_head = 0;
    s_dirty_tail = 0;
}

/*------------------------------ 哈密顿回路 --------------------------------*/
/* 20x20 偶数宽网格的经典构造:
 *   第 0 列从上往下走到底;
 *   第 1 列从下往上走到第 1 行(不碰第 0 行);
 *   第 2 列从第 1 行往下走到底; 第 3 列再往上 ...
 *   最后一列从下一直走到第 0 行;
 *   再沿第 0 行从右往左回到起点, 闭合成环。
 * 如果改了 SNAKE_GRID_W/H, 先检查这里是否仍然成立。 */
static void Snake_BuildCycle(void)
{
    u16 x, y, n, i, cell;

    n = 0;

    for (x = 0; x < SNAKE_GRID_W; x++)
    {
        if (x == 0)
        {
            for (y = 0; y < SNAKE_GRID_H; y++)
            {
                s_cyc_order[n++] = Snake_CellOf((u8)x, (u8)y);
            }
        }
        else if (x == (u16)(SNAKE_GRID_W - 1))
        {
            y = SNAKE_GRID_H;
            while (y > 0)
            {
                y--;
                s_cyc_order[n++] = Snake_CellOf((u8)x, (u8)y);
            }
        }
        else if ((x & 1) != 0)
        {
            y = SNAKE_GRID_H;
            while (y > 1)
            {
                y--;
                s_cyc_order[n++] = Snake_CellOf((u8)x, (u8)y);
            }
        }
        else
        {
            for (y = 1; y < SNAKE_GRID_H; y++)
            {
                s_cyc_order[n++] = Snake_CellOf((u8)x, (u8)y);
            }
        }
    }

    /* 第 0 行剩下的格子, 从右往左回起点 */
    x = (u16)(SNAKE_GRID_W - 2);
    while (x > 0)
    {
        s_cyc_order[n++] = Snake_CellOf((u8)x, 0);
        x--;
    }

    if (n != N_CELL)
    {
        s_cycle_ok = 0;
        return;
    }

    for (i = 0; i < N_CELL; i++)
    {
        cell = s_cyc_order[i];
        s_cyc_idx[cell] = i;
        s_cyc_next[cell] = s_cyc_order[(u16)((i + 1) % N_CELL)];
    }

    s_cycle_ok = 1;
}

/*------------------------------ 食物 --------------------------------------*/
static void Snake_SpawnFood(void)
{
    u16 n = 0;
    u16 cell;
    u16 pick;

    for (cell = 0; cell < N_CELL; cell++)
    {
        if (!s_occ[cell])
        {
            s_free[n++] = cell;
        }
    }

    if (n == 0)
    {
        /* 没有空格了: 蛇已经占满全屏 */
        g_snake.state = SNAKE_WIN;
        return;
    }

    pick = (u16)(Snake_Rand() % n);
    cell = s_free[pick];

    g_snake.food_x = (u8)(cell % SNAKE_GRID_W);
    g_snake.food_y = (u8)(cell / SNAKE_GRID_W);

    Snake_MarkDirty(cell);
}

/*------------------------------ 自动模式 ----------------------------------*/
/* 先虚拟走一步到 new_cell, 然后沿哈密顿回路走 len+1 步:
 * 假设前 SNAKE_AUTO_FOOD_FOUND 步会吃到食物(尾巴不缩), 之后按正常
 * 长度增长来算。只要中途不撞到自己, 这一步就是安全的。
 * 这是"哈密顿回路 + 安全捷径"算法的核心检查。 */
static u8 Snake_AutoIsSafe(u16 new_cell)
{
    u16 head = new_cell;
    u16 i;
    int tail_i = (int)g_snake.len - 1;
    u8  food_found = SNAKE_AUTO_FOOD_FOUND;

    memcpy(s_sim_occ, s_occ, N_CELL);
    s_sim_occ[new_cell] = 1;

    for (i = 0; i <= g_snake.len; i++)
    {
        u16 nxt = s_cyc_next[head];

        if (food_found > 0 && tail_i >= 0)
        {
            u16 tail_cell = Snake_CellOf(g_snake.body_x[tail_i],
                                         g_snake.body_y[tail_i]);
            s_sim_occ[tail_cell] = 0;
            tail_i--;
            food_found--;
        }

        if (s_sim_occ[nxt])
        {
            return 0;
        }

        s_sim_occ[nxt] = 1;
        head = nxt;
    }

    return 1;
}

/* 自动模式每一步先选方向: 优先选"沿回路离食物最近"的邻居,
 * 安全就采用; 不安全就老老实实沿回路走一格。 */
static void Snake_AutoChooseDir(void)
{
    u16 head = Snake_CellOf(g_snake.body_x[0], g_snake.body_y[0]);
    u16 food = Snake_CellOf(g_snake.food_x, g_snake.food_y);
    u16 best_cell = 0xFFFF;
    u16 best_cost = 0xFFFF;
    u8  best_dir = g_snake.dir;
    u8  d;

    for (d = 0; d < 4; d++)
    {
        u16 cell;
        u16 cost;

        if (!Snake_NextCell(head, d, &cell)) continue;
        if (s_occ[cell]) continue;

        /* 沿回路从 cell 走到食物还要多少步 */
        cost = (u16)(((u16)s_cyc_idx[food] + N_CELL - (u16)s_cyc_idx[cell]) % N_CELL);

        if (cost < best_cost)
        {
            best_cost = cost;
            best_cell = cell;
            best_dir = d;
        }
    }

    if (best_cell != 0xFFFF)
    {
        /* 最后一步: 直接吃就通关, 不用再做安全模拟 */
        if (best_cell == food && (u16)(g_snake.len + 1) >= N_CELL)
        {
            g_snake.next_dir = best_dir;
            return;
        }

        if (Snake_AutoIsSafe(best_cell))
        {
            g_snake.next_dir = best_dir;
            return;
        }
    }

    /* 沿回路走一格 */
    {
        u16 nxt = s_cyc_next[head];
        if (!s_occ[nxt] && nxt != head)
        {
            g_snake.next_dir = Snake_DirBetween(head, nxt);
            return;
        }
    }

    /* 兜底: 随便找一个空格, 实在没有就交给 Snake_Step 判负 */
    for (d = 0; d < 4; d++)
    {
        u16 cell;
        if (!Snake_NextCell(head, d, &cell)) continue;
        if (s_occ[cell]) continue;
        g_snake.next_dir = d;
        return;
    }
}

/*------------------------------ 走一步 ------------------------------------*/
static void Snake_Step(void)
{
    u16 head;
    u16 ncell;
    u16 tail_cell;
    u8  d;
    u8  ate;
    u16 i;

    if (g_snake.len < 2) return;

    head = Snake_CellOf(g_snake.body_x[0], g_snake.body_y[0]);
    d = g_snake.next_dir;

    if (!Snake_NextCell(head, d, &ncell))
    {
        g_snake.state = SNAKE_OVER;
        return;
    }

    ate = (ncell == Snake_CellOf(g_snake.food_x, g_snake.food_y)) ? 1 : 0;
    tail_cell = Snake_CellOf(g_snake.body_x[g_snake.len - 1],
                             g_snake.body_y[g_snake.len - 1]);

    if (s_occ[ncell])
    {
        /* 尾巴这一格下一步会空出来, 可以进; 其他身体格子都是撞 */
        if (!(ate == 0 && ncell == tail_cell))
        {
            g_snake.state = SNAKE_OVER;
            return;
        }
    }

    g_snake.dir = d;

    /* 记录这一步会变的格子(旧蛇头变成身体 / 蛇头 / 旧尾巴) */
    Snake_MarkDirty(head);
    Snake_MarkDirty(ncell);
    if (!ate)
    {
        Snake_MarkDirty(tail_cell);
    }

    if (ate)
    {
        /* 吃到: 尾巴不动, 身体整体后移一格, 长度+1 */
        for (i = g_snake.len; i > 0; i--)
        {
            g_snake.body_x[i] = g_snake.body_x[i - 1];
            g_snake.body_y[i] = g_snake.body_y[i - 1];
        }
        g_snake.len++;
        g_snake.score++;
        g_snake.body_x[0] = (u8)(ncell % SNAKE_GRID_W);
        g_snake.body_y[0] = (u8)(ncell / SNAKE_GRID_W);
        s_occ[ncell] = 1;
        g_snake.moves++;

        /* AUTO 自定义得分上限: 达到上限直接结束, 不要求吃满 400 格 */
        if (g_snake.auto_mode && g_snake.score >= (u32)g_snake.score_limit)
        {
            g_snake.state = SNAKE_WIN;
            return;
        }

        if (g_snake.len >= N_CELL)
        {
            g_snake.state = SNAKE_WIN;
            return;
        }

        Snake_SpawnFood();
    }
    else
    {
        /* 没吃到: 尾巴先空出来, 身体前移 */
        s_occ[tail_cell] = 0;
        for (i = (u16)(g_snake.len - 1); i > 0; i--)
        {
            g_snake.body_x[i] = g_snake.body_x[i - 1];
            g_snake.body_y[i] = g_snake.body_y[i - 1];
        }
        g_snake.body_x[0] = (u8)(ncell % SNAKE_GRID_W);
        g_snake.body_y[0] = (u8)(ncell / SNAKE_GRID_W);
        s_occ[ncell] = 1;
        g_snake.moves++;
    }

    g_snake.next_dir = d;
}

/*------------------------------ 开局 --------------------------------------*/
static void Snake_SeedRng(void)
{
    s_game_seq++;
    s_rng = TICK_Now() + s_game_seq * 2654435761UL + 0x5A5A5A5AUL;
    if (s_rng == 0) s_rng = 1;
}

static void Snake_StartCommon(u8 auto_mode)
{
    u16 head;
    u16 i;

    if (!s_cycle_ok)
    {
        Snake_BuildCycle();
        if (!s_cycle_ok) return;
    }

    memset(s_occ, 0, N_CELL);
    SNAKE_ClearDirty();

    g_snake.state        = SNAKE_RUNNING;
    g_snake.auto_mode    = auto_mode;
    g_snake.score        = 0;
    g_snake.moves        = 0;
    g_snake.len          = 3;
    g_snake.last_step_ms = TICK_Now();

    Snake_SeedRng();

    head = Snake_CellOf((u8)(SNAKE_GRID_W / 2), (u8)(SNAKE_GRID_H / 2));

    if (auto_mode)
    {
        /* 蛇头在中间, 身体沿哈密顿回路往后排, 方向朝回路的下一个格子 */
        u16 idx = s_cyc_idx[head];

        g_snake.body_x[0] = (u8)(head % SNAKE_GRID_W);
        g_snake.body_y[0] = (u8)(head / SNAKE_GRID_W);

        for (i = 1; i < 3; i++)
        {
            u16 cell = s_cyc_order[(u16)((idx + N_CELL - i) % N_CELL)];
            g_snake.body_x[i] = (u8)(cell % SNAKE_GRID_W);
            g_snake.body_y[i] = (u8)(cell / SNAKE_GRID_W);
        }

        g_snake.dir = Snake_DirBetween(head, s_cyc_next[head]);
    }
    else
    {
        /* 手动: 中间横着放, 朝右 */
        g_snake.body_x[0] = (u8)(SNAKE_GRID_W / 2);
        g_snake.body_y[0] = (u8)(SNAKE_GRID_H / 2);
        g_snake.body_x[1] = (u8)(SNAKE_GRID_W / 2 - 1);
        g_snake.body_y[1] = g_snake.body_y[0];
        g_snake.body_x[2] = (u8)(SNAKE_GRID_W / 2 - 2);
        g_snake.body_y[2] = g_snake.body_y[0];
        g_snake.dir = SNAKE_RIGHT;
    }

    g_snake.next_dir = g_snake.dir;

    for (i = 0; i < g_snake.len; i++)
    {
        s_occ[Snake_CellOf(g_snake.body_x[i], g_snake.body_y[i])] = 1;
    }

    Snake_SpawnFood();
}

void SNAKE_Init(void)
{
    memset(&g_snake, 0, sizeof(g_snake));
    g_snake.state        = SNAKE_IDLE;
    g_snake.dir          = SNAKE_RIGHT;
    g_snake.next_dir     = SNAKE_RIGHT;
    g_snake.auto_mode    = SNAKE_AUTO_DEFAULT;
    g_snake.score_limit  = SNAKE_MAX_LEN;
    g_snake.last_step_ms = 0;

    memset(s_occ, 0, N_CELL);
    SNAKE_ClearDirty();

    Snake_BuildCycle();

#if SNAKE_KEYS_ENABLE && !defined(SNAKE_HOST_TEST)
    {
        GPIO_InitTypeDef gpio;

        /* KEY0/KEY1/KEY2 = PE4/PE3/PE2, 按下为低; 内部上拉,
         * 板子上没有按键时读到的也是高电平, 不会误触发。
         * KEY_UP = PA0 已经被 TIM5_CH1 输入捕获初始化过, 这里只读 IDR,
         * 不改配置, 用完贪吃蛇回到捕获页仍然正常。 */
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

        gpio.GPIO_Pin   = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
        gpio.GPIO_Mode  = GPIO_Mode_IN;
        gpio.GPIO_OType = GPIO_OType_PP;
        gpio.GPIO_Speed = GPIO_Speed_50MHz;
        gpio.GPIO_PuPd  = GPIO_PuPd_UP;
        GPIO_Init(GPIOE, &gpio);
    }
#endif
}

void SNAKE_Start(void)
{
    Snake_StartCommon(0);
}

void SNAKE_StartAuto(void)
{
    Snake_StartCommon(1);
}

void SNAKE_Reset(void)
{
    g_snake.state     = SNAKE_IDLE;
    g_snake.len       = 0;
    g_snake.score     = 0;
    g_snake.auto_mode = 0;
    g_snake.dir       = SNAKE_RIGHT;
    g_snake.next_dir  = SNAKE_RIGHT;
    g_snake.moves     = 0;

    memset(s_occ, 0, N_CELL);
    SNAKE_ClearDirty();
}

void SNAKE_TogglePause(void)
{
    if (g_snake.state == SNAKE_RUNNING)
    {
        g_snake.state = SNAKE_PAUSED;
    }
    else if (g_snake.state == SNAKE_PAUSED)
    {
        g_snake.state = SNAKE_RUNNING;
        g_snake.last_step_ms = TICK_Now();
    }
}

void SNAKE_SetDir(SNAKE_Dir d)
{
    if (g_snake.state != SNAKE_RUNNING) return;
    if (g_snake.auto_mode) return;
    if (d > SNAKE_RIGHT) return;

    /* 不允许 180 度回头 */
    if ((d == SNAKE_UP    && g_snake.dir == SNAKE_DOWN)  ||
        (d == SNAKE_DOWN  && g_snake.dir == SNAKE_UP)    ||
        (d == SNAKE_LEFT  && g_snake.dir == SNAKE_RIGHT) ||
        (d == SNAKE_RIGHT && g_snake.dir == SNAKE_LEFT))
    {
        return;
    }

    g_snake.next_dir = (u8)d;
}

void SNAKE_SetAuto(u8 on)
{
    if (on)
    {
        /* 自动模式必须重新对齐蛇身, 所以直接重开一局 */
        SNAKE_StartAuto();
    }
    else
    {
        g_snake.auto_mode = 0;
    }
}

/*------------------------------ 物理按键 ----------------------------------*/
#if SNAKE_KEYS_ENABLE && !defined(SNAKE_HOST_TEST)
/* 返回按下的方向; 0xFF=没有新的按下 */
static u8 Snake_KeyPoll(void)
{
    u8 mask = 0;
    u8 pressed;
    u32 now;

    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)) mask |= 0x01;  /* WK_UP */
    if (!GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4)) mask |= 0x02; /* KEY0 */
    if (!GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_3)) mask |= 0x04; /* KEY1 */
    if (!GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_2)) mask |= 0x08; /* KEY2 */

    pressed = (u8)(mask & (u8)(~s_key_prev));
    s_key_prev = mask;

    if (pressed == 0) return 0xFF;

    now = TICK_Now();
    if ((u32)(now - s_key_lock_ms) < 120UL) return 0xFF;
    s_key_lock_ms = now;

    if (pressed & 0x01) return (u8)SNAKE_UP;
    if (pressed & 0x02) return (u8)SNAKE_LEFT;
    if (pressed & 0x04) return (u8)SNAKE_DOWN;
    if (pressed & 0x08) return (u8)SNAKE_RIGHT;
    return 0xFF;
}
#else
#define Snake_KeyPoll()  0xFF
#endif

/*------------------------------ 主 tick -----------------------------------*/
void SNAKE_Tick(u32 now_ms)
{
    u8 d;

    /* 物理按键: WK_UP=上, KEY0=左, KEY1=下, KEY2=右 */
    d = Snake_KeyPoll();
    if (d != 0xFF)
    {
        SNAKE_SetDir((SNAKE_Dir)d);
    }

    if (g_snake.state != SNAKE_RUNNING) return;

    if (g_snake.auto_mode)
    {
        u8 burst = 0;

        /* 自动模式快速推进: 每个 1ms 时间片走 SNAKE_AUTO_STEPS_PER_MS 步,
         * 每次调用最多补 SNAKE_AUTO_BURST 个时间片。
         * 每步只更新状态和脏格子队列, 画屏留给页面, 不阻塞。 */
        while (burst < SNAKE_AUTO_BURST &&
               (u32)(now_ms - g_snake.last_step_ms) >= SNAKE_AUTO_STEP_MS)
        {
            u8 k;

            g_snake.last_step_ms += SNAKE_AUTO_STEP_MS;

            for (k = 0; k < SNAKE_AUTO_STEPS_PER_MS; k++)
            {
                Snake_AutoChooseDir();
                Snake_Step();

                if (g_snake.state != SNAKE_RUNNING) return;
            }

            burst++;
        }

        /* 如果中间被触摸扫描/整屏重画耽误太久, 不要把时间债一直攒着 */
        if ((u32)(now_ms - g_snake.last_step_ms) > 100UL)
        {
            g_snake.last_step_ms = now_ms;
        }
    }
    else
    {
        if ((u32)(now_ms - g_snake.last_step_ms) >= SNAKE_STEP_MS)
        {
            g_snake.last_step_ms = now_ms;
            Snake_Step();
        }
    }
}

u8 SNAKE_GetState(void)
{
    return g_snake.state;
}

u32 SNAKE_GetScore(void)
{
    return g_snake.score;
}

u16 SNAKE_GetScoreLimit(void)
{
    return g_snake.score_limit;
}

void SNAKE_SetScoreLimit(u16 limit)
{
    if (limit < 1) limit = 1;
    if (limit > N_CELL) limit = N_CELL;

    g_snake.score_limit = limit;

    /* 自动模式运行/暂停中把上限改到当前分数以下: 立即结束 */
    if (g_snake.auto_mode &&
        (g_snake.state == SNAKE_RUNNING || g_snake.state == SNAKE_PAUSED) &&
        g_snake.score >= (u32)limit)
    {
        g_snake.state = SNAKE_WIN;
    }
}
