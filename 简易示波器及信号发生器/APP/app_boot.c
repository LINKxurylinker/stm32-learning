#include "app_boot.h"
#include "app_config.h"
#include "mpu6050.h"
#include "bsp_tick.h"
#include "tftlcd.h"
#include "touch.h"
#include "ui_core.h"
#include "ui_hanzi.h"

#if APP_DEBUG_UART
#include "usart.h"
#include <stdio.h>
#endif

/*==============================================================================
 * app_boot.c
 *-----------------------------------------------------------------------------
 * 开机状态机:
 *
 *   BOOT
 *    ├─ 短击 START (<3s 松开) -> PAGE_PWM  (微型输入输出仪器主界面)
 *    ├─ 长按 START (按住 3s)  -> PAGE_MAZE (迷宫彩蛋)
 *    └─ 右上角 CAL 十字       -> TP_Adjust() 四点校准 -> 回到 BOOT
 *
 * 注意:
 *   - "短击"和"长按 3 秒"不能同时用"按下立即动作"实现。
 *     所以短击改成"松开时判定", 长按在按住期间计时。
 *   - 长按触发后会等手指松开再生效, 避免触摸状态带进下一个页面。
 *   - 未校准时屏幕坐标无效, 只能把"任意按下"当成去校准。
 *   - 这里在初始化阶段运行, 可以阻塞; 主循环里不能这样写。
 *============================================================================*/

#define BOOT_ACTION_START   0
#define BOOT_ACTION_CALIB   1
#define BOOT_ACTION_MAZE    2

/* 开始界面交互框: 放在牛皮纸面板正中间 (面板 x=10..223, y=96..258) */
#define BOOT_START_X        50
#define BOOT_START_Y        154
#define BOOT_START_W        134
#define BOOT_START_H        46

#define BOOT_CAL_X          192
#define BOOT_CAL_Y          8
#define BOOT_CAL_W          40
#define BOOT_CAL_H          34

#define BOOT_PROGRESS_Y     (BOOT_START_Y + BOOT_START_H + 10)
#define BOOT_PROGRESS_H     8

/* 羊皮纸上的落叶活动区域 */
#define BOOT_LEAF_COUNT     10
#define BOOT_LEAF_X_MIN     16
#define BOOT_LEAF_X_MAX     214
#define BOOT_LEAF_Y_MIN     104
#define BOOT_LEAF_Y_MAX     148

/* 羊皮纸上层内嵌水槽 */
#define BOOT_WATER_X        20
#define BOOT_WATER_Y        102
#define BOOT_WATER_W        200
#define BOOT_WATER_H        46
#define BOOT_WATER_COUNT    18

#define PIX_WATER_DARK      0x09EB
#define PIX_WATER_MID       0x1371
#define PIX_WATER_LIGHT     0x2D37
#define PIX_WATER_FOAM      0xEFFF

/* 1=显示 START/CAL 的边框和长按进度条, 方便先对位置。
 * 你自己的 UI 画好后改成 0, 就不会再画这些参考框。 */
#define BOOT_UI_GUIDES      0

/* 开始界面顶部星空 */
#define BOOT_STAR_COUNT     12
#define BOOT_STAR_X_MAX     180
#define BOOT_STAR_Y_MAX     92
#define BOOT_STAR_SKY       0x0884   /* 夜空蓝黑 */
#define BOOT_STAR_HORIZON   0x4963   /* 天际线 */
#define BOOT_STAR_GOLD      0xDD28
#define BOOT_STAR_CREAM     0xF717
#define BOOT_STAR_WHITE     0x9CF3

/* 星空与木板交界上方的毛毛虫 */
#define BOOT_CAT_Y          82       /* 身体基准 y, 在 y=92 天际线上方 */
#define BOOT_CAT_X_MIN      8
#define BOOT_CAT_X_MAX      (UI_SCREEN_W - 36)
#define BOOT_CAT_STEP       2
#define BOOT_CAT_TICK_MS    80
#define BOOT_CAT_W          30

static const UI_Rect s_start_rect =
{
    BOOT_START_X, BOOT_START_Y, BOOT_START_W, BOOT_START_H
};

static const UI_Rect s_calib_rect =
{
    BOOT_CAL_X, BOOT_CAL_Y, BOOT_CAL_W, BOOT_CAL_H
};

static u8 s_calibrated = 0;

/*------------------------------ 顶部星空 ----------------------------------*/
typedef struct
{
    s16 x;
    s16 y;
    s8  vx;
    s8  vy;
    u8  len;
    u16 color;
} BootStar;

static BootStar s_stars[BOOT_STAR_COUNT];
static u32 s_star_seed = 0x12345678UL;
static u32 s_star_last_ms = 0;

static u16 Boot_StarRand(void)
{
    s_star_seed = s_star_seed * 1103515245UL + 12345UL;
    return (u16)(s_star_seed >> 16);
}

static void Boot_StarPaint(const BootStar *s, u16 color)
{
    u8 i;
    s16 x;
    s16 y;

    for (i = 0; i < s->len; i++)
    {
        x = (s16)(s->x - (s16)i * s->vx);
        y = (s16)(s->y - (s16)i * s->vy);

        if (x >= 2 && x < BOOT_STAR_X_MAX &&
            y >= 2 && y < BOOT_STAR_Y_MAX)
        {
            UI_Fill((u16)x, (u16)y, (u16)x, (u16)y, color);
        }
    }

    if (s->x >= 2 && s->x + 1 < BOOT_STAR_X_MAX &&
        s->y >= 2 && s->y + 1 < BOOT_STAR_Y_MAX)
    {
        UI_Fill((u16)s->x, (u16)s->y,
                (u16)(s->x + 1), (u16)(s->y + 1), color);
    }
}

static void Boot_StarReset(BootStar *s, u8 initial)
{
    s->x = (s16)(4 + (Boot_StarRand() % (BOOT_STAR_X_MAX - 12)));
    s->y = initial ? (s16)(4 + (Boot_StarRand() % (BOOT_STAR_Y_MAX - 12)))
                   : (s16)(2 + (Boot_StarRand() % 12));

    s->vx = (s8)((Boot_StarRand() & 1) ? 1 : 2);
    s->vy = (s8)(2 + (Boot_StarRand() % 3));
    s->len = (u8)(2 + (Boot_StarRand() % 3));

    switch (Boot_StarRand() % 3)
    {
        case 0:  s->color = BOOT_STAR_CREAM; break;
        case 1:  s->color = BOOT_STAR_GOLD;  break;
        default: s->color = BOOT_STAR_WHITE; break;
    }
}

static void Boot_StarsInit(void)
{
    u8 i;

    s_star_last_ms = TICK_Now();

    for (i = 0; i < BOOT_STAR_COUNT; i++)
    {
        Boot_StarReset(&s_stars[i], 1);
        Boot_StarPaint(&s_stars[i], s_stars[i].color);
    }
}

static void Boot_StarsTick(void)
{
    u8 i;
    u32 now = TICK_Now();

    if ((u32)(now - s_star_last_ms) < 70)
    {
        return;
    }

    s_star_last_ms = now;

    for (i = 0; i < BOOT_STAR_COUNT; i++)
    {
        Boot_StarPaint(&s_stars[i], BOOT_STAR_SKY);

        s_stars[i].x = (s16)(s_stars[i].x + s_stars[i].vx);
        s_stars[i].y = (s16)(s_stars[i].y + s_stars[i].vy);

        if (s_stars[i].x >= BOOT_STAR_X_MAX ||
            s_stars[i].y >= BOOT_STAR_Y_MAX)
        {
            Boot_StarReset(&s_stars[i], 0);
        }

        Boot_StarPaint(&s_stars[i], s_stars[i].color);
    }
}

static void Boot_DrawStarSky(void)
{
    UI_Fill(0, 0, UI_SCREEN_W - 1, BOOT_STAR_Y_MAX - 1, BOOT_STAR_SKY);
    UI_Fill(0, BOOT_STAR_Y_MAX, UI_SCREEN_W - 1,
            BOOT_STAR_Y_MAX + 1, BOOT_STAR_HORIZON);
}

/*------------------------------ 毛毛虫 ------------------------------------*/
/* 在星空和木板交界上方来回爬: 6 节身体 + 头 + 触角 + 小腿,
 * 每 80ms 移动 2px, 上下用 8 相位的小波动模拟蠕动。 */
static const s8 s_cat_wave[8] = { 0, 1, 2, 1, 0, -1, -2, -1 };
static s16 s_cat_x = BOOT_CAT_X_MIN;
static s8  s_cat_dir = 1;
static u8  s_cat_phase = 0;
static u32 s_cat_last_ms = 0;

static void Boot_CatDraw(s16 x, s8 dir, u8 phase)
{
    u8 i;
    s16 sx;
    s16 sy;
    s16 hx;
    s16 hy;
    u16 body;

    for (i = 0; i < 6; i++)
    {
        if (dir > 0) sx = (s16)(x + (s16)i * 4);
        else         sx = (s16)(x + 20 - (s16)i * 4);

        sy = (s16)(BOOT_CAT_Y + s_cat_wave[(phase + i * 2) & 7]);
        body = (i & 1) ? PIX_GREEN : PIX_DEEP_GREEN;

        UI_Fill((u16)sx, (u16)sy, (u16)(sx + 3), (u16)(sy + 4), body);

        /* 每两节画一条小腿 */
        if (i & 1)
        {
            UI_Fill((u16)(sx + 1), (u16)(sy + 5),
                    (u16)(sx + 1), (u16)(sy + 6), PIX_WOOD_DARK);
        }
    }

    hx = (dir > 0) ? (s16)(x + 22) : x;
    hy = (s16)(BOOT_CAT_Y + s_cat_wave[(phase + 3) & 7]);

    UI_Fill((u16)hx, (u16)hy, (u16)(hx + 4), (u16)(hy + 5), PIX_GREEN);

    /* 眼睛 */
    UI_Fill((u16)(hx + 1), (u16)(hy + 2),
            (u16)(hx + 1), (u16)(hy + 3), PIX_WOOD_DARK);
    UI_Fill((u16)(hx + 3), (u16)(hy + 2),
            (u16)(hx + 3), (u16)(hy + 3), PIX_WOOD_DARK);

    /* 触角 */
    UI_Fill((u16)(hx + 1), (u16)(hy - 2),
            (u16)(hx + 1), (u16)(hy - 1), PIX_GOLD);
    UI_Fill((u16)(hx + 3), (u16)(hy - 2),
            (u16)(hx + 3), (u16)(hy - 1), PIX_GOLD);
}

static void Boot_CatErase(void)
{
    u8 i;

    /* 只清毛毛虫的包围盒, 然后把可能被盖住的星星补回来 */
    UI_Fill((u16)(s_cat_x - 2), (u16)(BOOT_CAT_Y - 4),
            (u16)(s_cat_x + BOOT_CAT_W), (u16)(BOOT_CAT_Y + 9),
            BOOT_STAR_SKY);

    for (i = 0; i < BOOT_STAR_COUNT; i++)
    {
        Boot_StarPaint(&s_stars[i], s_stars[i].color);
    }
}

static void Boot_CatInit(void)
{
    s_cat_x = 20;
    s_cat_dir = 1;
    s_cat_phase = 0;
    s_cat_last_ms = TICK_Now();

    Boot_CatDraw(s_cat_x, s_cat_dir, s_cat_phase);
}

static void Boot_CatTick(void)
{
    u32 now = TICK_Now();

    if ((u32)(now - s_cat_last_ms) < BOOT_CAT_TICK_MS)
    {
        return;
    }
    s_cat_last_ms = now;

    Boot_CatErase();

    s_cat_x = (s16)(s_cat_x + (s16)(s_cat_dir * BOOT_CAT_STEP));
    if (s_cat_x <= BOOT_CAT_X_MIN)
    {
        s_cat_x = BOOT_CAT_X_MIN;
        s_cat_dir = 1;
    }
    else if (s_cat_x >= BOOT_CAT_X_MAX)
    {
        s_cat_x = BOOT_CAT_X_MAX;
        s_cat_dir = -1;
    }

    s_cat_phase = (u8)((s_cat_phase + 1) & 7);

    Boot_CatDraw(s_cat_x, s_cat_dir, s_cat_phase);
}

/*------------------------------ 羊皮纸落叶 --------------------------------*/
typedef struct
{
    s16 x;
    s16 y;
    u8  phase;
    u16 color;
} BootLeaf;

static BootLeaf s_leaves[BOOT_LEAF_COUNT];
static s16 s_wind_x = 0;
static u16 s_gust = 0;
static u32 s_sensor_last_ms = 0;
static u32 s_leaf_last_ms = 0;

static s16 Boot_Abs16(s16 v)
{
    return (v < 0) ? (s16)(-v) : v;
}

static void Boot_LeafPaint(const BootLeaf *leaf, u16 color)
{
    if (leaf->x < BOOT_LEAF_X_MIN ||
        leaf->x > (BOOT_LEAF_X_MAX - 2) ||
        leaf->y < BOOT_LEAF_Y_MIN ||
        leaf->y > (BOOT_LEAF_Y_MAX - 1))
    {
        return;
    }

    UI_Fill((u16)leaf->x, (u16)leaf->y,
            (u16)(leaf->x + 2), (u16)(leaf->y + 1),
            color);
}

static void Boot_LeafReset(BootLeaf *leaf, u8 initial)
{
    u16 r = Boot_StarRand();

    leaf->x = (s16)(BOOT_LEAF_X_MIN +
                    (r % (BOOT_LEAF_X_MAX - BOOT_LEAF_X_MIN - 3)));
    leaf->y = initial
              ? (s16)(BOOT_LEAF_Y_MIN +
                      (Boot_StarRand() % (BOOT_LEAF_Y_MAX - BOOT_LEAF_Y_MIN - 2)))
              : (s16)(BOOT_LEAF_Y_MIN + (Boot_StarRand() % 8));

    leaf->phase = (u8)(Boot_StarRand() & 1);

    switch (Boot_StarRand() % 3)
    {
        case 0:  leaf->color = PIX_GREEN;      break;
        case 1:  leaf->color = PIX_GOLD;       break;
        default: leaf->color = PIX_WOOD_LIGHT; break;
    }
}

static void Boot_LeavesInit(void)
{
    u8 i;

    s_wind_x = 0;
    s_gust = 0;
    s_sensor_last_ms = 0;
    s_leaf_last_ms = TICK_Now();

    for (i = 0; i < BOOT_LEAF_COUNT; i++)
    {
        Boot_LeafReset(&s_leaves[i], 1);
        Boot_LeafPaint(&s_leaves[i], s_leaves[i].color);
    }
}

static void Boot_WindUpdate(u32 now_ms)
{
    short ax, ay, az;
    short gx, gy, gz;
    s16 target;
    u16 shake;

    if (!MPU6050_IsReady())
    {
        return;
    }

    if ((u32)(now_ms - s_sensor_last_ms) < 60)
    {
        return;
    }

    s_sensor_last_ms = now_ms;

    if (MPU6050_Get_Accelerometer(&ax, &ay, &az) != 0)
    {
        return;
    }
    if (MPU6050_Get_Gyroscope(&gx, &gy, &gz) != 0)
    {
        return;
    }

    (void)ay;
    (void)az;
    (void)gx;

    /* 倾斜 -> 水平风 */
    target = (s16)(ax / 2048);
    if (target > 8)  target = 8;
    if (target < -8) target = -8;

    s_wind_x = (s16)((s_wind_x * 3 + target) / 4);

    /* 晃动 -> 阵风 */
    shake = (u16)((Boot_Abs16(gz) / 1024) +
                  (Boot_Abs16(gy) / 1024));
    if (shake > 10) shake = 10;
    s_gust = shake;
}

static void Boot_LeavesTick(void)
{
    u8 i;
    s16 wx;
    s16 wy;
    u32 now = TICK_Now();

    Boot_WindUpdate(now);

    if ((u32)(now - s_leaf_last_ms) < 50)
    {
        return;
    }

    s_leaf_last_ms = now;

    for (i = 0; i < BOOT_LEAF_COUNT; i++)
    {
        Boot_LeafPaint(&s_leaves[i], PIX_PARCHMENT);

        wx = (s16)(s_wind_x + ((s_leaves[i].phase & 1) ? -1 : 1));
        wy = (s16)(1 + (s_gust / 4) + (s_leaves[i].phase & 1));

        s_leaves[i].x = (s16)(s_leaves[i].x + wx);
        s_leaves[i].y = (s16)(s_leaves[i].y + wy);

        if (s_leaves[i].x < BOOT_LEAF_X_MIN ||
            s_leaves[i].x > (BOOT_LEAF_X_MAX - 2) ||
            s_leaves[i].y > (BOOT_LEAF_Y_MAX - 1))
        {
            Boot_LeafReset(&s_leaves[i], 0);
        }

        Boot_LeafPaint(&s_leaves[i], s_leaves[i].color);
    }
}

/*------------------------------ 内嵌水槽 --------------------------------*/
typedef struct
{
    s16 x;
    s16 y;
    s8  vx;
    u8  phase;
    u16 color;
} BootWater;

static BootWater s_water[BOOT_WATER_COUNT];
static u32 s_water_last_ms = 0;

static void Boot_WaterPaint(const BootWater *p, u16 color)
{
    if (p->x >= (BOOT_WATER_X + 2) &&
        p->x <= (BOOT_WATER_X + BOOT_WATER_W - 4) &&
        p->y >= (BOOT_WATER_Y + 14) &&
        p->y <= (BOOT_WATER_Y + BOOT_WATER_H - 9))
    {
        UI_Fill((u16)p->x, (u16)p->y,
                (u16)(p->x + 1), (u16)p->y, color);
    }
}

static void Boot_WaterReset(BootWater *p, u8 initial)
{
    p->x = (s16)(BOOT_WATER_X + 3 +
                 (Boot_StarRand() % (BOOT_WATER_W - 10)));
    p->y = initial
           ? (s16)(BOOT_WATER_Y + 16 +
                   (Boot_StarRand() % (BOOT_WATER_H - 24)))
           : (s16)(BOOT_WATER_Y + 16 +
                   (Boot_StarRand() % 8));

    p->vx = (s8)(1 + (Boot_StarRand() & 1));
    p->phase = (u8)Boot_StarRand();
    p->color = (Boot_StarRand() & 3) ? PIX_WATER_LIGHT : PIX_WATER_FOAM;
}

static void Boot_DrawWaterFrame(void)
{
    u16 x;
    u16 wy;

    /* 外框阴影 */
    UI_FillRound(BOOT_WATER_X - 2, BOOT_WATER_Y - 2,
                 BOOT_WATER_X + BOOT_WATER_W + 1,
                 BOOT_WATER_Y + BOOT_WATER_H + 1,
                 10, PIX_WOOD_SEAM);

    /* 木质外框 */
    UI_FillRound(BOOT_WATER_X - 1, BOOT_WATER_Y - 1,
                 BOOT_WATER_X + BOOT_WATER_W,
                 BOOT_WATER_Y + BOOT_WATER_H,
                 10, PIX_WOOD_MID);

    /* 水体 */
    UI_FillRound(BOOT_WATER_X, BOOT_WATER_Y,
                 BOOT_WATER_X + BOOT_WATER_W - 1,
                 BOOT_WATER_Y + BOOT_WATER_H - 1,
                 8, PIX_WATER_DARK);

    /* 上层水面 */
    UI_Fill(BOOT_WATER_X + 6, BOOT_WATER_Y + 2,
            BOOT_WATER_X + BOOT_WATER_W - 7,
            BOOT_WATER_Y + 8,
            PIX_WATER_MID);

    /* 水面波光 */
    for (x = BOOT_WATER_X + 4; x < BOOT_WATER_X + BOOT_WATER_W - 8; x += 12)
    {
        wy = (u16)(BOOT_WATER_Y + 6 + (((x / 12) & 1) ? 2 : 0));
        UI_Fill(x, wy, (u16)(x + 6), wy, PIX_WATER_LIGHT);
    }

    /* 水底 */
    UI_Fill(BOOT_WATER_X + 8, BOOT_WATER_Y + BOOT_WATER_H - 7,
            BOOT_WATER_X + BOOT_WATER_W - 7,
            BOOT_WATER_Y + BOOT_WATER_H - 3,
            PIX_WATER_MID);

    /* 内嵌金边 */
    UI_FrameRound(BOOT_WATER_X, BOOT_WATER_Y,
                  BOOT_WATER_X + BOOT_WATER_W - 1,
                  BOOT_WATER_Y + BOOT_WATER_H - 1,
                  8, PIX_GOLD);

    UI_FrameRound(BOOT_WATER_X + 2, BOOT_WATER_Y + 2,
             BOOT_WATER_X + BOOT_WATER_W - 7,
                  BOOT_WATER_Y + BOOT_WATER_H - 3,
                  6, PIX_WOOD_DARK);
}

static void Boot_WaterInit(void)
{
    u8 i;

    Boot_DrawWaterFrame();

    s_water_last_ms = TICK_Now();

    for (i = 0; i < BOOT_WATER_COUNT; i++)
    {
        Boot_WaterReset(&s_water[i], 1);
        Boot_WaterPaint(&s_water[i], s_water[i].color);
    }
}

static void Boot_WaterTick(void)
{
    u8 i;
    u32 now = TICK_Now();

    if ((u32)(now - s_water_last_ms) < 60)
    {
        return;
    }

    s_water_last_ms = now;

    for (i = 0; i < BOOT_WATER_COUNT; i++)
    {
        Boot_WaterPaint(&s_water[i], PIX_WATER_DARK);

        s_water[i].x = (s16)(s_water[i].x + s_water[i].vx);
        s_water[i].phase++;

        if (s_water[i].phase & 2)
        {
            s_water[i].y++;
        }
        else
        {
            s_water[i].y--;
        }

        if (s_water[i].y < (BOOT_WATER_Y + 14))
        {
            s_water[i].y = (s16)(BOOT_WATER_Y + 14);
        }
        if (s_water[i].y > (BOOT_WATER_Y + BOOT_WATER_H - 9))
        {
            s_water[i].y = (s16)(BOOT_WATER_Y + BOOT_WATER_H - 9);
        }

        if (s_water[i].x > (BOOT_WATER_X + BOOT_WATER_W - 4))
        {
            Boot_WaterReset(&s_water[i], 0);
        }

        Boot_WaterPaint(&s_water[i], s_water[i].color);
    }
}

/*------------------------------ 画面 --------------------------------------*/
static void Boot_DrawParchmentPanel(void)
{
    /* 阴影 */
    UI_FillRound(14, 100, 227, 262, 14, PIX_WOOD_SEAM);

    /* 羊皮纸面板 */
    UI_FillRound(10, 96, 223, 258, 12, PIX_PARCHMENT);

    /* 上沿高光 */
    UI_Fill(22, 96, 211, 99, PIX_CREAM);

    /* 下沿阴影 */
    UI_Fill(22, 255, 211, 258, PIX_WOOD_MID);

    /* 金色圆角边框 */
    UI_FrameRound(10, 96, 223, 258, 12, PIX_GOLD);
}

static void Boot_DrawProgress(u32 held_ms)
{
    u16 w;

    if (held_ms > BOOT_LONG_PRESS_MS) held_ms = BOOT_LONG_PRESS_MS;

    w = (u16)((held_ms * (u32)BOOT_START_W) / BOOT_LONG_PRESS_MS);

    UI_FillRound(BOOT_START_X, BOOT_PROGRESS_Y,
                 (u16)(BOOT_START_X + BOOT_START_W - 1),
                 (u16)(BOOT_PROGRESS_Y + BOOT_PROGRESS_H - 1),
                 4, PIX_WOOD_DARK);

    if (w > 0)
    {
        UI_FillRound(BOOT_START_X, BOOT_PROGRESS_Y,
                     (u16)(BOOT_START_X + w - 1),
                     (u16)(BOOT_PROGRESS_Y + BOOT_PROGRESS_H - 1),
                     4, PIX_GREEN);
    }

    UI_FrameRound(BOOT_START_X, BOOT_PROGRESS_Y,
                  (u16)(BOOT_START_X + BOOT_START_W - 1),
                  (u16)(BOOT_PROGRESS_Y + BOOT_PROGRESS_H - 1),
                  4, PIX_GOLD);
}

static void Boot_ClearProgress(void)
{
    Boot_DrawProgress(0);
}

#if BOOT_TOUCH_DEBUG
/* 开始界面底部的触摸调试行: CAL:1/0 + 当前坐标。
 * 按下时如果 TP: 后面有数字, 说明触摸驱动读到了坐标;
 * 一直 TP:-- 说明 TP_Scan 没检测到按下。 */
static void Boot_DrawTouchStatus(u8 calibrated, u8 pressed, u16 x, u16 y)
{
    (void)calibrated;
    (void)pressed;
    (void)x;
    (void)y;
    /* TODO(UI): 需要调试触摸时，在这里画自己的坐标显示。 */
}
#endif

static void Boot_DrawStartBox(void)
{
    /* 牛皮纸中间的木质开始框: 深木底 + 金边 + 米白 START */
    UI_FillRound(s_start_rect.x, s_start_rect.y,
                 (u16)(s_start_rect.x + s_start_rect.w - 1),
                 (u16)(s_start_rect.y + s_start_rect.h - 1),
                 10, PIX_WOOD_MID);

    UI_FrameRound(s_start_rect.x, s_start_rect.y,
                  (u16)(s_start_rect.x + s_start_rect.w - 1),
                  (u16)(s_start_rect.y + s_start_rect.h - 1),
                  10, PIX_GOLD);

    UI_FrameRound((u16)(s_start_rect.x + 2), (u16)(s_start_rect.y + 2),
                  (u16)(s_start_rect.x + s_start_rect.w - 3),
                  (u16)(s_start_rect.y + s_start_rect.h - 3),
                  8, PIX_WOOD_DARK);

    UI_HZ16_TextScaled((u16)(s_start_rect.x + (s_start_rect.w - 128) / 2),
                       (u16)(s_start_rect.y + (s_start_rect.h - 32) / 2),
                       "开始探险", PIX_CREAM, PIX_WOOD_MID, 2);
}

static void Boot_DrawSplash(u8 calibrated)
{
    (void)calibrated;

    UI_DrawPixelBackground();
    Boot_DrawStarSky();
    Boot_StarsInit();
    Boot_CatInit();
    Boot_DrawParchmentPanel();
    Boot_WaterInit();

    Boot_DrawStartBox();
    UI_DrawButtonEx(&s_calib_rect, "", 0, BOOT_STAR_SKY);
    UI_HZ16_Text((u16)(s_calib_rect.x + (s_calib_rect.w - 32) / 2),
                 (u16)(s_calib_rect.y + (s_calib_rect.h - 16) / 2),
                 "校准", UI_COLOR_TEXT, UI_COLOR_PANEL);

#if BOOT_UI_GUIDES
    /* 临时参考框: 只画交互区域, 不画旧版文字。 */
    UI_Frame(s_start_rect.x, s_start_rect.y,
             (u16)(s_start_rect.x + s_start_rect.w - 1),
             (u16)(s_start_rect.y + s_start_rect.h - 1),
             UI_COLOR_BORDER);

    UI_Frame(s_calib_rect.x, s_calib_rect.y,
             (u16)(s_calib_rect.x + s_calib_rect.w - 1),
             (u16)(s_calib_rect.y + s_calib_rect.h - 1),
             UI_COLOR_BORDER);
#endif

    Boot_ClearProgress();
}

static void Boot_DrawMessage(const char *line1, const char *line2, u16 color)
{
    (void)line1;
    (void)line2;
    (void)color;

    /* 校准时会经过这里: CALIBRATING / CALIB OK / CALIB FAILED。
     * 先清屏，文字由你自己接字库后画。 */
    UI_Fill(0, 0, UI_SCREEN_W - 1, UI_SCREEN_H - 1, UI_COLOR_BG);
}

/* 长按触发后等松手, 防止把"按住"状态带进下一个页面 */
static void Boot_WaitRelease(void)
{
    while (s_calibrated ? TP_Scan(0) : TP_Scan(1))
    {
        TICK_DelayMs(5);
    }
    tp_dev.sta &= ~TP_CATH_PRES;
}

/*------------------------------ 触摸判定 --------------------------------*/
static u8 Boot_WaitAction(void)
{
    u32 start_ms = TICK_Now();
    u32 down_ms = 0;
    u16 down_x = 0;
    u16 down_y = 0;
    u16 last_x = 0;
    u16 last_y = 0;
    u8  down = 0;
    u8  start_hold = 0;
    u8  long_ready = 0;

    while ((BOOT_SPLASH_TIMEOUT_MS == 0UL) ||
           ((u32)(TICK_Now() - start_ms) < BOOT_SPLASH_TIMEOUT_MS))
    {
        u8 pressed;
        u16 x = 0;
        u16 y = 0;

        pressed = s_calibrated ? TP_Scan(0) : TP_Scan(1);

        if (pressed)
        {
            x = tp_dev.x[0];
            y = tp_dev.y[0];
            if (x == 0xFFFF || y == 0xFFFF) pressed = 0;
        }

#if BOOT_TOUCH_DEBUG
        Boot_DrawTouchStatus(s_calibrated, pressed, x, y);
#endif

        Boot_StarsTick();
        Boot_WaterTick();
        Boot_CatTick();

        if (pressed)
        {
            last_x = x;
            last_y = y;

            if (!down)
            {
                down = 1;
                down_ms = TICK_Now();
                down_x = x;
                down_y = y;
                start_hold = 0;
                long_ready = 0;

                tp_dev.sta &= ~TP_CATH_PRES;

                if (s_calibrated && UI_HitTest(&s_start_rect, down_x, down_y))
                {
                    start_hold = 1;
                }
            }

            /* 长按 START: 只有按住点还在 START 区域内才计时 */
            if (s_calibrated && start_hold)
            {
                if (!UI_HitTest(&s_start_rect, x, y))
                {
                    start_hold = 0;
                    Boot_ClearProgress();
                }
                else
                {
                    u32 held = (u32)(TICK_Now() - down_ms);

                    if (!long_ready && held >= BOOT_LONG_PRESS_MS)
                    {
                        long_ready = 1;
                        Boot_DrawProgress(BOOT_LONG_PRESS_MS);
                        /* TODO(UI): 长按达标时，在这里画你自己的反馈。 */
                        Boot_WaitRelease();
                        return BOOT_ACTION_MAZE;
                    }
                    if (!long_ready) Boot_DrawProgress(held);
                }
            }
        }
        else if (down)
        {
            down = 0;
            Boot_ClearProgress();

            if (!s_calibrated)
            {
                return BOOT_ACTION_CALIB;
            }

            if (UI_HitTest(&s_start_rect, down_x, down_y) &&
                UI_HitTest(&s_start_rect, last_x, last_y))
            {
                return BOOT_ACTION_START;
            }

            if (UI_HitTest(&s_calib_rect, down_x, down_y) &&
                UI_HitTest(&s_calib_rect, last_x, last_y))
            {
                return BOOT_ACTION_CALIB;
            }
        }
    }

    /* 超时: 未校准必须先去校准, 已校准直接进主界面 */
    return s_calibrated ? BOOT_ACTION_START : BOOT_ACTION_CALIB;
}

/*------------------------------ 总流程 ------------------------------------*/
UI_PageId BOOT_Run(void)
{
    u8 action;
    u8 force_calib = BOOT_FORCE_CALIB;

    s_calibrated = TP_InitNoCalib();

#if APP_DEBUG_UART
    printf("Boot: touch %s\r\n", s_calibrated ? "calibrated" : "not calibrated");
#endif

    for (;;)
    {
        if (force_calib)
        {
            /* 强制校准: 开机先清屏校准, 不显示 WELCOME。
             * 校准完成后再画开始界面。 */
            force_calib = 0;
            action = BOOT_ACTION_CALIB;
        }
        else
        {
            Boot_DrawSplash(s_calibrated);
            action = Boot_WaitAction();
        }

        if (action == BOOT_ACTION_CALIB)
        {
            Boot_DrawMessage("CALIBRATING", "TOUCH THE 4 CROSSES", UI_COLOR_EDIT);
            TICK_DelayMs(300);

            TP_Adjust();
            TP_Save_Adjdata();
            s_calibrated = TP_Get_Adjdata();

#if APP_DEBUG_UART
            printf("Boot: calibration %s\r\n", s_calibrated ? "OK" : "FAILED");
#endif

            if (s_calibrated)
            {
                Boot_DrawMessage("CALIB OK", "BACK TO START", UI_COLOR_OK);
            }
            else
            {
                Boot_DrawMessage("CALIB FAILED", "TRY AGAIN", UI_COLOR_ERR);
            }
            TICK_DelayMs(800);
            continue;
        }

        if (action == BOOT_ACTION_MAZE)
        {
            return PAGE_MAZE;
        }

        return PAGE_PWM;
    }
}
