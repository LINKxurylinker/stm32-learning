#include "app_boot.h"
#include "app_config.h"
#include "mpu6050.h"
#include "bsp_tick.h"
#include "tftlcd.h"
#include "touch.h"
#include "ui_core.h"

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

/* 开始界面交互框: 先按这套尺寸画, 画完如果按钮位置变了再同步改这里 */
#define BOOT_START_X        32
#define BOOT_START_Y        176
#define BOOT_START_W        176
#define BOOT_START_H        50

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
#define BOOT_LEAF_Y_MAX     166

/* 1=显示 START/CAL 的边框和长按进度条, 方便先对位置。
 * 你自己的 UI 画好后改成 0, 就不会再画这些参考框。 */
#define BOOT_UI_GUIDES      1

/* 开始界面顶部星空 */
#define BOOT_STAR_COUNT     12
#define BOOT_STAR_X_MAX     180
#define BOOT_STAR_Y_MAX     92
#define BOOT_STAR_SKY       0x0884   /* 夜空蓝黑 */
#define BOOT_STAR_HORIZON   0x4963   /* 天际线 */
#define BOOT_STAR_GOLD      0xDD28
#define BOOT_STAR_CREAM     0xF717
#define BOOT_STAR_WHITE     0x9CF3

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

/*------------------------------ 画面 --------------------------------------*/
static void Boot_DrawParchmentPanel(void)
{
    /* 阴影 */
    UI_Fill(14, 100, 227, 262, PIX_WOOD_SEAM);

    /* 羊皮纸面板 */
    UI_Fill(10, 96, 223, 258, PIX_PARCHMENT);

    /* 上沿高光 */
    UI_Fill(10, 96, 223, 99, PIX_CREAM);

    /* 下沿阴影 */
    UI_Fill(10, 255, 223, 258, PIX_WOOD_MID);

    /* 金色边框 */
    UI_Frame(10, 96, 223, 258, PIX_GOLD);

    /* 像素切角 */
    UI_Fill(10, 96, 13, 99, PIX_WOOD_DARK);
    UI_Fill(220, 96, 223, 99, PIX_WOOD_DARK);
    UI_Fill(10, 255, 13, 258, PIX_WOOD_DARK);
    UI_Fill(220, 255, 223, 258, PIX_WOOD_DARK);
}

static void Boot_DrawProgress(u32 held_ms)
{
    u16 w;

    if (held_ms > BOOT_LONG_PRESS_MS) held_ms = BOOT_LONG_PRESS_MS;

    w = (u16)((held_ms * (u32)BOOT_START_W) / BOOT_LONG_PRESS_MS);

    UI_Fill(BOOT_START_X, BOOT_PROGRESS_Y,
            (u16)(BOOT_START_X + BOOT_START_W - 1),
            (u16)(BOOT_PROGRESS_Y + BOOT_PROGRESS_H - 1),
            UI_COLOR_BG);

    if (w > 0)
    {
        UI_Fill(BOOT_START_X, BOOT_PROGRESS_Y,
                (u16)(BOOT_START_X + w - 1),
                (u16)(BOOT_PROGRESS_Y + BOOT_PROGRESS_H - 1),
                UI_COLOR_ACCENT);
    }

    UI_Frame(BOOT_START_X, BOOT_PROGRESS_Y,
             (u16)(BOOT_START_X + BOOT_START_W - 1),
             (u16)(BOOT_PROGRESS_Y + BOOT_PROGRESS_H - 1),
             UI_COLOR_BORDER);
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

static const unsigned char s_boot_hanzi[4][116] =
{
    { /* 开 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xC0, 0x07, 0xFF, 0xFF, 0xC0, 0x04, 0x1C,
        0x38, 0x00, 0x00, 0x1C, 0x38, 0x00, 0x00, 0x1C, 0x38, 0x00, 0x1F, 0xFF, 0xFF, 0xFC,
        0x7F, 0xFF, 0xFF, 0xFE, 0x78, 0x1C, 0x38, 0xFC, 0x00, 0x1C, 0x38, 0x00, 0x00, 0x38,
        0x38, 0x00, 0x00, 0x38, 0x38, 0x00, 0x00, 0x78, 0x38, 0x00, 0x01, 0xF0, 0x38, 0x00,
        0x1F, 0xE0, 0x38, 0x00, 0x1F, 0x80, 0x38, 0x00, 0x06, 0x00, 0x38, 0x00, 0x00, 0x00,
        0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    },
    { /* 始 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x30, 0x0C, 0x00, 0x00, 0x70, 0x1E, 0x00, 0x00, 0xF0, 0x3C, 0x00, 0x00, 0xE0,
        0x71, 0xC0, 0x01, 0xC0, 0xE1, 0xF0, 0x3F, 0xDB, 0xC0, 0xF8, 0x7F, 0xFF, 0xFB, 0xBC,
        0x47, 0x1D, 0x7F, 0x84, 0x07, 0x1C, 0x04, 0x00, 0x07, 0x39, 0xC4, 0x70, 0x03, 0xB9,
        0xFF, 0xF0, 0x01, 0xF1, 0xC0, 0xF0, 0x00, 0xF1, 0xC0, 0x70, 0x03, 0xF9, 0xC0, 0x70,
        0x7F, 0x9E, 0xC0, 0x70, 0x7F, 0x06, 0xFF, 0xF0, 0x00, 0x00, 0xFF, 0xE0, 0x00, 0x00,
        0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    },
    { /* 探 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0xC3, 0x00, 0x00, 0x01, 0xC7, 0xFF, 0xF0, 0x01, 0xC7, 0x00, 0x70, 0x01, 0xC6,
        0x26, 0x30, 0x1F, 0xF6, 0x77, 0x10, 0x1D, 0xC6, 0xC1, 0x90, 0x01, 0xC2, 0x18, 0x10,
        0x01, 0xF0, 0x1C, 0x00, 0x1F, 0xE3, 0xFF, 0xF0, 0x3E, 0xC7, 0xFF, 0xC0, 0x10, 0xC0,
        0x7E, 0x00, 0x00, 0xC0, 0xFF, 0x80, 0x00, 0xC3, 0xDD, 0xE0, 0x00, 0xC7, 0x9C, 0x7C,
        0x01, 0xFE, 0x1C, 0x3E, 0x3F, 0xC0, 0x1C, 0x1C, 0x1F, 0x80, 0x1C, 0x00, 0x00, 0x00,
        0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    },
    { /* 险 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x60, 0x00, 0x08, 0x00, 0x3F, 0xF0, 0xF0, 0x00, 0x3E, 0x31, 0xFC, 0x00, 0x0E, 0x33,
        0x8F, 0x80, 0x0E, 0x67, 0x03, 0xF0, 0x0E, 0xCE, 0x00, 0xFE, 0x0F, 0xFB, 0xFE, 0x3C,
        0x0E, 0x67, 0xFF, 0x00, 0x0E, 0x30, 0x00, 0x00, 0x0E, 0x30, 0x63, 0x80, 0x0E, 0x77,
        0x73, 0x80, 0x0F, 0xE3, 0x33, 0x00, 0x0F, 0x03, 0x97, 0x00, 0x0E, 0x00, 0x86, 0x00,
        0x0E, 0x00, 0x04, 0x00, 0x0E, 0x1F, 0xFF, 0xF0, 0x0E, 0x1F, 0xFF, 0xF0, 0x0C, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    },
};

static void Boot_DrawHanzi32x29(u16 x, u16 y,
                                const unsigned char *glyph,
                                u16 color)
{
    u8 row;
    u8 col;
    u8 start;
    u8 byte;

    for (row = 0; row < 29; row++)
    {
        col = 0;
        while (col < 32)
        {
            while (col < 32)
            {
                byte = glyph[row * 4 + col / 8];
                if (byte & (0x80 >> (col % 8))) break;
                col++;
            }
            if (col >= 32) break;
            start = col;
            while (col < 32)
            {
                byte = glyph[row * 4 + col / 8];
                if (!(byte & (0x80 >> (col % 8)))) break;
                col++;
            }
            UI_Fill((u16)(x + start), (u16)(y + row),
                    (u16)(x + col - 1), (u16)(y + row), color);
        }
    }
}

static void Boot_DrawStartChinese(void)
{
    u16 x = (u16)(s_start_rect.x + (s_start_rect.w - 128) / 2);
    u16 y = (u16)(s_start_rect.y + (s_start_rect.h - 29) / 2);
    u8 i;

    for (i = 0; i < 4; i++)
    {
        Boot_DrawHanzi32x29((u16)(x + i * 32), y,
                            s_boot_hanzi[i], PIX_CREAM);
    }
}
static void Boot_DrawSplash(u8 calibrated)
{
    (void)calibrated;

    UI_DrawPixelBackground();
    Boot_DrawStarSky();
    Boot_StarsInit();
	Boot_DrawParchmentPanel();
    Boot_LeavesInit();

	UI_DrawButton(&s_start_rect, "", 0);
	UI_DrawButton(&s_calib_rect, "CAL", 0);

#if BOOT_UI_GUIDES
    /* 临时参考框: 只画交互区域, 不画旧版文字。
     * 你自己的 UI 画好后把 BOOT_UI_GUIDES 改成 0。 */
    UI_Frame(s_start_rect.x, s_start_rect.y,
             (u16)(s_start_rect.x + s_start_rect.w - 1),
             (u16)(s_start_rect.y + s_start_rect.h - 1),
             UI_COLOR_BORDER);

    UI_Frame(s_calib_rect.x, s_calib_rect.y,
             (u16)(s_calib_rect.x + s_calib_rect.w - 1),
             (u16)(s_calib_rect.y + s_calib_rect.h - 1),
             UI_COLOR_BORDER);
#endif

    /* 示例文字: 先放在交互框正中间。
     * 你导入自己的字库后，把下面两行换成自己的绘制函数即可。 */
    Boot_DrawStartChinese();

    UI_TextColor((u16)(s_calib_rect.x + (s_calib_rect.w - 18) / 2),
                 (u16)(s_calib_rect.y + (s_calib_rect.h - UI_FONT_S) / 2),
                 18, UI_FONT_S, UI_FONT_S,
                 UI_COLOR_TEXT, UI_COLOR_BG, "CAL");

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
        Boot_LeavesTick();

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
