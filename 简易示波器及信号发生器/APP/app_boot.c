#include "app_boot.h"
#include "app_config.h"
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
 *    ├─ 长按 START (按住 3s)  -> PAGE_SNAKE (彩蛋)
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
#define BOOT_ACTION_SNAKE   2

static const UI_Rect s_start_rect = { 80, 120, 80, 120 };   /* x=80..159, y=120..239 */
static const UI_Rect s_calib_rect = { 196, 6, 36, 28 };     /* 右上角十字 */

static u8 s_calibrated = 0;

/*------------------------------ 画面 --------------------------------------*/
static void Boot_DrawProgress(u32 held_ms)
{
    u16 w;
    u16 x2;

    w = (u16)((held_ms * (u32)s_start_rect.w) / BOOT_LONG_PRESS_MS);
    if (w > s_start_rect.w) w = s_start_rect.w;

    /* 进度条放在 START 下方: x=80..159, y=244..252 */
    UI_Fill(80, 244, 159, 252, UI_COLOR_BG);
    if (w > 0)
    {
        x2 = (u16)(80 + w - 1);
        UI_Fill(80, 244, x2, 252, UI_COLOR_ACCENT);
    }
    UI_Frame(80, 244, 159, 252, UI_COLOR_BORDER);
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
    UI_Fill(8, 270, 231, 284, UI_COLOR_BG);

    UI_TextColor(8, 272, 44, 14, UI_FONT_S,
                 UI_COLOR_MUTED, UI_COLOR_BG,
                 calibrated ? "CAL:1" : "CAL:0");
    UI_TextColor(54, 272, 24, 14, UI_FONT_S,
                 UI_COLOR_MUTED, UI_COLOR_BG, "TP:");

    if (pressed && x != 0xFFFF && y != 0xFFFF)
    {
        UI_NumColor(76, 272, x, 3, UI_FONT_S, UI_COLOR_VALUE, UI_COLOR_BG);
        UI_NumColor(112, 272, y, 3, UI_FONT_S, UI_COLOR_VALUE, UI_COLOR_BG);
    }
    else if (pressed)
    {
        UI_TextColor(76, 272, 60, 14, UI_FONT_S,
                     UI_COLOR_ERR, UI_COLOR_BG, "ERR");
    }
    else
    {
        UI_TextColor(76, 272, 60, 14, UI_FONT_S,
                     UI_COLOR_MUTED, UI_COLOR_BG, "--");
    }
}
#endif

static void Boot_DrawSplash(u8 calibrated)
{
    UI_Fill(0, 0, UI_SCREEN_W - 1, UI_SCREEN_H - 1, UI_COLOR_BG);

    /* 顶部标题区 */
    UI_Fill(0, 0, UI_SCREEN_W - 1, 75, UI_COLOR_PANEL);
    UI_Fill(0, 75, UI_SCREEN_W - 1, 75, UI_COLOR_ACCENT);

    UI_TextColor(12, 8, 216, 32, UI_FONT_L,
                 UI_COLOR_TEXT, UI_COLOR_PANEL, "WELCOME");
    UI_TextColor(12, 44, 216, 14, UI_FONT_S,
                 UI_COLOR_MUTED, UI_COLOR_PANEL, "PWM + CAPTURE + UI");
    UI_TextColor(12, 60, 216, 14, UI_FONT_S,
                 UI_COLOR_VALUE, UI_COLOR_PANEL, "STM32F407 / 240x320");

    /* 中间版本/校准状态 */
    UI_TextColor(12, 84, 216, 14, UI_FONT_S,
                 UI_COLOR_TEXT, UI_COLOR_BG, "BOOT " APP_VERSION_STR);
    UI_TextColor(12, 104, 216, 14, UI_FONT_S,
                 UI_COLOR_MUTED, UI_COLOR_BG,
                 calibrated ? "TOUCH CALIBRATION: OK" : "TOUCH CALIBRATION: NEEDED");

    /* 中间 START 区域: 按下/松开判定, 长按 3 秒进贪吃蛇 */
    UI_DrawButton(&s_start_rect, "START", 0);
    UI_TextColor(88, 196, 64, 14, UI_FONT_S,
                 UI_COLOR_MUTED, UI_COLOR_PANEL, "TAP");

    /* 进度条 + 长按提示 */
    Boot_ClearProgress();
    UI_TextColor(80, 256, 100, 14, UI_FONT_S,
                 UI_COLOR_MUTED, UI_COLOR_BG, "HOLD 3s: SNAKE");

    /* 右上角校准按钮: x=196..231, y=6..33 */
    UI_Frame(s_calib_rect.x, s_calib_rect.y,
             (u16)(s_calib_rect.x + s_calib_rect.w - 1),
             (u16)(s_calib_rect.y + s_calib_rect.h - 1),
             UI_COLOR_BORDER);
    LCD_DrawLine_Color(214, 12, 214, 28, UI_COLOR_ERR);
    LCD_DrawLine_Color(200, 20, 228, 20, UI_COLOR_ERR);
    UI_TextColor(200, 36, 32, 14, UI_FONT_S,
                 UI_COLOR_MUTED, UI_COLOR_PANEL, "CAL");

    UI_TextColor(12, 288, 216, 14, UI_FONT_S,
                 UI_COLOR_MUTED, UI_COLOR_BG,
                 calibrated ? "TAP=START  CAL=TOP-RIGHT" : "TAP=CALIBRATE");

#if BOOT_TOUCH_DEBUG
    Boot_DrawTouchStatus(calibrated, 0, 0, 0);
#endif
}

static void Boot_DrawMessage(const char *line1, const char *line2, u16 color)
{
    UI_Fill(0, 0, UI_SCREEN_W - 1, UI_SCREEN_H - 1, UI_COLOR_BG);
    UI_TextColor(12, 120, 216, 32, UI_FONT_L, color, UI_COLOR_BG, line1);
    UI_TextColor(12, 164, 216, 14, UI_FONT_S, UI_COLOR_MUTED, UI_COLOR_BG, line2);
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
                        UI_TextColor(80, 256, 100, 14, UI_FONT_S,
                                     UI_COLOR_OK, UI_COLOR_BG, "SNAKE!");
                        Boot_WaitRelease();
                        return BOOT_ACTION_SNAKE;
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

        if (action == BOOT_ACTION_SNAKE)
        {
            return PAGE_SNAKE;
        }

        return PAGE_PWM;
    }
}
