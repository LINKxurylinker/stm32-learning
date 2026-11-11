#include "ui_pages.h"

#include "app_config.h"

#include "bsp_pwm.h"

#include "bsp_capture.h"

#include "bsp_spwm.h"

#include "app_stopwatch.h"

#include "app_sweep.h"

#include "app_snake.h"

#include "app_maze.h"

#include "tftlcd.h"



/*==============================================================================

 * ui_pages.c

 *-----------------------------------------------------------------------------

 * 最小可交互 UI:

 *   - 目前只保留 PWM 主界面; 主菜单/占位页已删除, 后续再加

 *   - 每页顶部有状态栏, 非主界面页底部有统一的 BACK 按钮 (在 ui_core.c 里画)

 *   - PWM/CAPTURE/TIMER/SWEEP/SPWM/MAZE/SNAKE 页先显示关键数据

 *   - 页面里的按钮/键盘/滑条只留了最小接口, 你可以继续美化

 *

 * 规则:

 *   - 只用 tftlcd.c 的 LCD_* 函数, 不用任何图形库;

 *   - 页面里不要 delay_ms/while;

 *   - 改完值 UI_MarkDirty();

 *   - 文字全部用英文/数字 (字库只有 ASCII + 4 个汉字).

 *============================================================================*/



/*============================== 小工具 ====================================*/



static void Page_Begin(const char *title)

{

    /* 换页/关键盘时才整屏清一次; 周期性刷新只重画控件, 避免闪屏 */

    if (g_ui_full_redraw)

    {

        UI_Fill(0, 0, UI_SCREEN_W - 1, UI_SCREEN_H - 1, UI_COLOR_BG);

        g_ui_full_redraw = 0;

    }

    UI_DrawStatusBar(title);

}



static void Page_Label(u16 x, u16 y, const char *s)

{

    UI_TextColor(x, y, (u16)(UI_SCREEN_W - x - 4), UI_FONT_S + 2,

                 UI_FONT_S, UI_COLOR_MUTED, UI_COLOR_BG, s);

}



static void Page_Text(u16 x, u16 y, u16 color, const char *s)

{

    UI_TextColor(x, y, (u16)(UI_SCREEN_W - x - 4), UI_FONT_S + 2,

                 UI_FONT_S, color, UI_COLOR_BG, s);

}



static void Page_Value(u16 x, u16 y, u32 value, u8 len)

{

    UI_NumColor(x, y, value, len, UI_FONT_L, UI_COLOR_VALUE, UI_COLOR_BG);

}



/* mm:ss.mmm, 固定宽度, 用补0显示 */

static void Page_Time(u16 x, u16 y, u32 ms, u16 color)

{

    u32 min  = ms / 60000UL;

    u32 sec  = (ms / 1000UL) % 60UL;

    u32 msec = ms % 1000UL;



    UI_NumPadColor(x,      y, min,  2, UI_FONT_L, color, UI_COLOR_BG);

    UI_TextColor  (x + 24, y, 12, UI_FONT_L, UI_FONT_L, color, UI_COLOR_BG, ":");

    UI_NumPadColor(x + 36, y, sec,  2, UI_FONT_L, color, UI_COLOR_BG);

    UI_TextColor  (x + 60, y, 12, UI_FONT_L, UI_FONT_L, color, UI_COLOR_BG, ".");

    UI_NumPadColor(x + 72, y, msec, 3, UI_FONT_L, color, UI_COLOR_BG);

}



static const char *SweepStateStr(void)

{

    if (SWEEP_IsActive()) return "运行中";

    if (SWEEP_IsDone())   return "完成";

    return "空闲";

}



/*============================== PWM GEN ===================================*/

/* 主功能界面 (竖屏):

 *   上半屏: CH1/CH2 PWM 波形, CH1 橙色, CH2 绿色

 *   下半屏: CH1/CH2 开关 + 频率/占空比数字面板

 *

 *   CH1/CH2 按钮:

 *     亮色 = 正在输出; 背景色 = 关闭

 *   OVER/SPLIT:

 *     OVER  = 两路波形画在同一个大框里

 *     SPLIT = 上下两个独立框

 *

 *   TODO-15:

 *     数字面板现在改的是 s_freq_edit/s_duty_edit,

 *     按 SET 调 PWM_Set(); 真正生效要等 PWM_SetFreq/PWM_SetDuty 写完。

 */

static const UI_Rect s_ch1_btn     = {   8, 164,  70, 26 };

static const UI_Rect s_ch2_btn     = {  86, 164,  70, 26 };

static const UI_Rect s_mode_btn    = { 164, 164,  68, 26 };



static const UI_Rect s_freq_minus  = { 160, 194,  28, 24 };

static const UI_Rect s_freq_plus   = { 196, 194,  28, 24 };

static const UI_Rect s_duty_minus  = { 160, 222,  28, 24 };

static const UI_Rect s_duty_plus   = { 196, 222,  28, 24 };

static const UI_Rect s_phase_minus = { 160, 250,  28, 24 };

static const UI_Rect s_phase_plus  = { 196, 250,  28, 24 };



/* 点数字值区域 -> 打开数字键盘 */

static const UI_Rect s_freq_value  = {  44, 192, 104, 22 };

static const UI_Rect s_duty_value  = {  44, 220, 104, 22 };

static const UI_Rect s_phase_value = {  44, 248, 104, 22 };



static u8  s_overlay_mode = 1;      /* 1=OVER 一个大框, 0=SPLIT 上下两个框 */

static u8  s_edit_ch = 0;           /* 0=CH1, 1=CH2, 数字键盘作用于哪个通道 */

static u32 s_freq_edit = 1000;

static u16 s_duty_edit = 50;

static u16 s_phase_edit = 0;

static u8  s_input_last_valid = 0;



/* 滑条拖动状态 */

static u8  s_drag_active = 0;

static u8  s_drag_ch = 0;

static u16 s_drag_last_duty = 0;



static void Pwm_Enter(void)

{

    s_overlay_mode = 1;

    s_edit_ch = 0;

    s_freq_edit = g_pwm.freq_set_hz;

    s_duty_edit = g_pwm.duty_set_pct;

    s_phase_edit = PWM2_GetPhaseDeg();

    s_input_last_valid = 0;

    s_drag_active = 0;

    g_ui_drag_lock = 0;

}



/* 数字键盘回调: 确认时立即生效, 不再需要 SET */

static void Keypad_Freq(int32_t v)

{

    if (v < 1) v = 1;

    if (v > (int32_t)PWM_FREQ_MAX_HZ) v = (int32_t)PWM_FREQ_MAX_HZ;

    s_freq_edit = (u32)v;

    PWM_SetFreq((u32)v);

    UI_MarkDirty();

}



static void Keypad_Duty(int32_t v)

{

    if (v < 0) v = 0;

    if (v > 100) v = 100;



    /* 500k 模式 5% 一档; 200k 模式 1% 一档 */

    v = ((v + PWM_DUTY_STEP_PCT / 2) / PWM_DUTY_STEP_PCT) * PWM_DUTY_STEP_PCT;

    if (v > 100) v = 100;



    s_duty_edit = (u16)v;

    if (s_edit_ch == 0)

    {

        PWM_SetDutyCH1((u16)v);

    }

    else

    {

        PWM2_SetDuty((u16)v);

    }

    UI_MarkDirty();

}



static void Keypad_Phase(int32_t v)

{

    if (s_edit_ch != 1) return;      /* CH1 没有相位 */

    if (v < 0) v = 0;

    if (v > 360) v = 360;

    v = (v / 5) * 5;                 /* 5 度一档 */

    s_phase_edit = (u16)v;

    PWM2_SetPhaseDeg((u16)v);

    UI_MarkDirty();

}



/*----------------------------------------------------------------------------

 * 画一条 PWM 波形 + 底部的占空比滑条

 *   selected=1 时波形加粗

 *----------------------------------------------------------------------------*/

static void Meter_LineH(u16 x1, u16 x2, u16 y, u16 color, u8 thick)

{

    if (x2 < x1) return;

    if (thick < 1) thick = 1;

    UI_Fill(x1, y, x2, (u16)(y + thick - 1), color);

}



static void Meter_LineV(u16 x, u16 y1, u16 y2, u16 color, u8 thick)

{

    u16 t;



    /* y1 可能大于 y2, 统一成上小下大, 否则 UI_Fill 直接返回, 竖线就没了 */

    if (y1 > y2)

    {

        t = y1;

        y1 = y2;

        y2 = t;

    }



    if (thick < 1) thick = 1;

    UI_Fill(x, y1, (u16)(x + thick - 1), y2, color);

}



/*---------------------------------------------------------------------------

 * 拖动时先画到 RAM, 再一次性刷到 LCD, 避免"先擦后画"造成屏闪。

 * 只缓存车道顶部 14px 以下的部分, 标签不参与。

 *-------------------------------------------------------------------------*/

#define METER_BUF_W 220

#define METER_BUF_H 48

static u16 s_meter_buf[METER_BUF_W * METER_BUF_H];



static void Meter_BufFill(u16 x0, u16 y0, u16 x1, u16 y1, u16 color)

{

    u16 x;

    u16 y;

    u16 *row;



    if (x0 >= METER_BUF_W || y0 >= METER_BUF_H) return;

    if (x1 >= METER_BUF_W) x1 = METER_BUF_W - 1;

    if (y1 >= METER_BUF_H) y1 = METER_BUF_H - 1;

    if (x0 > x1 || y0 > y1) return;



    for (y = y0; y <= y1; y++)

    {

        row = &s_meter_buf[(u32)y * METER_BUF_W];

        for (x = x0; x <= x1; x++) row[x] = color;

    }

}



static void Meter_BufH(u16 x1, u16 x2, u16 y, u16 color, u8 thick)

{

    if (x1 > x2) return;

    if (thick < 1) thick = 1;

    Meter_BufFill(x1, y, x2, (u16)(y + thick - 1), color);

}



static void Meter_BufV(u16 x, u16 y1, u16 y2, u16 color, u8 thick)

{

    u16 t;



    if (y1 > y2)

    {

        t = y1;

        y1 = y2;

        y2 = t;

    }

    if (thick < 1) thick = 1;

    Meter_BufFill(x, y1, (u16)(x + thick - 1), y2, color);

}



static void Meter_BufSlider(u16 w, u16 bh, u16 duty, u16 color)

{

    u16 track_y = (u16)(bh - 8);

    u16 fill;

    u16 knob_x;



    if (duty > 100) duty = 100;



    Meter_BufFill(2, track_y, (u16)(w - 5), (u16)(track_y + 3),

                  UI_COLOR_BORDER);



    fill = (u16)(((u32)(w - 8) * duty) / 100UL);

    if (fill > 0)

    {

        Meter_BufFill(2, track_y, (u16)(2 + fill), (u16)(track_y + 3), color);

    }



    knob_x = (u16)(2 + fill);

    if (knob_x > (u16)(w - 7)) knob_x = (u16)(w - 7);

    Meter_BufFill(knob_x, (u16)(track_y - 2), (u16)(knob_x + 5),

                  (u16)(track_y + 4), UI_COLOR_TEXT);

}



static void Meter_BufWave(u16 w, u16 bh, u16 duty, u16 color)

{

    u16 lane_h = (u16)(bh + 14);

    u16 period;

    u16 high;

    u16 yhi;

    u16 ylo;

    u16 i;

    u16 sx;

    u16 hx;

    u16 end;

    u16 thick = 3;



    if (duty > 100) duty = 100;



    period = w / 3;

    if (period < 9) period = 9;



    high = (u16)(((u32)period * duty) / 100UL);

    if (high < 1) high = 1;

    if (high > period) high = period;



    yhi = (u16)(lane_h / 2 - 24);

    ylo = (u16)(lane_h / 2 - 4);



    if (duty == 0)

    {

        Meter_BufH(0, (u16)(w - 1), ylo, color, thick);

        return;

    }

    if (duty >= 100)

    {

        Meter_BufH(0, (u16)(w - 1), yhi, color, thick);

        return;

    }



    Meter_BufV(0, ylo, yhi, color, thick);



    for (i = 0; i < 3; i++)

    {

        sx = (u16)(i * period);

        hx = (u16)(sx + high);

        if (hx > w) hx = w;



        end = (u16)(sx + period);

        if (end > w) end = w;



        Meter_BufH(sx, (u16)(hx - 1), yhi, color, thick);

        if (hx < end)

        {

            Meter_BufH(hx, (u16)(end - 1), ylo, color, thick);

        }



        if (i > 0) Meter_BufV(sx, ylo, yhi, color, thick);

        if (hx < w) Meter_BufV(hx, yhi, ylo, color, thick);

    }

}



static void Meter_BlitDrag(u16 x, u16 y, u16 w, u16 bh)

{

    u32 n;

    u32 i;



    if (w > METER_BUF_W || bh > METER_BUF_H || bh < 20) return;



    LCD_Set_Window(x, y, (u16)(x + w - 1), (u16)(y + bh - 1));

    n = (u32)w * bh;

    for (i = 0; i < n; i++)

    {

        LCD_WriteData_Color(s_meter_buf[i]);

    }

}



static void Meter_DrawDragBuffer(const UI_Rect *r, u16 duty, u16 color)

{

    u16 bh = (u16)(r->h - 14);



    if (bh > METER_BUF_H) bh = METER_BUF_H;



    Meter_BufFill(0, 0, (u16)(r->w - 1), (u16)(bh - 1), UI_COLOR_BG);

    Meter_BufSlider(r->w, bh, duty, color);

    Meter_BufWave(r->w, bh, duty, color);

    Meter_BlitDrag(r->x, (u16)(r->y + 14), r->w, bh);

}



static void Meter_LaneRect(u8 ch, UI_Rect *r)

{

    r->x = 10;

    r->w = 220;



    if (s_overlay_mode)

    {

        r->y = (ch == 0) ? 28 : 94;

        r->h = 62;

    }

    else

    {

        r->y = (ch == 0) ? 28 : 98;

        r->h = (ch == 0) ? 62 : 60;

    }

}



static void Meter_DrawSlider(u16 x, u16 y, u16 w, u16 h,

                             u16 color, u16 duty, u8 show, u8 bold)

{

    u16 track_y;

    u16 fill;

    u16 knob_x;



    if (!show) return;

    if (duty > 100) duty = 100;



    track_y = (u16)(y + h - 8);

    UI_Fill(x + 2, track_y, (u16)(x + w - 5), (u16)(track_y + 3),

            UI_COLOR_BORDER);



    fill = (u16)(((u32)(w - 8) * duty) / 100UL);

    if (fill > 0)

    {

        UI_Fill(x + 2, track_y, (u16)(x + 2 + fill), (u16)(track_y + 3), color);

    }



    knob_x = (u16)(x + 2 + fill);

    if (knob_x > (u16)(x + w - 7)) knob_x = (u16)(x + w - 7);

    UI_Fill(knob_x, (u16)(track_y - 2), (u16)(knob_x + 5),

            (u16)(track_y + 4), bold ? UI_COLOR_TEXT : color);

}



static void Meter_DrawLane(u16 x, u16 y, u16 w, u16 h,

                           u16 color, u8 on, const char *label,

                           u16 duty, u8 selected, u8 bold)

{

    u16 period;

    u16 high;

    u16 yhi;

    u16 ylo;

    u16 i;

    u16 sx;

    u16 hx;

    u16 end;

    u16 thick;



    UI_TextColor(x + 2, y + 1, 60, 12, UI_FONT_S,

                 color, UI_COLOR_BG, label);



    if (duty > 100) duty = 100;



    /* 只有选中且正在输出的通道画滑条; 关闭后拖动无效 */

    if (selected && on)

    {

        Meter_DrawSlider(x, y, w, h, color, duty, 1, bold);

    }



    if (!on)

    {

        UI_TextColor(x + 2, y + h / 2 - 6, 60, 12, UI_FONT_S,

                     UI_COLOR_MUTED, UI_COLOR_BG, "关闭");

        return;

    }



    period = w / 3;

    if (period < 9) period = 9;



    high = (u16)(((u32)period * duty) / 100UL);

    if (high < 1) high = 1;

    if (high > period) high = period;



    yhi = (u16)(y + h / 2 - 10);

    ylo = (u16)(y + h / 2 + 10);

    if (yhi < (u16)(y + 15)) yhi = (u16)(y + 15);

    if (ylo > (u16)(y + h - 3)) ylo = (u16)(y + h - 3);



    thick = bold ? 3 : 1;



    /* 0% 和 100% 单独处理, 避免画出两条平行线 */

    if (duty == 0)

    {

        Meter_LineH(x, (u16)(x + w - 1), ylo, color, thick);

        return;

    }

    if (duty >= 100)

    {

        Meter_LineH(x, (u16)(x + w - 1), yhi, color, thick);

        return;

    }



    /* 起始沿 */

    Meter_LineV(x, ylo, yhi, color, thick);



    /* 每个周期: 高电平段 + 低电平段 + 两个跳变沿 */

    for (i = 0; i < 3; i++)

    {

        sx = (u16)(x + i * period);

        hx = (u16)(sx + high);

        if (hx > (u16)(x + w)) hx = (u16)(x + w);



        end = (u16)(sx + period);

        if (end > (u16)(x + w)) end = (u16)(x + w);



        /* 高电平段 */

        Meter_LineH(sx, (u16)(hx - 1), yhi, color, thick);



        /* 低电平段: 从高电平结束到本周期结束 */

        if (hx < end)

        {

            Meter_LineH(hx, (u16)(end - 1), ylo, color, thick);

        }



        /* 周期起点/下降沿 */

        if (i > 0)

        {

            Meter_LineV(sx, ylo, yhi, color, thick);

        }

        if (hx < (u16)(x + w))

        {

            Meter_LineV(hx, yhi, ylo, color, thick);

        }

    }

}



/* 通道开关按钮: 亮色=输出, 背景色=关闭 */

static void Meter_DrawChannelBtn(const UI_Rect *r, const char *label,

                                 u8 on, u16 color)

{

    u16 bg = on ? color : UI_COLOR_BG;

    u16 fg = on ? UI_COLOR_BG : UI_COLOR_MUTED;

    u16 tw;

    u16 tx;

    u16 ty;

    const char *p;



    tw = 0;

    for (p = label; *p != '\0'; p++) tw = (u16)(tw + UI_FONT_M / 2);



    UI_Fill(r->x, r->y, (u16)(r->x + r->w - 1), (u16)(r->y + r->h - 1), UI_COLOR_BG);
    UI_FillRound(r->x, r->y, (u16)(r->x + r->w - 1), (u16)(r->y + r->h - 1), 7, bg);

    UI_FrameRound(r->x, r->y, (u16)(r->x + r->w - 1), (u16)(r->y + r->h - 1),

                  7, on ? UI_COLOR_TEXT : UI_COLOR_BORDER);



    tx = (u16)(r->x + (r->w - tw) / 2);

    ty = (u16)(r->y + (r->h - UI_FONT_M) / 2);

    UI_TextColor(tx, ty, r->w, UI_FONT_M, UI_FONT_M, fg, bg, label);

}



/* 上半屏波形区 */

static void Meter_DrawWaveArea(void)

{

    UI_Rect r;



    if (s_overlay_mode)

    {

        UI_FrameRound(8, 26, 231, 160, 8, UI_COLOR_BORDER);

    }

    else

    {

        UI_FrameRound(8, 26, 231, 92, 8, UI_COLOR_BORDER);

        UI_FrameRound(8, 96, 231, 160, 8, UI_COLOR_BORDER);

    }



    Meter_LaneRect(0, &r);

    UI_Fill(r.x, r.y, (u16)(r.x + r.w - 1), (u16)(r.y + r.h - 1),

            UI_COLOR_BG);

    Meter_DrawLane(r.x, r.y, r.w, r.h, UI_COLOR_ACCENT,

                   g_pwm.running, "CH1", g_pwm.duty_actual_pct,

                   (u8)(s_edit_ch == 0),

                   (u8)(s_drag_active && s_drag_ch == 0));



    Meter_LaneRect(1, &r);

    UI_Fill(r.x, r.y, (u16)(r.x + r.w - 1), (u16)(r.y + r.h - 1),

            UI_COLOR_BG);

    Meter_DrawLane(r.x, r.y, r.w, r.h, UI_COLOR_OK,

                   g_pwm2_enabled, "CH2", PWM2_GetDuty(),

                   (u8)(s_edit_ch == 1),

                   (u8)(s_drag_active && s_drag_ch == 1));

}



static void Pwm_Draw(void)

{

    Page_Begin("信号发生");



    /* 输入信号只是一个小提示, 不抢主界面布局 */

    if (g_cap.valid)

    {

        UI_TextColor(184, 4, 20, 14, UI_FONT_S,

                     UI_COLOR_OK, UI_COLOR_PANEL, "入");

    }



    Meter_DrawWaveArea();



    /* 通道 + 叠显/分显 */

    Meter_DrawChannelBtn(&s_ch1_btn, "CH1", g_pwm.running, UI_COLOR_ACCENT);

    Meter_DrawChannelBtn(&s_ch2_btn, "CH2", g_pwm2_enabled, UI_COLOR_OK);

    UI_DrawButton(&s_mode_btn, s_overlay_mode ? "叠加" : "分屏", 0);



    /* 数字面板 */

    UI_TextColor(8, 198, 44, 12, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_BG, "频率");

    UI_NumColor(48, 194, s_freq_edit, 6, UI_FONT_M,

                (s_freq_edit == g_pwm.freq_set_hz) ? UI_COLOR_VALUE : UI_COLOR_EDIT,

                UI_COLOR_BG);



    UI_TextColor(8, 226, 44, 12, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_BG, "占空比");

    UI_NumColor(60, 222, s_duty_edit, 3, UI_FONT_M,

                (s_duty_edit == ((s_edit_ch == 0) ? g_pwm.duty_set_pct : PWM2_GetDuty()))

                ? UI_COLOR_VALUE : UI_COLOR_EDIT,

                UI_COLOR_BG);



    UI_DrawButton(&s_freq_minus, "-", 0);

    UI_DrawButton(&s_freq_plus,  "+", 0);

    UI_DrawButton(&s_duty_minus, "-", 0);

    UI_DrawButton(&s_duty_plus,  "+", 0);



    UI_TextColor(8, 254, 44, 12, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_BG, "相位");

    if (s_edit_ch == 1)

    {

        UI_NumColor(48, 250, s_phase_edit, 3, UI_FONT_M,

                    (s_phase_edit == PWM2_GetPhaseDeg()) ? UI_COLOR_VALUE : UI_COLOR_EDIT,

                    UI_COLOR_BG);

    }

    else

    {

        UI_TextColor(48, 250, 60, 16, UI_FONT_M,

                     UI_COLOR_MUTED, UI_COLOR_BG, "---");

    }

    UI_DrawButton(&s_phase_minus, "-", 0);

    UI_DrawButton(&s_phase_plus,  "+", 0);

}



/* 拖动时只重画当前车道和占空比数字, 不整页重画 */

static void Pwm_DrawDrag(u16 old_duty)

{

    UI_Rect r;

    u16 color;

    u16 duty;

    u8 on;



    (void)old_duty;



    Meter_LaneRect(s_drag_ch, &r);

    color = (s_drag_ch == 0) ? UI_COLOR_ACCENT : UI_COLOR_OK;

    on = (s_drag_ch == 0) ? g_pwm.running : g_pwm2_enabled;

    duty = (s_drag_ch == 0) ? g_pwm.duty_actual_pct : PWM2_GetDuty();



    /* 波形和滑条先画到 RAM, 再整块刷到 LCD: 不会出现"先擦黑再画"的闪 */

    if (on)

    {

        Meter_DrawDragBuffer(&r, duty, color);

    }



    UI_NumColor(60, 222, s_duty_edit, 3, UI_FONT_M,

                UI_COLOR_VALUE, UI_COLOR_BG);

}



static u8 Meter_HitPad(const UI_Rect *r, u16 x, u16 y,

                       u8 pad_x, u8 pad_y)

{

    UI_Rect t = *r;



    if (t.x > pad_x) t.x = (u16)(t.x - pad_x);

    else             t.x = 0;



    if (t.y > pad_y) t.y = (u16)(t.y - pad_y);

    else             t.y = 0;



    t.w = (u16)(t.w + pad_x * 2);

    t.h = (u16)(t.h + pad_y * 2);



    if ((u32)t.x + t.w > UI_SCREEN_W) t.w = (u16)(UI_SCREEN_W - t.x);

    if ((u32)t.y + t.h > UI_SCREEN_H) t.h = (u16)(UI_SCREEN_H - t.y);



    return UI_HitTest(&t, x, y);

}



static void Pwm_DragTo(u16 x)

{

    UI_Rect r;

    int x0;

    int x1;

    int v;

    u16 old_duty;



    /* 绝对位置映射: 滑条前端/旋钮跟手指位置走 */

    Meter_LaneRect(s_drag_ch, &r);

    x0 = (int)r.x + 2;

    x1 = (int)r.x + (int)r.w - 5;



    if (x1 <= x0)

    {

        v = 0;

    }

    else if ((int)x <= x0)

    {

        v = 0;

    }

    else if ((int)x >= x1)

    {

        v = 100;

    }

    else

    {

        v = ((int)x - x0) * 100 / (x1 - x0);

    }



    if (v < 0) v = 0;

    if (v > 100) v = 100;



    if ((u16)v == s_drag_last_duty) return;



    old_duty = s_drag_last_duty;

    s_drag_last_duty = (u16)v;

    s_duty_edit = (u16)v;

    if (s_drag_ch == 0)

    {

        PWM_SetDutyCH1((u16)v);

    }

    else

    {

        PWM2_SetDuty((u16)v);

    }

    Pwm_DrawDrag(old_duty);

}



static void Pwm_Event(const UI_Event *ev)

{

    UI_Rect r;

    u8 ch;



    /* 滑条: 按下记录起点, 拖动跟手, 抬起结束 */

    if (ev->type == UI_EV_MOVE && s_drag_active)

    {

        Pwm_DragTo(ev->x);

        return;

    }



    if (ev->type == UI_EV_UP)

    {

        if (s_drag_active)

        {

            Pwm_DragTo(ev->x);

            s_drag_active = 0;

            g_ui_drag_lock = 0;



            /* 抬起只恢复本页车道: 不再整页重画, 也不保留加粗旋钮 */

            Meter_DrawWaveArea();

        }

        return;

    }



    /* 拖动期间再按下的触摸全部忽略, 只认当前这一次拖动 */

    if (s_drag_active) return;



    if (ev->type != UI_EV_DOWN) return;



    /* CH1/CH2 矩形只负责输出开关, 不负责选中 */

    if (Meter_HitPad(&s_ch1_btn, ev->x, ev->y, 2, 2))

    {

        PWM_Enable(g_pwm.running ? 0 : 1);

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_ch2_btn, ev->x, ev->y, 2, 2))

    {

        PWM2_Enable(g_pwm2_enabled ? 0 : 1);

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_mode_btn, ev->x, ev->y, 2, 2))

    {

        s_overlay_mode = (u8)(s_overlay_mode ? 0 : 1);

        UI_MarkDirty();

        return;

    }



    /* 点波形区域 = 选中该通道; 关闭的通道不能拖动 */

    for (ch = 0; ch < 2; ch++)

    {

        u8 ch_on;



        Meter_LaneRect(ch, &r);

        if (!UI_HitTest(&r, ev->x, ev->y)) continue;



        s_duty_edit = (ch == 0) ? g_pwm.duty_set_pct : PWM2_GetDuty();

        s_phase_edit = PWM2_GetPhaseDeg();

        ch_on = (ch == 0) ? g_pwm.running : g_pwm2_enabled;



        if (s_edit_ch != ch)

        {

            s_edit_ch = ch;

            UI_MarkDirty();          /* 只在这里整页重画一次, 显示加粗和滑条 */

        }



        if (!ch_on) return;          /* 关闭的通道只选中, 不调占空比 */



        s_drag_active = 1;

        s_drag_ch = ch;

        s_drag_last_duty = s_duty_edit;

        g_ui_drag_lock = 1;          /* 拖动期间屏蔽整页重画和导航 */



        /* 按下时直接按手指位置定位, 让滑条前端跟手 */

        Pwm_DragTo(ev->x);

        return;

    }



    /* 点数字值 -> 数字键盘 */

    if (Meter_HitPad(&s_freq_value, ev->x, ev->y, 2, 2))

    {

        UI_Keypad_Open(0, (int32_t)s_freq_edit, 6, Keypad_Freq);

        return;

    }



    if (Meter_HitPad(&s_duty_value, ev->x, ev->y, 2, 2))

    {

        UI_Keypad_Open(1, (int32_t)s_duty_edit, 3, Keypad_Duty);

        return;

    }



    if (s_edit_ch == 1 &&

        Meter_HitPad(&s_phase_value, ev->x, ev->y, 2, 2))

    {

        UI_Keypad_Open(2, (int32_t)s_phase_edit, 3, Keypad_Phase);

        return;

    }



    /* +/- 快速调节, 立即生效 */

    if (Meter_HitPad(&s_freq_minus, ev->x, ev->y, 3, 2))

    {

        if (s_freq_edit > 100UL) s_freq_edit -= 100UL;

        else s_freq_edit = 1;

        PWM_SetFreq(s_freq_edit);

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_freq_plus, ev->x, ev->y, 3, 2))

    {

        if (s_freq_edit < PWM_FREQ_MAX_HZ - 100UL)

            s_freq_edit += 100UL;

        else

            s_freq_edit = PWM_FREQ_MAX_HZ;

        PWM_SetFreq(s_freq_edit);

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_duty_minus, ev->x, ev->y, 3, 2))

    {

        if (s_duty_edit >= PWM_DUTY_STEP_PCT)

            s_duty_edit = (u16)(s_duty_edit - PWM_DUTY_STEP_PCT);

        else

            s_duty_edit = 0;

        if (s_edit_ch == 0) PWM_SetDutyCH1(s_duty_edit);

        else                PWM2_SetDuty(s_duty_edit);

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_duty_plus, ev->x, ev->y, 3, 2))

    {

        if (s_duty_edit + PWM_DUTY_STEP_PCT <= 100)

            s_duty_edit = (u16)(s_duty_edit + PWM_DUTY_STEP_PCT);

        else

            s_duty_edit = 100;

        if (s_edit_ch == 0) PWM_SetDutyCH1(s_duty_edit);

        else                PWM2_SetDuty(s_duty_edit);

        UI_MarkDirty();

        return;

    }



    if (s_edit_ch == 1 && Meter_HitPad(&s_phase_minus, ev->x, ev->y, 3, 2))

    {

        if (s_phase_edit >= 5) s_phase_edit -= 5;

        else s_phase_edit = 360;

        PWM2_SetPhaseDeg(s_phase_edit);

        UI_MarkDirty();

        return;

    }



    if (s_edit_ch == 1 && Meter_HitPad(&s_phase_plus, ev->x, ev->y, 3, 2))

    {

        if (s_phase_edit <= 355) s_phase_edit += 5;

        else s_phase_edit = 0;

        PWM2_SetPhaseDeg(s_phase_edit);

        UI_MarkDirty();

        return;

    }

}



static void Pwm_Tick(u32 now_ms)

{

    u8 in_valid = g_cap.valid ? 1 : 0;



    (void)now_ms;



    /* 输入状态变化时才重画 IN 标记, 不周期性重画波形区 */

    if (in_valid != s_input_last_valid)

    {

        s_input_last_valid = in_valid;

        if (!s_drag_active) UI_MarkDirty();

    }

}



/*============================ Capture =====================================*/

/* 最小版本只读 g_cap 显示; CAP_Update 在 APP_Loop 里算。

 * TODO-16: 美化/加信号状态图标。 */

static void Cap_Draw(void)

{

    Page_Begin("输入捕获");



    Page_Label(8, 30, "频率(Hz)");

    UI_Fill(8, 46, 120, 70, UI_COLOR_BG);

    if (g_cap.valid) Page_Value(8, 46, g_cap.freq_hz, 6);

    else             Page_Text(8, 50, UI_COLOR_MUTED, "---");



    Page_Label(8, 80, "占空比(%)");

    UI_Fill(8, 96, 140, 120, UI_COLOR_BG);

    if (g_cap.valid)

    {

        /* duty_permille 0..1000 -> 100.0% 需要 3 位整数 */

        UI_NumColor(8,  96, g_cap.duty_permille / 10,  3, UI_FONT_L, UI_COLOR_VALUE, UI_COLOR_BG);

        UI_TextColor(44, 96, 12, UI_FONT_L, UI_FONT_L, UI_COLOR_VALUE, UI_COLOR_BG, ".");

        UI_NumColor(56, 96, g_cap.duty_permille % 10,  1, UI_FONT_L, UI_COLOR_VALUE, UI_COLOR_BG);

        UI_TextColor(68, 96, 12, UI_FONT_L, UI_FONT_L, UI_COLOR_VALUE, UI_COLOR_BG, "%");

    }

    else

    {

        Page_Text(8, 100, UI_COLOR_MUTED, "---");

    }



    Page_Label(8, 136, "周期(计数)");

    Page_Value(8, 152, g_cap.period_ticks, 9);



    Page_Label(8, 186, "高电平(计数)");

    Page_Value(8, 202, g_cap.high_ticks, 9);



    Page_Text(8, 238, g_cap.valid ? UI_COLOR_OK : UI_COLOR_ERR,

              g_cap.valid ? "信号正常" : "无信号");

#if APP_ENABLE_EXT500K

    Page_Text(8, 258, UI_COLOR_MUTED, "1Hz-500kHz, <=3.3V, GND");

#else

    Page_Text(8, 258, UI_COLOR_MUTED, "1Hz-200kHz, <=3.3V, GND");

#endif

}



static void Cap_Event(const UI_Event *ev)

{

    (void)ev;

    /* TODO-16: 需要的话加 HOLD/AVG 按钮 */

}



static void Cap_Tick(u32 now_ms)

{

    static u32 last = 0;

    static u32 last_freq = 0;

    static u32 last_period = 0;

    static u32 last_high = 0;

    static u16 last_duty = 0;

    static u8  last_valid = 0xFF;



    /* 只有显示值真的变化才重画, 无信号/噪声时不周期刷屏 */

    if ((u32)(now_ms - last) >= 200)

    {

        last = now_ms;



        if (g_cap.valid != last_valid ||

            g_cap.freq_hz != last_freq ||

            g_cap.period_ticks != last_period ||

            g_cap.high_ticks != last_high ||

            g_cap.duty_permille != last_duty)

        {

            last_valid  = g_cap.valid;

            last_freq   = g_cap.freq_hz;

            last_period = g_cap.period_ticks;

            last_high   = g_cap.high_ticks;

            last_duty   = g_cap.duty_permille;

            UI_MarkDirty();

        }

    }

}



/*============================ Stopwatch ===================================*/

/* 5. 定时器正计时: 暂停/重置, 毫秒显示, LAP(本圈) / SPLIT(累计) */

static const UI_Rect s_sw_start = {  14, 206,  60, 60 };

static const UI_Rect s_sw_lap   = {  90, 206,  60, 60 };

static const UI_Rect s_sw_reset = { 166, 206,  60, 60 };



static u8 s_sw_btns_valid = 0;
static u8 s_sw_btn_running = 0xFF;

static void Stopwatch_Enter(void)
{
    s_sw_btns_valid = 0;
    s_sw_btn_running = 0xFF;
}

static void Stopwatch_Draw(void)

{

    u8 running;
    u16 cx;
    u16 cy;



    if (g_ui_full_redraw) s_sw_btns_valid = 0;

    Page_Begin("秒表");

    running = STOPWATCH_IsRunning();



    Page_Label(8, 28, "总时间");

    Page_Time(8, 42, STOPWATCH_ElapsedMs(), UI_COLOR_VALUE);



    Page_Label(8, 78, "单圈");

    Page_Time(8, 92, STOPWATCH_LapMs(), UI_COLOR_TEXT);



    Page_Label(8, 128, "分段");

    Page_Time(8, 142, STOPWATCH_SplitMs(), UI_COLOR_TEXT);



    Page_Text(8, 180,

              STOPWATCH_IsRunning() ? UI_COLOR_OK : UI_COLOR_MUTED,

              STOPWATCH_IsRunning() ? "运行中" : "已暂停");



    UI_TextColor(150, 180, 40, 12, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_BG, "圈数");

    UI_NumColor(190, 176, g_sw.lap_count, 3, UI_FONT_M,

                UI_COLOR_VALUE, UI_COLOR_BG);



    if (!s_sw_btns_valid || running != s_sw_btn_running)
    {
        cx = (u16)(s_sw_start.x + s_sw_start.w / 2);
        cy = (u16)(s_sw_start.y + s_sw_start.h / 2);

        UI_DrawCircleButton(&s_sw_start, "", 0, UI_COLOR_BG);
        if (running) UI_DrawPauseIcon(cx, cy, UI_COLOR_TEXT);
        else         UI_DrawPlayIcon(cx, cy, UI_COLOR_TEXT);

        UI_DrawCircleButton(&s_sw_lap, "计圈", 0, UI_COLOR_BG);
        UI_DrawCircleButton(&s_sw_reset, "复位", 0, UI_COLOR_BG);
        s_sw_btns_valid = 1;
        s_sw_btn_running = running;
    }

}



static void Stopwatch_Event(const UI_Event *ev)

{

    if (ev->type != UI_EV_DOWN) return;



    if (Meter_HitPad(&s_sw_start, ev->x, ev->y, 2, 2))

    {

        if (STOPWATCH_IsRunning()) STOPWATCH_Pause();

        else                       STOPWATCH_Start();

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_sw_lap, ev->x, ev->y, 2, 2))

    {

        STOPWATCH_Lap();

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_sw_reset, ev->x, ev->y, 2, 2))

    {

        STOPWATCH_Reset();

        UI_MarkDirty();

        return;

    }

}



static void Stopwatch_Tick(u32 now_ms)

{

    static u32 last = 0;



    /* 只有运行中才需要每秒 20 次刷新; 暂停时不用重画 */

    if (STOPWATCH_IsRunning() && (u32)(now_ms - last) >= 50)

    {

        last = now_ms;

        UI_MarkDirty();

    }

}



/*============================== Sweep =====================================*/

/* 自动扫频自检: 本机 PWM -> PA3 -> PA0 -> 输入捕获 */

static const UI_Rect s_sweep_btn = { 150, 26, 82, 24 };



static void Sweep_Draw(void)

{

    const SWEEP_Row *rows = SWEEP_GetRows();

    u8 count = SWEEP_GetCount();

    u8 i;

    u16 y;

    u16 color;

    const char *result;

    u16 result_color;



    Page_Begin("扫频自检");



    UI_DrawButton(&s_sweep_btn, SWEEP_IsActive() ? "中止" : "开始", 0);



    /* 表头 */

    UI_TextColor(8,   48, 60, 12, UI_FONT_S, UI_COLOR_MUTED, UI_COLOR_BG, "设定");

    UI_TextColor(70,  48, 60, 12, UI_FONT_S, UI_COLOR_MUTED, UI_COLOR_BG, "实测");

    UI_TextColor(140, 48, 40, 12, UI_FONT_S, UI_COLOR_MUTED, UI_COLOR_BG, "误差");



    for (i = 0; i < SWEEP_POINT_COUNT; i++)

    {

        y = (u16)(66 + i * 15);



        if (i < count)

        {

            color = rows[i].pass ? UI_COLOR_OK : UI_COLOR_ERR;



            UI_NumColor(8, y, rows[i].set_hz, 6, UI_FONT_S, color, UI_COLOR_BG);



            if (rows[i].meas_hz == 0)

            {

                UI_TextColor(70, y, 60, 12, UI_FONT_S, color, UI_COLOR_BG, "---");

            }

            else

            {

                UI_NumColor(70, y, rows[i].meas_hz, 6, UI_FONT_S,

                            color, UI_COLOR_BG);

            }



            UI_NumColor(140, y, (u32)(rows[i].err_x100 / 100), 3,

                        UI_FONT_S, color, UI_COLOR_BG);

            UI_TextColor(158, y, 6, 12, UI_FONT_S, color, UI_COLOR_BG, ".");

            UI_NumColor(164, y, (u32)(rows[i].err_x100 % 100), 2,

                        UI_FONT_S, color, UI_COLOR_BG);

            UI_TextColor(176, y, 8, 12, UI_FONT_S, color, UI_COLOR_BG, "%");

        }

        else

        {

            UI_TextColor(8,   y, 60, 12, UI_FONT_S, UI_COLOR_MUTED, UI_COLOR_BG, "---");

            UI_TextColor(70,  y, 60, 12, UI_FONT_S, UI_COLOR_MUTED, UI_COLOR_BG, "---");

            UI_TextColor(140, y, 40, 12, UI_FONT_S, UI_COLOR_MUTED, UI_COLOR_BG, "---");

        }

    }



    if (SWEEP_IsDone())

    {

        result = SWEEP_GetPass() ? "通过" : "失败";

        result_color = SWEEP_GetPass() ? UI_COLOR_OK : UI_COLOR_ERR;

    }

    else

    {

        result = SweepStateStr();

        result_color = SWEEP_IsActive() ? UI_COLOR_EDIT : UI_COLOR_MUTED;

    }



    UI_TextColor(8, 248, 120, 16, UI_FONT_S,

                 result_color, UI_COLOR_BG, result);



    UI_NumColor(160, 248, count, 2, UI_FONT_S,

                UI_COLOR_VALUE, UI_COLOR_BG);

    UI_TextColor(176, 248, 30, 12, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_BG, "/12");



    UI_TextColor(8, 266, 220, 12, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_BG, "PA3->PA0 回环");

}



static void Sweep_Event(const UI_Event *ev)

{

    if (ev->type != UI_EV_DOWN) return;



    if (UI_HitTest(&s_sweep_btn, ev->x, ev->y))

    {

        if (SWEEP_IsActive())

        {

            SWEEP_Abort();

        }

        else

        {

            SWEEP_Start();

            /* 清掉上一轮的表格 */

            UI_Fill(0, 52, UI_SCREEN_W - 1, 280, UI_COLOR_BG);

        }

        UI_MarkDirty();

    }

}



static void Sweep_Tick(u32 now_ms)

{

    static u32 last = 0;

    static u8 last_done = 0;



    SWEEP_Poll();



    if (SWEEP_IsActive())

    {

        if ((u32)(now_ms - last) >= 100)

        {

            last = now_ms;

            UI_MarkDirty();

        }

    }

    else if (SWEEP_IsDone() && !last_done)

    {

        UI_MarkDirty();

    }



    last_done = SWEEP_IsDone();

}



/*============================== SPWM ======================================*/

/* 20kHz 载波, 400 点, 50Hz 正弦; PA6 输出, 外部 RC 低通 */

static const UI_Rect s_spwm_btn = { 150, 26, 82, 24 };



static void Spwm_Draw(void)

{

    Page_Begin("正弦调制");



    UI_DrawButton(&s_spwm_btn,

                  g_spwm.enabled ? "停止" : "开始", 0);



    Page_Label(8, 30, "正弦");

    Page_Text(8, 46, UI_COLOR_VALUE, "50 Hz");



    Page_Label(8, 70, "载波");

    Page_Text(8, 86, UI_COLOR_VALUE, "20 kHz");



    Page_Label(8, 110, "点数");

    Page_Text(8, 126, UI_COLOR_VALUE, "400");



    Page_Label(8, 150, "幅值");

    Page_Text(8, 166, UI_COLOR_VALUE, "90%");



    Page_Label(8, 190, "状态");

    Page_Text(8, 206, g_spwm.enabled ? UI_COLOR_OK : UI_COLOR_MUTED,

              g_spwm.enabled ? "运行中" : "已停止");



    Page_Text(8, 236, UI_COLOR_MUTED, "PA6->RC低通");

}



static void Spwm_Event(const UI_Event *ev)

{

    if (ev->type != UI_EV_DOWN) return;



    if (UI_HitTest(&s_spwm_btn, ev->x, ev->y))

    {

        if (g_spwm.enabled) SPWM_Stop();

        else                SPWM_Start();

        UI_MarkDirty();

    }

}



/*============================== Maze ======================================*/

/* 开机长按 START 3 秒的彩蛋。

 *

 * 布局(竖屏 240x320, 进入本页时隐藏底部导航条):

 *   y=0..21    状态栏: MAZE + 状态 + 步数 + 时间

 *   y=26..235  21x21 迷宫 (每格 10px, 210x210)

 *   y=250..271 第一排: AUTO / PAUSE / NEW / SNAKE / BACK

 *   y=275..296 方向键: 上

 *   y=298..319 方向键: 左 / 下 / 右

 *

 * 触摸: 在迷宫上滑动 = 走一格; 也可以点方向键。

 * 按键: WK_UP=上, KEY0=左, KEY1=下, KEY2=右 (app_maze.c 里轮询)。

 * 自动: AUTO 用 A* 求最短路径后回放, PAUSE 可暂停; NEW 换新迷宫。

 * SNAKE: 贪吃蛇仍是最后一页, 从本页点 SNAKE 进去。 */

#define MZ_BX             15

#define MZ_BY             26

#define MZ_CELL           MAZE_CELL_PX

#define MZ_BOARD_W        (MAZE_GRID_W * MAZE_CELL_PX)

#define MZ_BOARD_H        (MAZE_GRID_H * MAZE_CELL_PX)



#define MZ_ROW1_Y         250

#define MZ_ROW2_Y         275

#define MZ_ROW3_Y         298

#define MZ_BTN_H          22

#define MZ_UTIL_W         42

#define MZ_UTIL_GAP       4

#define MZ_UTIL_X0        4

#define MZ_ARROW_W        52



#define MZ_BG_PATH        0x0000

#define MZ_WALL           0x2945

#define MZ_WALL_HI        0x4A69

#define MZ_START          0xFD20

#define MZ_EXIT           0x07E0

#define MZ_EXIT_DARK      0x03E0

#define MZ_PLAYER         0x07FF



static const UI_Rect s_mz_auto  = { MZ_UTIL_X0 + (MZ_UTIL_W + MZ_UTIL_GAP) * 0, MZ_ROW1_Y, MZ_UTIL_W, MZ_BTN_H };

static const UI_Rect s_mz_pause = { MZ_UTIL_X0 + (MZ_UTIL_W + MZ_UTIL_GAP) * 1, MZ_ROW1_Y, MZ_UTIL_W, MZ_BTN_H };

static const UI_Rect s_mz_new   = { MZ_UTIL_X0 + (MZ_UTIL_W + MZ_UTIL_GAP) * 2, MZ_ROW1_Y, MZ_UTIL_W, MZ_BTN_H };

static const UI_Rect s_mz_snake = { MZ_UTIL_X0 + (MZ_UTIL_W + MZ_UTIL_GAP) * 3, MZ_ROW1_Y, MZ_UTIL_W, MZ_BTN_H };

static const UI_Rect s_mz_back  = { MZ_UTIL_X0 + (MZ_UTIL_W + MZ_UTIL_GAP) * 4, MZ_ROW1_Y, MZ_UTIL_W, MZ_BTN_H };



static const UI_Rect s_mz_up    = {  94, MZ_ROW2_Y, MZ_ARROW_W, MZ_BTN_H };

static const UI_Rect s_mz_left  = {  36, MZ_ROW3_Y, MZ_ARROW_W, MZ_BTN_H };

static const UI_Rect s_mz_down  = {  94, MZ_ROW3_Y, MZ_ARROW_W, MZ_BTN_H };

static const UI_Rect s_mz_right = { 152, MZ_ROW3_Y, MZ_ARROW_W, MZ_BTN_H };

static const UI_Rect s_mz_board = { MZ_BX, MZ_BY, MZ_BOARD_W, MZ_BOARD_H };



static u8 s_mz_full = 1;          /* 1=下一帧整页重画 */

static u8 s_mz_swipe_done = 0;    /* 一次滑动只走一格 */



static u16 Mz_CellOf(u8 x, u8 y)

{

    return (u16)((u16)y * MAZE_GRID_W + x);

}



static void Mz_DrawCell(u16 cell)

{

    u8  cx;

    u8  cy;

    u16 x;

    u16 y;

    u16 px;

    u16 py;



    if (cell >= (u16)(MAZE_GRID_W * MAZE_GRID_H)) return;



    cx = (u8)(cell % MAZE_GRID_W);

    cy = (u8)(cell / MAZE_GRID_W);

    x = (u16)(MZ_BX + (u16)cx * MZ_CELL);

    y = (u16)(MZ_BY + (u16)cy * MZ_CELL);



    if (MAZE_GetCell(cx, cy) != 0)

    {

        /* 墙: 深灰块 + 顶部亮线, 和黑色通道区分开 */

        UI_Fill(x, y, (u16)(x + MZ_CELL - 1), (u16)(y + MZ_CELL - 1), MZ_WALL);

        UI_Fill(x, y, (u16)(x + MZ_CELL - 1), y, MZ_WALL_HI);

        return;

    }



    UI_Fill(x, y, (u16)(x + MZ_CELL - 1), (u16)(y + MZ_CELL - 1), MZ_BG_PATH);



    /* 起点标记 */

    if (cx == 1 && cy == 1)

    {

        UI_Fill((u16)(x + 3), (u16)(y + 3), (u16)(x + 6), (u16)(y + 6), MZ_START);

    }



    /* 出口: 绿色方块 */

    if (cx == MAZE_GetGoalX() && cy == MAZE_GetGoalY())

    {

        UI_Fill((u16)(x + 1), (u16)(y + 1), (u16)(x + 8), (u16)(y + 8), MZ_EXIT_DARK);

        UI_Fill((u16)(x + 2), (u16)(y + 2), (u16)(x + 7), (u16)(y + 7), MZ_EXIT);

    }



    /* 玩家: 青色方块, 画在出口/起点上面 */

    if (cx == MAZE_GetPlayerX() && cy == MAZE_GetPlayerY())

    {

        px = (u16)(x + 1);

        py = (u16)(y + 1);

        UI_Fill(px, py, (u16)(px + 7), (u16)(py + 7), MZ_PLAYER);

        UI_Fill((u16)(px + 2), (u16)(py + 2), (u16)(px + 3), (u16)(py + 3), UI_COLOR_BG);

        UI_Fill((u16)(px + 5), (u16)(py + 2), (u16)(px + 6), (u16)(py + 3), UI_COLOR_BG);

    }

}



static void Mz_DrawBoardFull(void)

{

    u8  cx;

    u8  cy;



    for (cy = 0; cy < MAZE_GRID_H; cy++)

    {

        for (cx = 0; cx < MAZE_GRID_W; cx++)

        {

            Mz_DrawCell(Mz_CellOf(cx, cy));

        }

    }



    UI_FrameRound((u16)(MZ_BX - 1), (u16)(MZ_BY - 1),

                  (u16)(MZ_BX + MZ_BOARD_W), (u16)(MZ_BY + MZ_BOARD_H),

                  8, UI_COLOR_ACCENT);

}



static const char *Mz_StateStr(void)

{

    if (MAZE_GetAuto() && MAZE_GetState() == MAZE_RUNNING) return "自动";



    switch (MAZE_GetState())

    {

        case MAZE_RUNNING: return "运行";

        case MAZE_PAUSED:  return "已暂停";

        case MAZE_WIN:     return "胜利";

        default:           return "准备";

    }

}



static u16 Mz_StateColor(void)

{

    switch (MAZE_GetState())

    {

        case MAZE_RUNNING: return MAZE_GetAuto() ? UI_COLOR_OK : UI_COLOR_VALUE;

        case MAZE_PAUSED:  return UI_COLOR_EDIT;

        case MAZE_WIN:     return UI_COLOR_OK;

        default:           return UI_COLOR_MUTED;

    }

}



static void Mz_DrawHud(void)

{

    u32 sec;



    UI_Fill(52, 2, 238, (u16)(UI_STATUS_H - 3), UI_COLOR_PANEL);



    UI_TextColor(52, 4, 70, 14, UI_FONT_S,

                 Mz_StateColor(), UI_COLOR_PANEL, Mz_StateStr());



    UI_TextColor(128, 4, 10, 14, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_PANEL, "S:");

    UI_NumColor(140, 3, (u32)MAZE_GetSteps(), 3, UI_FONT_S,

                UI_COLOR_VALUE, UI_COLOR_PANEL);



    sec = MAZE_GetElapsedMs() / 1000UL;

    if (sec > 999UL) sec = 999UL;



    UI_TextColor(166, 4, 10, 14, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_PANEL, "T:");

    UI_NumColor(178, 3, sec, 3, UI_FONT_S,

                UI_COLOR_VALUE, UI_COLOR_PANEL);

}



static void Mz_DrawButtons(void)

{

    UI_DrawButton(&s_mz_auto, "自动", (u8)(MAZE_GetAuto() ? 1 : 0));

    UI_DrawButton(&s_mz_pause,

                  (MAZE_GetState() == MAZE_PAUSED) ? "运行" : "暂停", 0);

    UI_DrawButton(&s_mz_new, "新局", 0);

    UI_DrawButton(&s_mz_snake, "蛇", 0);

    UI_DrawButton(&s_mz_back, "返回", 0);



    UI_DrawButton(&s_mz_up, "上", 0);

    UI_DrawButton(&s_mz_left, "左", 0);

    UI_DrawButton(&s_mz_down, "下", 0);

    UI_DrawButton(&s_mz_right, "右", 0);

}



static u16 Ui_TextWidthPx(const char *s, u8 ascii_size)

{

    u16 w = 0;



    while (*s != '\0')

    {

        if ((u8)*s >= 0x80)

        {

            w = (u16)(w + 16);

            s += 2;

        }

        else

        {

            w = (u16)(w + ascii_size / 2);

            s++;

        }

    }

    return w;

}



static void Mz_TextCenter(u16 y, u8 font, u16 color, const char *s)

{

    u16 w;

    u16 x;



    w = Ui_TextWidthPx(s, font);

    x = (u16)((UI_SCREEN_W - w) / 2);



    UI_TextColor(x, y, (u16)(w + 2), font, font, color, UI_COLOR_PANEL, s);

}



static void Mz_DrawOverlay(void)

{

    const char *title;

    const char *line2;

    const char *line3;

    u16 color;



    if (MAZE_GetState() == MAZE_PAUSED)

    {

        title = "已暂停";

        line2 = "暂停/继续";

        line3 = MAZE_GetAuto() ? "自动寻路模式" : "滑动或方向键";

        color = UI_COLOR_EDIT;

    }

    else if (MAZE_GetState() == MAZE_WIN)

    {

        title = "迷宫通过";

        line2 = MAZE_GetAuto() ? "自动寻路完成" : "找到出口";

        line3 = "新局重玩";

        color = UI_COLOR_OK;

    }

    else

    {

        return;

    }



    UI_FillRound(28, 106, 211, 206, 12, UI_COLOR_PANEL);

    UI_FrameRound(28, 106, 211, 206, 12, UI_COLOR_ACCENT);



    Mz_TextCenter(118, UI_FONT_L, color, title);

    Mz_TextCenter(152, UI_FONT_S, UI_COLOR_TEXT, line2);

    Mz_TextCenter(174, UI_FONT_S, UI_COLOR_MUTED, line3);

}



static void Mz_DrawFull(void)

{

    g_ui_full_redraw = 1;

    Page_Begin("迷宫");



    Mz_DrawHud();

    Mz_DrawBoardFull();

    Mz_DrawButtons();

    Mz_DrawOverlay();



    MAZE_ClearDirty();

    s_mz_full = 0;

}



static void Mz_Enter(void)

{

    UI_NavSetVisible(0);        /* 全屏游戏: 不要底部 BACK/NEXT */

    s_mz_swipe_done = 0;



    /* 从别的页面回来先暂停; 第一次进来(IDLE)或已经通关则开新局 */

    if (MAZE_GetState() == MAZE_RUNNING)

    {

        MAZE_TogglePause();

    }

    if (MAZE_GetState() == MAZE_IDLE || MAZE_GetState() == MAZE_WIN)

    {

        MAZE_Start();

    }



    s_mz_full = 1;

    MAZE_ClearDirty();

    UI_MarkDirty();

}



static void Mz_Leave(void)

{

    if (MAZE_GetState() == MAZE_RUNNING)

    {

        MAZE_TogglePause();

    }

    s_mz_full = 1;

}



static void Mz_Draw(void)

{

    if (g_ui_full_redraw)

    {

        g_ui_full_redraw = 0;

        s_mz_full = 1;

    }



    if (s_mz_full)

    {

        Mz_DrawFull();

        return;

    }



    Mz_DrawHud();

    Mz_DrawButtons();

}



static void Mz_Event(const UI_Event *ev)

{

    int dx;

    int dy;



    if (ev->type == UI_EV_UP)

    {

        s_mz_swipe_done = 0;

        return;

    }



    /* 滑动走一格: 从按下点算位移, 一次手势只走一格 */

    if (ev->type == UI_EV_MOVE)

    {

        if (s_mz_swipe_done) return;

        if (MAZE_GetAuto() || MAZE_GetState() != MAZE_RUNNING) return;

        if (!UI_HitTest(&s_mz_board, ev->down_x, ev->down_y)) return;



        dx = (int)ev->x - (int)ev->down_x;

        dy = (int)ev->y - (int)ev->down_y;

        if (dx < 0) dx = -dx;

        if (dy < 0) dy = -dy;



        if (dx < 12 && dy < 12) return;



        if (dx > dy)

        {

            MAZE_Move(((int)ev->x >= (int)ev->down_x) ? MAZE_RIGHT : MAZE_LEFT);

        }

        else

        {

            MAZE_Move(((int)ev->y >= (int)ev->down_y) ? MAZE_DOWN : MAZE_UP);

        }



        s_mz_swipe_done = 1;

        if (MAZE_GetState() == MAZE_WIN) s_mz_full = 1;

        UI_MarkDirty();

        return;

    }



    if (ev->type != UI_EV_DOWN) return;



    if (Meter_HitPad(&s_mz_back, ev->x, ev->y, 2, 2))

    {

        UI_GoRoot(PAGE_PWM);

        return;

    }



    if (Meter_HitPad(&s_mz_snake, ev->x, ev->y, 2, 2))

    {

        /* 贪吃蛇仍是最后一页; 从迷宫进去, BACK 再回主界面 */

        UI_GoPage(PAGE_SNAKE);

        return;

    }



    if (Meter_HitPad(&s_mz_auto, ev->x, ev->y, 2, 2))

    {

        /* 自动进行中/暂停时按 AUTO = 切回手动; 其它情况从当前位置开始 A* */

        if (MAZE_GetAuto() &&

            (MAZE_GetState() == MAZE_RUNNING || MAZE_GetState() == MAZE_PAUSED))

        {

            MAZE_SetAuto(0);

        }

        else

        {

            MAZE_StartAuto();

        }

        s_mz_full = 1;

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_mz_pause, ev->x, ev->y, 2, 2))

    {

        if (MAZE_GetState() == MAZE_RUNNING || MAZE_GetState() == MAZE_PAUSED)

        {

            MAZE_TogglePause();

        }

        s_mz_full = 1;

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_mz_new, ev->x, ev->y, 2, 2))

    {

        MAZE_Start();

        s_mz_full = 1;

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_mz_up, ev->x, ev->y, 2, 2))

    {

        if (!MAZE_GetAuto() && MAZE_GetState() == MAZE_RUNNING) MAZE_Move(MAZE_UP);

        if (MAZE_GetState() == MAZE_WIN) s_mz_full = 1;

        UI_MarkDirty();

        return;

    }

    if (Meter_HitPad(&s_mz_left, ev->x, ev->y, 2, 2))

    {

        if (!MAZE_GetAuto() && MAZE_GetState() == MAZE_RUNNING) MAZE_Move(MAZE_LEFT);

        if (MAZE_GetState() == MAZE_WIN) s_mz_full = 1;

        UI_MarkDirty();

        return;

    }

    if (Meter_HitPad(&s_mz_down, ev->x, ev->y, 2, 2))

    {

        if (!MAZE_GetAuto() && MAZE_GetState() == MAZE_RUNNING) MAZE_Move(MAZE_DOWN);

        if (MAZE_GetState() == MAZE_WIN) s_mz_full = 1;

        UI_MarkDirty();

        return;

    }

    if (Meter_HitPad(&s_mz_right, ev->x, ev->y, 2, 2))

    {

        if (!MAZE_GetAuto() && MAZE_GetState() == MAZE_RUNNING) MAZE_Move(MAZE_RIGHT);

        if (MAZE_GetState() == MAZE_WIN) s_mz_full = 1;

        UI_MarkDirty();

        return;

    }



    if (UI_HitTest(&s_mz_board, ev->x, ev->y))

    {

        s_mz_swipe_done = 0;

    }

}



static void Mz_Tick(u32 now_ms)

{

    u8  st0;

    u16 steps0;

    u32 sec0;

    u16 cell;



    st0 = MAZE_GetState();

    steps0 = MAZE_GetSteps();

    sec0 = MAZE_GetElapsedMs() / 1000UL;



    MAZE_Tick(now_ms);



    if (MAZE_GetState() != st0)

    {

        if (MAZE_GetState() == MAZE_WIN || MAZE_GetState() == MAZE_PAUSED)

        {

            s_mz_full = 1;

            MAZE_ClearDirty();

            UI_MarkDirty();

            return;

        }

    }



    if (s_mz_full)

    {

        MAZE_ClearDirty();

        return;

    }



    /* 正常走一步: 只重画旧格/新格 */

    while (MAZE_PopDirtyCell(&cell))

    {

        Mz_DrawCell(cell);

    }



    /* 步数/秒数变化时只重画顶部 HUD */

    if (MAZE_GetSteps() != steps0 ||

        (MAZE_GetElapsedMs() / 1000UL) != sec0)

    {

        Mz_DrawHud();

    }

}



/*============================== Snake =====================================*/

/* 最后一页 / 从 MAZE 页点 SNAKE 进入。

 *

 * 布局(竖屏 240x320, 进入本页时隐藏底部导航条):

 *   y=0..21    状态栏: 标题 SNAKE + 状态 + 分数 + 长度 + 上限

 *   y=26..245  20x20 棋盘 (每格 11px, 220x220)

 *   y=250..271 第一排: AUTO / LIMIT / PAUSE / NEW / BACK

 *   y=275..296 方向键: 上

 *   y=298..319 方向键: 左 / 下 / 右

 *

 * 触摸: 在棋盘上滑动 = 转向; 也可以点方向键。

 * 按键: WK_UP=上, KEY0=左, KEY1=下, KEY2=右 (app_snake.c 里轮询)。

 * 自动: AUTO 从新开一局开始, 达到 M(上限) 后自动结束。

 *

 * 画法:

 *   - 换页/开局/暂停/结束才整页重画;

 *   - 正常走一步只重画 app_snake.c 给出的 2~4 个变化格子;

 *   - 分数/长度变化时只重画顶部 HUD。 */

#define SK_BX             10

#define SK_BY             26

#define SK_CELL           SNAKE_CELL_PX

#define SK_BOARD_W        (SNAKE_GRID_W * SNAKE_CELL_PX)

#define SK_BOARD_H        (SNAKE_GRID_H * SNAKE_CELL_PX)



#define SK_ROW1_Y         250

#define SK_ROW2_Y         275

#define SK_ROW3_Y         298

#define SK_BTN_H          22

#define SK_UTIL_W         42

#define SK_UTIL_GAP       4

#define SK_UTIL_X0        4

#define SK_ARROW_W        52



#define SK_BG_A           0x0000

#define SK_BG_B           0x0841

#define SK_BODY           0x07E0

#define SK_HEAD           0x07FF

#define SK_FOOD           0xF800

#define SK_FOOD_HI        0xFD20



/* 第一排: 5 个 42x22 按钮, 间距 4 */

static const UI_Rect s_sk_auto  = { SK_UTIL_X0 + (SK_UTIL_W + SK_UTIL_GAP) * 0, SK_ROW1_Y, SK_UTIL_W, SK_BTN_H };

static const UI_Rect s_sk_limit = { SK_UTIL_X0 + (SK_UTIL_W + SK_UTIL_GAP) * 1, SK_ROW1_Y, SK_UTIL_W, SK_BTN_H };

static const UI_Rect s_sk_pause = { SK_UTIL_X0 + (SK_UTIL_W + SK_UTIL_GAP) * 2, SK_ROW1_Y, SK_UTIL_W, SK_BTN_H };

static const UI_Rect s_sk_new   = { SK_UTIL_X0 + (SK_UTIL_W + SK_UTIL_GAP) * 3, SK_ROW1_Y, SK_UTIL_W, SK_BTN_H };

static const UI_Rect s_sk_back  = { SK_UTIL_X0 + (SK_UTIL_W + SK_UTIL_GAP) * 4, SK_ROW1_Y, SK_UTIL_W, SK_BTN_H };



/* 方向键按键盘布局: 上在中间, 下面左/下/右 */

static const UI_Rect s_sk_up    = {  94, SK_ROW2_Y, SK_ARROW_W, SK_BTN_H };

static const UI_Rect s_sk_left  = {  36, SK_ROW3_Y, SK_ARROW_W, SK_BTN_H };

static const UI_Rect s_sk_down  = {  94, SK_ROW3_Y, SK_ARROW_W, SK_BTN_H };

static const UI_Rect s_sk_right = { 152, SK_ROW3_Y, SK_ARROW_W, SK_BTN_H };

static const UI_Rect s_sk_board = { SK_BX, SK_BY, SK_BOARD_W, SK_BOARD_H };



static u8 s_sk_full = 1;          /* 1=下一帧整页重画 */

static u8 s_sk_swipe_done = 0;    /* 一次滑动只转一次向 */

static u8 s_sk_limit_resume = 0;  /* 打开 LIMIT 键盘前正在跑, 关闭后恢复 */



/*------------------------------ 画格子 ------------------------------------*/

static u16 Sk_CellOf(u8 x, u8 y)

{

    return (u16)((u16)y * SNAKE_GRID_W + x);

}



static u16 Sk_CellBg(u8 cx, u8 cy)

{

    return (u16)(((cx + cy) & 1) ? SK_BG_B : SK_BG_A);

}



static void Sk_CellPos(u16 cell, u16 *x, u16 *y)

{

    u8 cx = (u8)(cell % SNAKE_GRID_W);

    u8 cy = (u8)(cell / SNAKE_GRID_W);



    *x = (u16)(SK_BX + (u16)cx * SK_CELL);

    *y = (u16)(SK_BY + (u16)cy * SK_CELL);

}



/* 在格子左上角 (x,y) 画蛇身/蛇头 */

static void Sk_DrawBody(u16 x, u16 y, u8 is_head)

{

    UI_Fill((u16)(x + 1), (u16)(y + 1),

            (u16)(x + SK_CELL - 2), (u16)(y + SK_CELL - 2),

            is_head ? SK_HEAD : SK_BODY);



    if (is_head)

    {

        /* 两个小眼睛, 让蛇头有方向感 */

        UI_Fill((u16)(x + 3), (u16)(y + 3), (u16)(x + 3), (u16)(y + 4), UI_COLOR_BG);

        UI_Fill((u16)(x + 6), (u16)(y + 3), (u16)(x + 6), (u16)(y + 4), UI_COLOR_BG);

    }

}



static void Sk_DrawFood(u16 x, u16 y)

{

    UI_Fill((u16)(x + 2), (u16)(y + 2),

            (u16)(x + SK_CELL - 3), (u16)(y + SK_CELL - 3), SK_FOOD);

    UI_Fill((u16)(x + 4), (u16)(y + 4), (u16)(x + 6), (u16)(y + 6), SK_FOOD_HI);

}



/* cell 是否在蛇身上; 是的话 *is_head 标记蛇头 */

static u8 Sk_BodyIndex(u16 cell, u8 *is_head)

{

    u16 i;



    for (i = 0; i < g_snake.len; i++)

    {

        if (Sk_CellOf(g_snake.body_x[i], g_snake.body_y[i]) == cell)

        {

            if (is_head != 0) *is_head = (u8)(i == 0);

            return 1;

        }

    }

    return 0;

}



/* 重画一个格子(先铺回棋盘底色, 再按当前状态画食物/蛇身) */

static void Sk_DrawCell(u16 cell)

{

    u16 x;

    u16 y;

    u8  cx;

    u8  cy;

    u8  is_head = 0;



    if (cell >= (u16)(SNAKE_GRID_W * SNAKE_GRID_H)) return;



    cx = (u8)(cell % SNAKE_GRID_W);

    cy = (u8)(cell / SNAKE_GRID_W);

    Sk_CellPos(cell, &x, &y);



    UI_Fill(x, y, (u16)(x + SK_CELL - 1), (u16)(y + SK_CELL - 1),

            Sk_CellBg(cx, cy));



    if (cell == Sk_CellOf(g_snake.food_x, g_snake.food_y))

    {

        Sk_DrawFood(x, y);

        return;

    }



    if (Sk_BodyIndex(cell, &is_head))

    {

        Sk_DrawBody(x, y, is_head);

    }

}



/* 整块棋盘: 棋盘格 + 所有蛇身 + 食物 */

static void Sk_DrawBoardFull(void)

{

    u16 x;

    u16 y;

    u16 i;

    u8  cx;

    u8  cy;



    /* 边框画在棋盘外侧, 不占格子 */

    UI_FrameRound((u16)(SK_BX - 1), (u16)(SK_BY - 1),

                  (u16)(SK_BX + SK_BOARD_W), (u16)(SK_BY + SK_BOARD_H),

                  8, UI_COLOR_BORDER);



    for (cy = 0; cy < SNAKE_GRID_H; cy++)

    {

        for (cx = 0; cx < SNAKE_GRID_W; cx++)

        {

            x = (u16)(SK_BX + (u16)cx * SK_CELL);

            y = (u16)(SK_BY + (u16)cy * SK_CELL);

            UI_Fill(x, y, (u16)(x + SK_CELL - 1), (u16)(y + SK_CELL - 1),

                    Sk_CellBg(cx, cy));

        }

    }



    for (i = 0; i < g_snake.len; i++)

    {

        Sk_CellPos(Sk_CellOf(g_snake.body_x[i], g_snake.body_y[i]), &x, &y);

        Sk_DrawBody(x, y, (u8)(i == 0));

    }



    Sk_CellPos(Sk_CellOf(g_snake.food_x, g_snake.food_y), &x, &y);

    Sk_DrawFood(x, y);

}



/*------------------------------ HUD ---------------------------------------*/

static const char *Sk_StateStr(void)

{

    if (g_snake.auto_mode && g_snake.state == SNAKE_RUNNING) return "自动";



    switch (g_snake.state)

    {

        case SNAKE_RUNNING: return "运行";

        case SNAKE_PAUSED:  return "已暂停";

        case SNAKE_OVER:    return "结束";

        case SNAKE_WIN:     return "胜利";

        default:            return "准备";

    }

}



static u16 Sk_StateColor(void)

{

    switch (g_snake.state)

    {

        case SNAKE_RUNNING: return g_snake.auto_mode ? UI_COLOR_OK : UI_COLOR_VALUE;

        case SNAKE_PAUSED:  return UI_COLOR_EDIT;

        case SNAKE_OVER:    return UI_COLOR_ERR;

        case SNAKE_WIN:     return UI_COLOR_OK;

        default:            return UI_COLOR_MUTED;

    }

}



static void Sk_DrawHud(void)

{

    /* 先铺掉旧状态文字, 防止 "已暂停" -> "运行" 留下尾巴 */

    UI_Fill(52, 2, 238, (u16)(UI_STATUS_H - 3), UI_COLOR_PANEL);



    UI_TextColor(52, 4, 70, 14, UI_FONT_S,

                 Sk_StateColor(), UI_COLOR_PANEL, Sk_StateStr());



    UI_TextColor(128, 4, 10, 14, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_PANEL, "S:");

    UI_NumColor(140, 3, (u32)g_snake.score, 3, UI_FONT_S,

                UI_COLOR_VALUE, UI_COLOR_PANEL);



    UI_TextColor(166, 4, 10, 14, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_PANEL, "L:");

    UI_NumColor(178, 3, (u32)g_snake.len, 3, UI_FONT_S,

                UI_COLOR_VALUE, UI_COLOR_PANEL);



    UI_TextColor(204, 4, 10, 14, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_PANEL, "M:");

    UI_NumColor(216, 3, (u32)SNAKE_GetScoreLimit(), 3, UI_FONT_S,

                UI_COLOR_VALUE, UI_COLOR_PANEL);

}



/*------------------------------ 按键 ---------------------------------------*/

static void Sk_DrawButtons(void)

{

    UI_DrawButton(&s_sk_auto, "自动", (u8)(g_snake.auto_mode ? 1 : 0));

    UI_DrawButton(&s_sk_limit, "限分", 0);

    UI_DrawButton(&s_sk_pause,

                  (g_snake.state == SNAKE_PAUSED) ? "运行" : "暂停", 0);

    UI_DrawButton(&s_sk_new, "新局", 0);

    UI_DrawButton(&s_sk_back, "返回", 0);



    UI_DrawButton(&s_sk_up, "上", 0);

    UI_DrawButton(&s_sk_left, "左", 0);

    UI_DrawButton(&s_sk_down, "下", 0);

    UI_DrawButton(&s_sk_right, "右", 0);

}



/* LIMIT 数字键盘确认: 1..400 */

static void Sk_LimitConfirm(int32_t v)

{

    if (v < 1) v = 1;

    if (v > SNAKE_MAX_LEN) v = SNAKE_MAX_LEN;



    SNAKE_SetScoreLimit((u16)v);

    s_sk_full = 1;

    UI_MarkDirty();

}



/*------------------------------ 覆盖层 -------------------------------------*/

static void Sk_TextCenter(u16 y, u8 font, u16 color, const char *s)

{

    u16 w;

    u16 x;



    w = Ui_TextWidthPx(s, font);

    x = (u16)((UI_SCREEN_W - w) / 2);



    UI_TextColor(x, y, (u16)(w + 2), font, font, color, UI_COLOR_PANEL, s);

}



static void Sk_DrawOverlay(void)

{

    const char *title;

    const char *line2;

    const char *line3;

    u16 color;



    if (g_snake.state == SNAKE_PAUSED)

    {

        title = "已暂停";

        line2 = "暂停/继续";

        line3 = g_snake.auto_mode ? "自动模式" : "滑动或方向键";

        color = UI_COLOR_EDIT;

    }

    else if (g_snake.state == SNAKE_OVER)

    {

        title = "游戏结束";

        line2 = "按新局重开";

        line3 = "自动演示";

        color = UI_COLOR_ERR;

    }

    else if (g_snake.state == SNAKE_WIN)

    {

        title = "胜利";

        line2 = (SNAKE_GetScoreLimit() < SNAKE_MAX_LEN)

                    ? "达到分数上限"

                    : "棋盘已满";

        line3 = "新局重玩";

        color = UI_COLOR_OK;

    }

    else

    {

        return;

    }



    UI_FillRound(28, 106, 211, 206, 12, UI_COLOR_PANEL);

    UI_FrameRound(28, 106, 211, 206, 12, UI_COLOR_ACCENT);



    Sk_TextCenter(118, UI_FONT_L, color, title);

    Sk_TextCenter(152, UI_FONT_S, UI_COLOR_TEXT, line2);

    Sk_TextCenter(174, UI_FONT_S, UI_COLOR_MUTED, line3);

}



/*------------------------------ 整页/增量绘制 ------------------------------*/

static void Snake_DrawFull(void)

{

    /* 新版 Page_Begin 只在 g_ui_full_redraw=1 时清整屏, 这里主动要求清 */

    g_ui_full_redraw = 1;

    Page_Begin("贪吃蛇");



    Sk_DrawHud();

    Sk_DrawBoardFull();

    Sk_DrawButtons();

    Sk_DrawOverlay();



    SNAKE_ClearDirty();

    s_sk_full = 0;

}



static void Snake_Enter(void)

{

    UI_NavSetVisible(0);        /* 全屏游戏: 不要底部 BACK/NEXT */

    s_sk_swipe_done = 0;

    s_sk_limit_resume = 0;



    /* 从别的页面回来时先暂停; 第一次进来(IDLE)直接开一局 */

    if (g_snake.state == SNAKE_RUNNING)

    {

        SNAKE_TogglePause();

    }

    if (g_snake.state == SNAKE_IDLE)

    {

        SNAKE_Start();

    }



    s_sk_full = 1;

    SNAKE_ClearDirty();

    UI_MarkDirty();

}



static void Snake_Leave(void)

{

    /* 离开页面就暂停, 回来时不会发现蛇已经撞死了 */

    if (g_snake.state == SNAKE_RUNNING)

    {

        SNAKE_TogglePause();

    }

    s_sk_limit_resume = 0;

    s_sk_full = 1;

}



static void Snake_Draw(void)

{

    if (g_ui_full_redraw)

    {

        g_ui_full_redraw = 0;

        s_sk_full = 1;

    }



    if (s_sk_full)

    {

        Snake_DrawFull();

        return;

    }



    /* 普通脏标记(按钮/状态变化)只重画 HUD 和按键, 不碰棋盘 */

    Sk_DrawHud();

    Sk_DrawButtons();

}



static void Snake_Event(const UI_Event *ev)

{

    int dx;

    int dy;



    if (ev->type == UI_EV_UP)

    {

        s_sk_swipe_done = 0;

        return;

    }



    /* 滑动转向: 从按下点算位移, 一次手势只转一次 */

    if (ev->type == UI_EV_MOVE)

    {

        if (s_sk_swipe_done) return;

        if (g_snake.auto_mode || g_snake.state != SNAKE_RUNNING) return;



        /* 只有从棋盘上开始的滑动才算转向, 避免手指从按键拖到棋盘误触 */

        if (!UI_HitTest(&s_sk_board, ev->down_x, ev->down_y)) return;



        dx = (int)ev->x - (int)ev->down_x;

        dy = (int)ev->y - (int)ev->down_y;

        if (dx < 0) dx = -dx;

        if (dy < 0) dy = -dy;



        if (dx < 12 && dy < 12) return;



        if (dx > dy)

        {

            SNAKE_SetDir(((int)ev->x >= (int)ev->down_x) ? SNAKE_RIGHT : SNAKE_LEFT);

        }

        else

        {

            SNAKE_SetDir(((int)ev->y >= (int)ev->down_y) ? SNAKE_DOWN : SNAKE_UP);

        }

        s_sk_swipe_done = 1;

        return;

    }



    if (ev->type != UI_EV_DOWN) return;



    if (Meter_HitPad(&s_sk_back, ev->x, ev->y, 2, 2))

    {

        UI_GoRoot(PAGE_PWM);         /* 清历史回主界面, 再按 BACK 才回开始界面 */

        return;

    }



    if (Meter_HitPad(&s_sk_limit, ev->x, ev->y, 2, 2))

    {

        /* 设置上限时暂停, 键盘关闭后自动恢复(如果原来在跑) */

        s_sk_limit_resume = (g_snake.state == SNAKE_RUNNING) ? 1 : 0;

        if (s_sk_limit_resume)

        {

            SNAKE_TogglePause();

        }

        UI_Keypad_Open(4, (int32_t)SNAKE_GetScoreLimit(), 3, Sk_LimitConfirm);

        return;

    }



    if (Meter_HitPad(&s_sk_auto, ev->x, ev->y, 2, 2))

    {

        /* 自动进行中/暂停时按 AUTO = 关掉自动, 保留当前局面;

         * 手动局面或已经 OVER/WIN 时按 AUTO = 从新一局开始自动。 */

        if (g_snake.auto_mode &&

            g_snake.state != SNAKE_WIN && g_snake.state != SNAKE_OVER)

        {

            SNAKE_SetAuto(0);

        }

        else

        {

            SNAKE_StartAuto();       /* 自动模式从新一局开始(蛇身对齐回路) */

        }

        s_sk_full = 1;

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_sk_pause, ev->x, ev->y, 2, 2))

    {

        if (g_snake.state == SNAKE_RUNNING || g_snake.state == SNAKE_PAUSED)

        {

            SNAKE_TogglePause();

        }

        s_sk_full = 1;

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_sk_new, ev->x, ev->y, 2, 2))

    {

        if (g_snake.auto_mode) SNAKE_StartAuto();

        else                   SNAKE_Start();

        s_sk_full = 1;

        UI_MarkDirty();

        return;

    }



    if (Meter_HitPad(&s_sk_up, ev->x, ev->y, 2, 2))

    {

        SNAKE_SetDir(SNAKE_UP);

        return;

    }

    if (Meter_HitPad(&s_sk_left, ev->x, ev->y, 2, 2))

    {

        SNAKE_SetDir(SNAKE_LEFT);

        return;

    }

    if (Meter_HitPad(&s_sk_down, ev->x, ev->y, 2, 2))

    {

        SNAKE_SetDir(SNAKE_DOWN);

        return;

    }

    if (Meter_HitPad(&s_sk_right, ev->x, ev->y, 2, 2))

    {

        SNAKE_SetDir(SNAKE_RIGHT);

        return;

    }



    /* 按在棋盘上: 准备识别滑动 */

    if (UI_HitTest(&s_sk_board, ev->x, ev->y))

    {

        s_sk_swipe_done = 0;

    }

}



static void Snake_Tick(u32 now_ms)

{

    u8  st0;

    u32 sc0;

    u16 len0;

    u16 cell;



    /* LIMIT 键盘关闭后, 恢复打开键盘前的运行状态 */

    if (s_sk_limit_resume && !g_keypad.active)

    {

        s_sk_limit_resume = 0;

        if (g_snake.state == SNAKE_PAUSED)

        {

            SNAKE_TogglePause();

            s_sk_full = 1;

            UI_MarkDirty();

        }

    }



    st0  = g_snake.state;

    sc0  = g_snake.score;

    len0 = g_snake.len;



    SNAKE_Tick(now_ms);



    if (g_snake.state != st0)

    {

        if (g_snake.state == SNAKE_OVER || g_snake.state == SNAKE_WIN)

        {

            s_sk_full = 1;

            SNAKE_ClearDirty();

            UI_MarkDirty();

            return;

        }

    }



    /* 整页重画还没执行: 这些脏格子不用画, 整页会覆盖 */

    if (s_sk_full)

    {

        SNAKE_ClearDirty();

        return;

    }



    /* 正常走一步: 只重画变化过的格子(2~4 个) */

    while (SNAKE_PopDirtyCell(&cell))

    {

        Sk_DrawCell(cell);

    }



    /* 分数/长度只在吃到食物时变, 重画顶部 HUD 即可 */

    if (g_snake.score != sc0 || g_snake.len != len0)

    {

        Sk_DrawHud();

    }

}



/*============================== 页面表 ====================================*/

const UI_PageOps g_ui_pages[PAGE_COUNT] =

{

    /* enter        leave        draw            on_event        tick */

    { Pwm_Enter,    0,           Pwm_Draw,       Pwm_Event,      Pwm_Tick   }, /* PWM/METER */

    { 0,            0,           Cap_Draw,       Cap_Event,      Cap_Tick   }, /* CAPTURE */

    { Stopwatch_Enter, 0,         Stopwatch_Draw, Stopwatch_Event, Stopwatch_Tick }, /* TIMER */

    { 0,            0,           Sweep_Draw,     Sweep_Event,    Sweep_Tick }, /* SWEEP */

    { 0,            0,           Spwm_Draw,      Spwm_Event,     0          }, /* SPWM */

    { Mz_Enter,     Mz_Leave,    Mz_Draw,        Mz_Event,       Mz_Tick    }, /* MAZE */

    { Snake_Enter,   Snake_Leave,  Snake_Draw,    Snake_Event,    Snake_Tick }  /* SNAKE */

};

