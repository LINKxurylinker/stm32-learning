/*
 * ui.c - touch UI for the STM32F407 ADC oscilloscope (question 3)
 *
 * Visual language borrowed from the question-2 PWM page:
 *   dark background, rounded panels, muted labels + cyan/orange values,
 *   orange status-bar accent line.
 * Menu page layout borrowed from KEYctrl: 3x2 grid of mode cards plus a
 * bottom slider panel, active card highlighted in green with a yellow rim.
 *
 * Pages:
 *   PAGE_SCOPE : status bar + 10x8 grid + waveform + 4 parameter cards + nav
 *   PAGE_MENU  : 6 function cards (run / single / mode / timebase / vdiv / edge)
 *                + trigger-level slider (+/- and drag) + calibration
 *
 * The acquisition core stays in adc.c/cmd.c; this file only reads the
 * display frame that cmd.c has already gated by the trigger mode.
 */

#include "ui.h"
#include "lcd.h"
#include "touch.h"
#include "24cxx.h"
#include "adc.h"
#include "cmd.h"
#include "measure.h"
#include "delay.h"
#include "ui_cn.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* palette (same warmth as the Q2 PWM page)                           */
/* ------------------------------------------------------------------ */
#define C_BG        0x28C1      /* dark wooden background             */
#define C_PANEL     0x18E3      /* card / bar fill                    */
#define C_PANEL_HI  0x2D6B      /* highlighted card fill              */
#define C_BORDER    0x5ACB      /* 1px border                         */
#define C_TEXT      0xF7BE      /* main text                          */
#define C_MUTED     0x8410      /* labels                             */
#define C_VALUE     0x07FF      /* cyan: live value / waveform        */
#define C_EDIT      0xFFE0      /* yellow: cursor / editing           */
#define C_OK        0x07E0      /* green: running / active            */
#define C_ERR       0xF800      /* red: stopped / error               */
#define C_ACCENT    0xFD20      /* orange: accents, trigger line      */
#define C_GRID      0x3186      /* grid lines                         */
#define C_GRID_MID  0x4A69      /* grid centre lines                  */

#define SCR_W       320
#define SCR_H       240

/* scope page geometry */
#define TOP_H       24
#define PAN_X0      6
#define PAN_Y0      27
#define PAN_X1      313
#define PAN_Y1      170
#define PLOT_X0     42
#define PLOT_X1     278
#define PLOT_Y0     33
#define PLOT_Y1     143
#define PLOT_W      (PLOT_X1 - PLOT_X0 + 1)
#define PLOT_H      (PLOT_Y1 - PLOT_Y0 + 1)

#define SCOPE_PAGE  0
#define MENU_PAGE   1
#define THD_PAGE    2

typedef struct
{
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} ui_rect;

/* navigation buttons on the scope page */
static const ui_rect R_NAV_RUN    = {  6, 212, 58, 26};
static const ui_rect R_NAV_SINGLE = { 68, 212, 58, 26};
static const ui_rect R_NAV_EDGE   = {130, 212, 58, 26};
static const ui_rect R_CUR_X      = {130, 212, 29, 26};
static const ui_rect R_CUR_Y      = {159, 212, 29, 26};
static const ui_rect R_NAV_HOLD   = {192, 212, 58, 26};
static const ui_rect R_NAV_MENU   = {254, 212, 60, 26};

/* scope top bar badges */
static const ui_rect R_TOP_RUN    = {202,  3, 50, 18};
static const ui_rect R_TOP_MODE   = {256,  3, 58, 18};
static const ui_rect R_AUTO_RANGE = {150,  3, 46, 18};

/* menu page buttons */
static const ui_rect R_BTN_CAL    = {190,  3, 58, 19};
static const ui_rect R_BTN_BACK   = {252,  3, 62, 19};
static const ui_rect R_BTN_THD    = {126,  3, 58, 19};
static const ui_rect R_TILE[6] =
{
    {  6,  50,  98, 64},
    {110,  50,  98, 64},
    {214,  50,  98, 64},
    {  6, 120,  98, 64},
    {110, 120,  98, 64},
    {214, 120,  98, 64}
};
static const ui_rect R_SLIDER    = {6, 190, 308, 46};
static const ui_rect R_AUTO      = {196, 192, 56, 18};
static const ui_rect R_TRIG_MINUS = {252, 206, 24, 26};
static const ui_rect R_TRIG_PLUS  = {280, 206, 24, 26};

#define SLIDER_X0   92
#define SLIDER_X1   244
#define SLIDER_Y    219

/* Below this the input is treated as no AC waveform (DC/empty input). */
#define UI_SIG_VPP_MIN   0.04f
#define UI_SIG_FREQ_MIN  20.0f

/* ------------------------------------------------------------------ */
/* state                                                              */
/* ------------------------------------------------------------------ */
static u8  s_page = SCOPE_PAGE;
static u8  s_frozen = 0;
static u32 s_last_seq = 0;
static u32 s_scope_sig = 0xFFFFFFFFU;
static u8  s_touch_down = 0;
static u8  s_slider_drag = 0;
static u16 s_cursor_a = 0;
static u16 s_cursor_b = 0;
static u16 s_cursor_ya = 0;
static u16 s_cursor_yb = 0;
static u8  s_cursor_drag = 0;
static u8  s_cursor_active = 0;
static u8  s_cursor_mode = 0;      /* 0 = X (time), 1 = Y (voltage) */
static s16 s_cursor_grab_x = 0;
static s16 s_cursor_grab_y = 0;
static u8  s_cursor_touch_freeze = 0;

static char s_ch_line1[2][24] = {"", ""};
static char s_ch_line2[2][32] = {"", ""};
static u8   s_ch_en_cache[2] = {0xFFU, 0xFFU};

/* Dynamic wave refresh: remember the previous trace so a new frame can
 * erase only the old pixels and redraw the grid under them, instead of
 * filling the whole panel every acquisition frame. */
static u16 s_trace_y0[2][PLOT_W];
static u16 s_trace_y1[2][PLOT_W];
static u16 s_trace_px[2][PLOT_W];
static u16 s_trace_py[2][PLOT_W];
static u16 s_trace_count[2] = {0U, 0U};
static u8  s_trace_mode[2]  = {0U, 0U}; /* 0 none, 1 column, 2 polyline */
static u16 s_old_trig_y = 0;
static u8  s_old_trig_valid = 0;
static u16 s_zero_y = 0;
static u8  s_zero_valid = 0;
static u8  s_no_sig = 0xFFU;
static u32 s_axis_tb = 0;
static u16 s_axis_vdiv[2] = {0U, 0U};
static u16 s_axis_vcenter[2] = {0U, 0U};
static u8  s_axis_valid = 0;

static u32 s_param_sig = 0xFFFFFFFFU;
static u32 s_thd_seq = 0xFFFFFFFFU;
static u8  s_thd_div = 0U;
static u16 s_thd_bar_top[5] = {189U, 189U, 189U, 189U, 189U};
static u8  s_thd_ok_prev = 0U;

/* ------------------------------------------------------------------ */
/* small graphics helpers                                             */
/* ------------------------------------------------------------------ */
static u16 ui_isqrt(u16 v)
{
    u16 r = 0;

    while ((u32)(r + 1U) * (u32)(r + 1U) <= (u32)v) r++;
    return r;
}

static void ui_fill(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
    u16 t;

    if (x1 > x2) { t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; }
    if (x1 >= SCR_W || y1 >= SCR_H) return;
    if (x2 >= SCR_W) x2 = SCR_W - 1;
    if (y2 >= SCR_H) y2 = SCR_H - 1;

    LCD_Fill(x1, y1, x2, y2, color);
}

static void ui_frame(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
    if (x1 >= SCR_W || y1 >= SCR_H) return;
    if (x2 >= SCR_W) x2 = SCR_W - 1;
    if (y2 >= SCR_H) y2 = SCR_H - 1;

    POINT_COLOR = color;
    LCD_DrawRectangle(x1, y1, x2, y2);
}

static void ui_fill_round(u16 x1, u16 y1, u16 x2, u16 y2, u8 radius, u16 color)
{
    u16 t;
    u16 dy;
    u16 half;
    u16 inset;
    u8  i;

    if (x1 > x2) { t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; }
    if (x2 >= SCR_W) x2 = SCR_W - 1;
    if (y2 >= SCR_H) y2 = SCR_H - 1;
    if (x1 > x2 || y1 > y2) return;

    if (radius > (u8)((x2 - x1 + 1) / 2)) radius = (u8)((x2 - x1 + 1) / 2);
    if (radius > (u8)((y2 - y1 + 1) / 2)) radius = (u8)((y2 - y1 + 1) / 2);

    if (radius == 0)
    {
        ui_fill(x1, y1, x2, y2, color);
        return;
    }

    ui_fill(x1, (u16)(y1 + radius), x2, (u16)(y2 - radius), color);

    for (i = 0; i < radius; i++)
    {
        dy = (u16)(radius - i);
        half = ui_isqrt((u16)((u16)radius * radius - (u16)(dy * dy)));
        inset = (u16)(radius - half);

        ui_fill((u16)(x1 + inset), (u16)(y1 + i),
                (u16)(x2 - inset), (u16)(y1 + i), color);
        ui_fill((u16)(x1 + inset), (u16)(y2 - i),
                (u16)(x2 - inset), (u16)(y2 - i), color);
    }
}

static void ui_frame_round(u16 x1, u16 y1, u16 x2, u16 y2, u8 radius, u16 color)
{
    u16 t;
    u16 dy;
    u16 half;
    u16 inset;
    u8  i;

    if (x1 > x2) { t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; }
    if (x2 >= SCR_W) x2 = SCR_W - 1;
    if (y2 >= SCR_H) y2 = SCR_H - 1;
    if (x1 > x2 || y1 > y2) return;

    if (radius > (u8)((x2 - x1 + 1) / 2)) radius = (u8)((x2 - x1 + 1) / 2);
    if (radius > (u8)((y2 - y1 + 1) / 2)) radius = (u8)((y2 - y1 + 1) / 2);

    if (radius == 0)
    {
        ui_frame(x1, y1, x2, y2, color);
        return;
    }

    ui_fill((u16)(x1 + radius), y1, (u16)(x2 - radius), y1, color);
    ui_fill((u16)(x1 + radius), y2, (u16)(x2 - radius), y2, color);
    ui_fill(x1, (u16)(y1 + radius), x1, (u16)(y2 - radius), color);
    ui_fill(x2, (u16)(y1 + radius), x2, (u16)(y2 - radius), color);

    for (i = 0; i < radius; i++)
    {
        dy = (u16)(radius - i);
        half = ui_isqrt((u16)((u16)radius * radius - (u16)(dy * dy)));
        inset = (u16)(radius - half);

        ui_fill((u16)(x1 + inset), (u16)(y1 + i),
                (u16)(x1 + inset), (u16)(y1 + i), color);
        ui_fill((u16)(x2 - inset), (u16)(y1 + i),
                (u16)(x2 - inset), (u16)(y1 + i), color);
        ui_fill((u16)(x1 + inset), (u16)(y2 - i),
                (u16)(x1 + inset), (u16)(y2 - i), color);
        ui_fill((u16)(x2 - inset), (u16)(y2 - i),
                (u16)(x2 - inset), (u16)(y2 - i), color);
    }
}

static u16 ui_str_w(const char *s, u8 size)
{
    return (u16)(strlen(s) * (size / 2U));
}

static void ui_text(u16 x, u16 y, u16 w, u16 h, u8 size,
                    u16 fg, u16 bg, const char *s)
{
    POINT_COLOR = fg;
    BACK_COLOR  = bg;
    LCD_ShowString(x, y, w, h, size, (u8 *)s);
}

static void ui_text_c(u16 x, u16 y, u16 w, u16 h, u8 size,
                      u16 fg, u16 bg, const char *s)
{
    u16 tw = ui_str_w(s, size);
    u16 tx = x;

    if (tw < w) tx = (u16)(x + (w - tw) / 2U);
    ui_text(tx, y, (u16)(w - (tx - x)), h, size, fg, bg, s);
}

static u8 ui_hit(const ui_rect *r, u16 x, u16 y)
{
    if (r == 0) return 0;
    if (x < r->x || x >= (u16)(r->x + r->w)) return 0;
    if (y < r->y || y >= (u16)(r->y + r->h)) return 0;
    return 1;
}

/* 16px mixed Chinese/ASCII text, borrowed from the Q2 ui_hanzi module */
static void ui_cn(u16 x, u16 y, const char *s, u16 fg, u16 bg)
{
    ui_hz16_text(x, y, s, fg, bg);
}

static void ui_cn_c(u16 x, u16 y, u16 w, const char *s, u16 fg, u16 bg)
{
    u16 tw = ui_hz16_width(s);
    u16 tx = x;

    if (tw < w) tx = (u16)(x + (w - tw) / 2U);
    ui_hz16_text(tx, y, s, fg, bg);
}

static void ui_cn_button(const ui_rect *r, const char *label,
                         u16 fill, u16 border, u16 fg)
{
    u8 rad = (u8)(r->h / 3U);

    if (rad < 4) rad = 4;
    if (rad > 8) rad = 8;

    ui_fill_round(r->x, r->y, (u16)(r->x + r->w - 1),
                  (u16)(r->y + r->h - 1), rad, fill);
    ui_frame_round(r->x, r->y, (u16)(r->x + r->w - 1),
                   (u16)(r->y + r->h - 1), rad, border);
    ui_cn_c(r->x, (u16)(r->y + (r->h - 16U) / 2U), r->w, label, fg, fill);
}

static void ui_cn_pill(const ui_rect *r, const char *label,
                       u16 fill, u16 border, u16 fg)
{
    u8 rad = (u8)(r->h / 2U);

    ui_fill_round(r->x, r->y, (u16)(r->x + r->w - 1),
                  (u16)(r->y + r->h - 1), rad, fill);
    ui_frame_round(r->x, r->y, (u16)(r->x + r->w - 1),
                   (u16)(r->y + r->h - 1), rad, border);
    ui_cn_c(r->x, (u16)(r->y + (r->h - 16U) / 2U), r->w, label, fg, fill);
}

/* ------------------------------------------------------------------ */
/* text / value formatting                                            */
/* ------------------------------------------------------------------ */
static const char *ui_mode_name(u8 mode)
{
    if (mode == CMD_MODE_NORM)   return "NORM";
    if (mode == CMD_MODE_SINGLE) return "SINGLE";
    return "AUTO";
}

static const char *ui_mode_cn(u8 mode)
{
    if (mode == CMD_MODE_NORM)   return CN_NORM;
    if (mode == CMD_MODE_SINGLE) return CN_SINGLE;
    return CN_AUTO;
}

static const char *ui_type_short(u8 type)
{
    switch (type)
    {
        case 1:  return CN_SINE;
        case 2:  return CN_SQUARE;
        case 3:  return CN_DC;
        case 4:  return CN_TRI;
        default: return CN_UNKNOWN;
    }
}

static void ui_fmt_tb(u32 us, char *buf)
{
    if (us >= 1000U) sprintf(buf, "%.2fms", (double)us / 1000.0);
    else             sprintf(buf, "%luus", (unsigned long)us);
}

static void ui_fmt_vd(u16 mv, char *buf)
{
    /* Auto range can select 20/25/125 mV per division.  Two decimals
     * would display 0.125 V as 0.12/0.13 V, so keep three decimals for
     * non-multiples of 10 mV. */
    if ((mv % 1000U) == 0U)
        sprintf(buf, "%.0fV", (double)mv / 1000.0);
    else if ((mv % 10U) == 0U)
        sprintf(buf, "%.2fV", (double)mv / 1000.0);
    else
        sprintf(buf, "%.3fV", (double)mv / 1000.0);
}

/* ------------------------------------------------------------------ */
/* trigger helpers                                                    */
/* ------------------------------------------------------------------ */
static u16 ui_frame_min(const u16 *f)
{
    u16 i;
    u16 v = 4095;

    for (i = 0; i < ADC_FRAME_LEN; i++) if (f[i] < v) v = f[i];
    return v;
}

static u16 ui_frame_max(const u16 *f)
{
    u16 i;
    u16 v = 0;

    for (i = 0; i < ADC_FRAME_LEN; i++) if (f[i] > v) v = f[i];
    return v;
}

static u16 ui_trig_raw(void)
{
    u16 mn;
    u16 mx;
    const u16 *f;

    if (CMD_GetTrigManual()) return CMD_GetTrigLevel();
    if (CMD_DisplaySeq() == 0U) return 2048U;

    f = CMD_GetChannelEnabled(0) ? CMD_DisplayFrame()
                                 : CMD_DisplayFrame2();
    mn = ui_frame_min(f);
    mx = ui_frame_max(f);
    return (u16)((mn + mx) / 2U);
}

static u16 ui_volt_to_y(float volt, float vdiv, float center)
{
    float ppd = (float)PLOT_H / 8.0f;
    float y = (float)((PLOT_Y0 + PLOT_Y1) / 2) - (volt - center) * ppd / vdiv;

    if (y < (float)PLOT_Y0) y = (float)PLOT_Y0;
    if (y > (float)PLOT_Y1) y = (float)PLOT_Y1;
    return (u16)y;
}

static u16 ui_raw_to_y(u16 raw, float vdiv, float center)
{
    return ui_volt_to_y((float)raw * 3.3f / 4095.0f, vdiv, center);
}

/* ------------------------------------------------------------------ */
/* status bar                                                         */
/* ------------------------------------------------------------------ */
static void ui_topbar(void)
{
    u8  running = CMD_IsRunning();
    u8  mode = CMD_GetMode();
    u8  auto_on = (u8)((CMD_GetAutoTimebase() || CMD_GetAutoVdiv()) ? 1 : 0);
    u8  active_ch = CMD_GetChannelEnabled(0) ? 0U : 1U;
    char tb[12];
    char vd[12];
    char info[28];

    ui_fill(0, 0, SCR_W - 1, TOP_H - 1, C_PANEL);
    ui_cn(8, 4, CN_SCOPE, C_TEXT, C_PANEL);

    /* Current timebase and vertical scale directly on the scope page.
     * Example: 500us/1.00V. */
    ui_fmt_tb(CMD_GetTimebaseUs(), tb);
    ui_fmt_vd(CMD_GetVdivMvCh(active_ch), vd);
    sprintf(info, "%s/%s", tb, vd);
    ui_text(62, 7, 86, 14, 12, C_MUTED, C_PANEL, info);

    /* RUN / STOP badge */
    ui_cn_pill(&R_TOP_RUN, running ? CN_RUN : CN_STOP,
               running ? C_OK : C_PANEL,
               running ? C_OK : C_ERR,
               running ? C_BG : C_ERR);

    /* auto range on/off (tap to toggle) */
    ui_cn_pill(&R_AUTO_RANGE, CN_AUTO,
               auto_on ? C_OK : C_PANEL_HI,
               C_BORDER,
               auto_on ? C_BG : C_TEXT);

    /* trigger mode badge */
    ui_cn_pill(&R_TOP_MODE, ui_mode_cn(mode),
               C_PANEL_HI, C_BORDER,
               (mode == CMD_MODE_SINGLE) ? C_EDIT : C_VALUE);

    ui_fill(0, TOP_H - 1, SCR_W - 1, TOP_H - 1, C_ACCENT);
}

/* Update only the TB/Vdiv text after an auto-range step.  Calling the
 * whole topbar here is unnecessary and would add another blinking strip
 * every time the scale changes. */
static void ui_topbar_info(void)
{
    u8 active_ch = CMD_GetChannelEnabled(0) ? 0U : 1U;
    char tb[12];
    char vd[12];
    char info[28];

    ui_fmt_tb(CMD_GetTimebaseUs(), tb);
    ui_fmt_vd(CMD_GetVdivMvCh(active_ch), vd);
    sprintf(info, "%s/%s", tb, vd);

    ui_fill(60, 4, 148, 21, C_PANEL);
    ui_text(62, 7, 86, 14, 12, C_MUTED, C_PANEL, info);
}

/* ------------------------------------------------------------------ */
/* scope page                                                         */
/* ------------------------------------------------------------------ */
static void ui_draw_wave_grid(void)
{
    u16 i;
    u16 x;
    u16 y;

    for (i = 0; i <= 10U; i++)
    {
        x = (u16)(PLOT_X0 + ((u32)i * (PLOT_W - 1U)) / 10U);
        ui_fill(x, PLOT_Y0, x, PLOT_Y1, (i == 5U) ? C_GRID_MID : C_GRID);
    }
    for (i = 0; i <= 8U; i++)
    {
        y = (u16)(PLOT_Y0 + ((u32)i * (PLOT_H - 1U)) / 8U);
        ui_fill(PLOT_X0, y, PLOT_X1, y, (i == 4U) ? C_GRID_MID : C_GRID);
    }
}

/* Erase only the old waveform pixels of both channels.  The grid lines
 * are redrawn afterwards, so no full-panel fill is needed. */
static void ui_erase_trace(void)
{
    u16 i;
    u8  ch;

    for (ch = 0U; ch < 2U; ch++)
    {
        if (s_trace_mode[ch] == 1U)      /* peak-detect columns */
        {
            for (i = 0U; i < PLOT_W; i++)
            {
                if (s_trace_y0[ch][i] <= s_trace_y1[ch][i])
                {
                    ui_fill((u16)(PLOT_X0 + i), s_trace_y0[ch][i],
                            (u16)(PLOT_X0 + i), s_trace_y1[ch][i], C_BG);
                }
            }
        }
        else if (s_trace_mode[ch] == 2U) /* polyline */
        {
            POINT_COLOR = C_BG;
            for (i = 1U; i < s_trace_count[ch]; i++)
            {
                LCD_DrawLine(s_trace_px[ch][i - 1U], s_trace_py[ch][i - 1U],
                             s_trace_px[ch][i], s_trace_py[ch][i]);
            }
        }
        s_trace_mode[ch] = 0U;
    }
}

static void ui_erase_trigger_line(void)
{
    u16 x;
    u16 ty;

    if (!s_old_trig_valid)
    {
        return;
    }

    for (x = PLOT_X0; x < (u16)(PLOT_X1 - 2U); x = (u16)(x + 8U))
    {
        ui_fill(x, s_old_trig_y, (u16)(x + 3U), s_old_trig_y, C_BG);
    }

    ty = (s_old_trig_y > 14U) ? (u16)(s_old_trig_y - 14U) : PLOT_Y0;
    /* The T glyph is drawn with a 12-pixel text box starting at
     * PLOT_X1-12, so its background reaches PLOT_X1.  Erase two pixels
     * into the right margin as well; otherwise the rightmost column of
     * the old T accumulates as a noisy vertical strip when the trigger
     * level moves. */
    ui_fill((u16)(PLOT_X1 - 12U), ty, (u16)(PLOT_X1 + 2U),
            (u16)(ty + 14U), C_BG);

    s_old_trig_valid = 0U;
}

/* Zero-volt reference line.  It is erased/redrawn separately from the
 * grid because the vertical centre follows the DC bias, so its Y
 * position can change without a full-panel repaint. */
static void ui_erase_zero_line(void)
{
    u16 x;

    if (!s_zero_valid) return;
    for (x = PLOT_X0; x < (u16)(PLOT_X1 - 2U); x = (u16)(x + 8U))
    {
        ui_fill(x, s_zero_y, (u16)(x + 3U), s_zero_y, C_BG);
    }
    s_zero_valid = 0U;
}

/* Axis tick labels are redrawn only when TB / Vdiv / vertical centre
 * changes, not on every acquisition frame. */
static void ui_erase_voltage_labels(void)
{
    ui_fill(7, PLOT_Y0, (u16)(PLOT_X0 - 1U), PLOT_Y1, C_BG);
    ui_fill(280, PLOT_Y0, (u16)(PAN_X1 - 1U), PLOT_Y1, C_BG);
}

static void ui_erase_time_labels(void)
{
    ui_fill((u16)(PAN_X0 + 1U), (u16)(PLOT_Y1 + 1U),
            (u16)(PAN_X1 - 1U), (u16)(PAN_Y1 - 1U), C_BG);
}

static void ui_draw_voltage_labels(u8 ch, u16 vdiv_mv, u16 vcenter_mv)
{
    u16 i;
    u16 y;
    u16 x;
    u16 fg;
    float vdiv;
    float mid;
    char buf[12];

    if (vdiv_mv < 20U) vdiv_mv = 20U;
    vdiv = (float)vdiv_mv / 1000.0f;
    mid = (float)vcenter_mv / 1000.0f;

    if (ch == 0U)
    {
        x = 7U;
        fg = C_VALUE;
    }
    else if (CMD_GetChannelEnabled(0))
    {
        x = 282U;
        fg = C_OK;
    }
    else
    {
        x = 7U;
        fg = C_OK;
    }

    for (i = 0; i <= 4U; i++)
    {
        float v = mid + 4.0f * vdiv - (float)i * 2.0f * vdiv;
        if (v >= 10.0f || v <= -10.0f) sprintf(buf, "%.1f", (double)v);
        else                           sprintf(buf, "%.2f", (double)v);
        y = (u16)(PLOT_Y0 + ((u32)i * (PLOT_H - 1U)) / 4U);
        if (y >= 6U) y -= 6U;
        ui_text(x, y, 32, 13, 12, fg, C_BG, buf);
    }
}

static void ui_draw_time_labels(u32 tb)
{
    u16 i;
    u16 x;
    u8  use_ms = (u8)((tb >= 100U) ? 1U : 0U);
    char buf[12];

    for (i = 0; i <= 5U; i++)
    {
        u32 t = (u32)i * 2U * tb;
        u16 tw;
        u16 tx;

        if (use_ms)
        {
            if (i == 5U) sprintf(buf, "%.2fms", (double)t / 1000.0);
            else         sprintf(buf, "%.2f", (double)t / 1000.0);
        }
        else
        {
            if (i == 5U) sprintf(buf, "%luus", (unsigned long)t);
            else         sprintf(buf, "%lu", (unsigned long)t);
        }

        tw = ui_str_w(buf, 12);
        x = (u16)(PLOT_X0 + ((u32)i * (PLOT_W - 1U)) / 5U);
        tx = (x > (u16)(tw / 2U)) ? (u16)(x - tw / 2U) : 2U;
        if (tx > (u16)(PAN_X1 - tw - 2U)) tx = (u16)(PAN_X1 - tw - 2U);
        ui_text(tx, 151, (u16)(tw + 2U), 14, 12, C_MUTED, C_BG, buf);
    }
}

static void ui_erase_cursor_layer(void)
{
    u16 p;
    u16 q;

    if (!s_frozen) return;

    if (s_cursor_mode == 0U)
    {
        u16 ca = s_cursor_a;
        u16 cb = s_cursor_b;

        if (ca < PLOT_X0) ca = PLOT_X0;
        if (ca > PLOT_X1) ca = PLOT_X1;
        if (cb < PLOT_X0) cb = PLOT_X0;
        if (cb > PLOT_X1) cb = PLOT_X1;

        for (p = PLOT_Y0; p < (u16)(PLOT_Y1 - 3U); p = (u16)(p + 8U))
        {
            ui_fill(ca, p, ca, (u16)(p + 3U), C_BG);
            ui_fill(cb, p, cb, (u16)(p + 3U), C_BG);
        }
        ui_fill((ca > (u16)(PLOT_X1 - 10U)) ? (u16)(PLOT_X1 - 10U) : ca,
                PLOT_Y0, (u16)((ca > (u16)(PLOT_X1 - 10U)) ?
                (u16)(PLOT_X1 - 10U) : ca) + 9U,
                (u16)(PLOT_Y0 + 15U), C_BG);
        ui_fill((cb > (u16)(PLOT_X1 - 10U)) ? (u16)(PLOT_X1 - 10U) : cb,
                PLOT_Y0, (u16)((cb > (u16)(PLOT_X1 - 10U)) ?
                (u16)(PLOT_X1 - 10U) : cb) + 9U,
                (u16)(PLOT_Y0 + 15U), C_BG);
    }
    else
    {
        u16 ya = s_cursor_ya;
        u16 yb = s_cursor_yb;

        if (ya < PLOT_Y0) ya = PLOT_Y0;
        if (ya > PLOT_Y1) ya = PLOT_Y1;
        if (yb < PLOT_Y0) yb = PLOT_Y0;
        if (yb > PLOT_Y1) yb = PLOT_Y1;

        for (p = PLOT_X0; p < (u16)(PLOT_X1 - 2U); p = (u16)(p + 8U))
        {
            ui_fill(p, ya, (u16)(p + 3U), ya, C_BG);
            ui_fill(p, yb, (u16)(p + 3U), yb, C_BG);
        }
        q = (ya > 14U) ? (u16)(ya - 14U) : ya;
        ui_fill((u16)(PLOT_X0 + 2U), q, (u16)(PLOT_X0 + 11U),
                (u16)(q + 14U), C_BG);
        q = (yb > 14U) ? (u16)(yb - 14U) : yb;
        ui_fill((u16)(PLOT_X0 + 2U), q, (u16)(PLOT_X0 + 11U),
                (u16)(q + 14U), C_BG);
    }

    /* Cursor readout panel.  Dynamic redraw reconstructs grid/traces
     * underneath, so this does not flash the whole plot. */
    ui_fill((u16)(PLOT_X1 - 176U), (u16)(PLOT_Y0 + 2U),
            (u16)(PLOT_X1 - 2U), (u16)(PLOT_Y0 + 17U), C_BG);
}

static u8 ui_channel_signal(u8 ch, MEAS_Result *m)
{
    const u16 *f;

    if (ch > 1U || !CMD_GetChannelEnabled(ch) ||
        CMD_DisplaySeq() == 0U)
    {
        return 0U;
    }

    f = (ch == 0U) ? CMD_DisplayFrame() : CMD_DisplayFrame2();
    MEAS_Compute(f, ADC_FRAME_LEN, (float)ADC1_GetSampleRate(), m);
    if (m->vpp < UI_SIG_VPP_MIN || m->freq < UI_SIG_FREQ_MIN)
    {
        return 0U;
    }
    if (m->type == 3U)      /* pure DC: no AC waveform */
    {
        return 0U;
    }
    return 1U;
}

static void ui_draw_wave_area(u8 full)
{
    u16 i;
    u16 x;
    u16 y;
    u16 level_y;
    u16 level_raw;
    u16 vdiv_mv[2];
    u16 vcenter_mv[2];
    u32 tb = CMD_GetDisplayTimebaseUs();
    float vdiv[2];
    float mid[2];
    char buf[12];
    const u16 *frame;
    u16 trig;
    u16 span;
    u16 px;
    u16 mn;
    u16 mx;
    u8  trig_ch;
    u8  zch;
    u16 zy;
    float zyf;
    MEAS_Result sm1;
    MEAS_Result sm2;
    u8  sig1;
    u8  sig2;
    u8  no_sig;

    vdiv_mv[0] = CMD_GetVdivMvCh(0);
    vdiv_mv[1] = CMD_GetVdivMvCh(1);
    vcenter_mv[0] = CMD_GetVcenterMvCh(0);
    vcenter_mv[1] = CMD_GetVcenterMvCh(1);

    if (vdiv_mv[0] < 20U) vdiv_mv[0] = 20U;
    if (vdiv_mv[1] < 20U) vdiv_mv[1] = 20U;
    vdiv[0] = (float)vdiv_mv[0] / 1000.0f;
    vdiv[1] = (float)vdiv_mv[1] / 1000.0f;
    mid[0] = (float)vcenter_mv[0] / 1000.0f;
    mid[1] = (float)vcenter_mv[1] / 1000.0f;

    span = CMD_GetDisplaySpan();
    if (span < 16U) span = 16U;
    if (span > ADC_FRAME_LEN) span = ADC_FRAME_LEN;

    sig1 = ui_channel_signal(0U, &sm1);
    sig2 = ui_channel_signal(1U, &sm2);
    no_sig = (u8)((sig1 || sig2) ? 0U : 1U);

    /* No-signal <-> signal is a page-level change: clear the old
     * placeholder before drawing the new state, otherwise the "NO
     * SIGNAL" text would remain under a returning trace. */
    if (no_sig != s_no_sig)
    {
        s_no_sig = no_sig;
        if (!full)
        {
            ui_draw_wave_area(1U);
            return;
        }
    }

    if (full)
    {
        /* panel background + grid + static labels */
        ui_fill(PAN_X0, PAN_Y0, PAN_X1, PAN_Y1, C_BG);
        s_zero_valid = 0U;
        ui_draw_wave_grid();

        /* CH1 labels left, CH2 labels right; CH2 alone uses the left. */
        if (CMD_GetChannelEnabled(0))
        {
            ui_draw_voltage_labels(0U, vdiv_mv[0], vcenter_mv[0]);
        }
        if (CMD_GetChannelEnabled(1))
        {
            ui_draw_voltage_labels(1U, vdiv_mv[1], vcenter_mv[1]);
        }
        ui_draw_time_labels(tb);
    }
    else
    {
        /* Dynamic: erase only the old trace/trigger/zero line, then
         * restore the grid under them.  No panel fill. */
        ui_erase_zero_line();
        ui_erase_trigger_line();
        ui_erase_trace();
        ui_draw_wave_grid();
    }

    /* 0 V reference axis: only drawn when it is inside the visible
     * vertical range; the waveform itself is centred on the DC bias. */
    zch = CMD_GetChannelEnabled(0) ? 0U : 1U;
    zyf = (float)((PLOT_Y0 + PLOT_Y1) / 2) -
          (0.0f - mid[zch]) * ((float)PLOT_H / 8.0f) / vdiv[zch];
    if (zyf >= (float)PLOT_Y0 && zyf <= (float)PLOT_Y1)
    {
        zy = (u16)zyf;
        for (x = PLOT_X0; x < (u16)(PLOT_X1 - 2U); x = (u16)(x + 8U))
        {
            ui_fill(x, zy, (u16)(x + 3U), zy, C_MUTED);
        }
        s_zero_y = zy;
        s_zero_valid = 1U;
    }

    /* trigger level: dashed orange line (active channel's scale) */
    trig_ch = CMD_GetChannelEnabled(0) ? 0U : 1U;
    level_raw = ui_trig_raw();
    level_y = ui_raw_to_y(level_raw, vdiv[trig_ch], mid[trig_ch]);
    for (x = PLOT_X0; x < (u16)(PLOT_X1 - 2U); x = (u16)(x + 8U))
    {
        ui_fill(x, level_y, (u16)(x + 3U), level_y, C_ACCENT);
    }
    ui_text((u16)(PLOT_X1 - 12U), (u16)(level_y > 14U ? level_y - 14U : PLOT_Y0),
            14, 14, 12, C_ACCENT, C_BG, "T");
    s_old_trig_y = level_y;
    s_old_trig_valid = 1U;

    /* waveform: CH1 cyan, CH2 green, same trigger/time base */
    {
        static const u16 ch_color[2] = {C_VALUE, C_OK};
        u8 ch;

        for (ch = 0U; ch < 2U; ch++)
        {
            s_trace_mode[ch] = 0U;
            s_trace_count[ch] = 0U;
        }

        if (CMD_DisplaySeq() != 0U)
        {
            trig = CMD_DisplayTrig();

            for (ch = 0U; ch < 2U; ch++)
            {
                if (!CMD_GetChannelEnabled(ch) || !((ch == 0U) ? sig1 : sig2))
                {
                    s_trace_mode[ch] = 0U;
                    s_trace_count[ch] = 0U;
                    continue;
                }

                frame = (ch == 0U) ? CMD_DisplayFrame()
                                   : CMD_DisplayFrame2();

                if (span < PLOT_W)
                {
                    /* Few samples: resample to one point per screen
                     * column instead of placing one point every few
                     * pixels.  The old sparse polyline looked like a
                     * staircase at the right edge and jittered when the
                     * trigger moved by a fraction of a sample. */
                    u16 k;
                    u16 xp = 0, yp = 0;

                    s_trace_mode[ch] = 2U;
                    s_trace_count[ch] = PLOT_W;
                    POINT_COLOR = ch_color[ch];
                    for (k = 0U; k < PLOT_W; k++)
                    {
                        u32 num = (u32)k * (u32)(span - 1U);
                        u16 i0 = (u16)(num / (u16)(PLOT_W - 1U));
                        u16 rem = (u16)(num % (u16)(PLOT_W - 1U));
                        u16 next = (u16)((i0 + 1U < span) ? (i0 + 1U) : i0);
                        u32 idx0 = (u32)trig + i0;
                        u32 idx1 = (u32)trig + next;
                        u16 v0;
                        u16 v1;
                        u16 v;
                        u16 xn;
                        u16 yn;

                        if (idx0 >= ADC_FRAME_LEN) idx0 -= ADC_FRAME_LEN;
                        if (idx1 >= ADC_FRAME_LEN) idx1 -= ADC_FRAME_LEN;
                        v0 = frame[idx0];
                        v1 = frame[idx1];
                        if (v1 >= v0)
                            v = (u16)(v0 + ((u32)(v1 - v0) * rem) /
                                           (u16)(PLOT_W - 1U));
                        else
                            v = (u16)(v0 - ((u32)(v0 - v1) * rem) /
                                           (u16)(PLOT_W - 1U));

                        xn = (u16)(PLOT_X0 + k);
                        yn = ui_raw_to_y(v, vdiv[ch], mid[ch]);

                        s_trace_px[ch][k] = xn;
                        s_trace_py[ch][k] = yn;
                        if (k != 0U) LCD_DrawLine(xp, yp, xn, yn);
                        xp = xn;
                        yp = yn;
                    }
                }
                else
                {
                    u16 px_top = 0;
                    u16 px_bot = 0;
                    u8  have_prev = 0;

                    s_trace_mode[ch] = 1U;
                    POINT_COLOR = ch_color[ch];
                    for (px = 0; px < PLOT_W; px++)
                    {
                        u32 s0 = (u32)trig + ((u32)px * span) / PLOT_W;
                        u32 s1 = (u32)trig +
                                 ((u32)(px + 1U) * span) / PLOT_W;
                        u16 ytop;
                        u16 ybot;

                        if (s1 <= s0) s1 = s0 + 1U;

                        mn = 4095U;
                        mx = 0U;
                        for (i = 0; i < (u16)(s1 - s0); i++)
                        {
                            u32 idx = s0 + i;
                            u16 v;

                            if (idx >= ADC_FRAME_LEN) idx -= ADC_FRAME_LEN;
                            v = frame[idx];
                            if (v < mn) mn = v;
                            if (v > mx) mx = v;
                        }

                        ytop = ui_raw_to_y(mx, vdiv[ch], mid[ch]);
                        ybot = ui_raw_to_y(mn, vdiv[ch], mid[ch]);
                        if (ytop > ybot)
                        {
                            u16 t = ytop; ytop = ybot; ybot = t;
                        }

                        /* Draw the current column's min/max envelope,
                         * then join it to the previous column with a
                         * thin connector.  The stored erase span is
                         * expanded to cover those connector pixels, so
                         * no ghost/double edge is left behind without
                         * filling the whole vertical wedge (which made
                         * fast waveforms look thick/blocky). */
                        {
                            u16 y0 = ytop;
                            u16 y1 = ybot;

                            if (have_prev)
                            {
                                if (px_top < y0) y0 = px_top;
                                if (px_bot < y0) y0 = px_bot;
                                if (px_top > y1) y1 = px_top;
                                if (px_bot > y1) y1 = px_bot;
                            }

                            x = (u16)(PLOT_X0 + px);
                            s_trace_y0[ch][px] = y0;
                            s_trace_y1[ch][px] = y1;
                            ui_fill(x, ytop, x, ybot, ch_color[ch]);

                            if (have_prev)
                            {
                                u16 c0 = (ytop < px_top) ? ytop : px_top;
                                u16 c1 = (ybot > px_bot) ? ybot : px_bot;

                                if (c0 < s_trace_y0[ch][px - 1U])
                                    s_trace_y0[ch][px - 1U] = c0;
                                if (c1 > s_trace_y1[ch][px - 1U])
                                    s_trace_y1[ch][px - 1U] = c1;

                                LCD_DrawLine((u16)(x - 1U), px_top, x, ytop);
                                LCD_DrawLine((u16)(x - 1U), px_bot, x, ybot);
                            }
                        }

                        px_top = ytop;
                        px_bot = ybot;
                        have_prev = 1;
                    }
                }
            }
        }
    }

    /* Phase difference badge: draw whenever both channels are visible,
     * including HOLD.  (Cursors are single-channel only, so this cannot
     * overlap the cursor readout.) */
    if (CMD_GetChannelEnabled(0) && CMD_GetChannelEnabled(1))
    {
        char pbuf[20];
        float phase = -1.0f;

        if (CMD_DisplaySeq() != 0U)
        {
            MEAS_Result pm1;
            MEAS_Result pm2;
            float fs = (float)ADC1_GetSampleRate();
            float f1;
            float f2;
            float fmax;
            float df;

            MEAS_Compute(CMD_DisplayFrame(), ADC_FRAME_LEN, fs, &pm1);
            MEAS_Compute(CMD_DisplayFrame2(), ADC_FRAME_LEN, fs, &pm2);
            f1 = pm1.freq;
            f2 = pm2.freq;
            fmax = (f1 > f2) ? f1 : f2;
            df = (f1 > f2) ? (f1 - f2) : (f2 - f1);

            /* A fixed phase angle only exists while both channels have
             * the same fundamental.  Allow 20% for measurement spread;
             * really different frequencies still show --. */
            if (pm1.vpp >= 0.03f && pm2.vpp >= 0.03f &&
                fmax >= 20.0f && df <= (fmax * 0.20f))
            {
                phase = MEAS_Phase(CMD_DisplayFrame(), CMD_DisplayFrame2(),
                                   ADC_FRAME_LEN, fs, (f1 + f2) * 0.5f);
            }
        }

        ui_fill_round((u16)(PLOT_X1 - 92U), (u16)(PLOT_Y0 + 2U),
                      (u16)(PLOT_X1 - 2U), (u16)(PLOT_Y0 + 18U),
                      5, C_PANEL);
        if (phase >= 0.0f)
            sprintf(pbuf, "PH %.1fdeg", (double)phase);
        else
            sprintf(pbuf, "PH --");
        ui_text((u16)(PLOT_X1 - 88U), (u16)(PLOT_Y0 + 3U), 84U, 14U, 12U,
                C_EDIT, C_PANEL, pbuf);
    }

    /* No AC input: replace the noisy flat trace with a readable
     * placeholder.  Per-channel DC/-- values stay in the two cards. */
    if (no_sig)
    {
        u16 cy = (u16)((PLOT_Y0 + PLOT_Y1) / 2U);

        ui_fill_round((u16)(PLOT_X0 + 30U), (u16)(cy - 18U),
                      (u16)(PLOT_X1 - 30U), (u16)(cy + 24U),
                      6, C_BG);
        ui_frame_round((u16)(PLOT_X0 + 30U), (u16)(cy - 18U),
                       (u16)(PLOT_X1 - 30U), (u16)(cy + 24U),
                       6, C_BORDER);
        ui_text_c((u16)(PLOT_X0 + 30U), (u16)(cy - 7U),
                  (u16)(PLOT_W - 60U), 18U, 16U, C_MUTED, C_BG,
                  "NO SIGNAL");
    }

    /* panel frame: static, only needed on a full redraw */
    if (full)
    {
        ui_frame_round(PAN_X0, PAN_Y0, PAN_X1, PAN_Y1, 7, C_BORDER);
    }

    /* frozen: X (time) or Y (voltage) cursors */
    if (s_frozen)
    {
        if (s_cursor_mode == 0U)
        {
            u16 ca = s_cursor_a;
            u16 cb = s_cursor_b;
            u32 dt_us;
            u32 f_hz;
            u32 win_us = tb * 10U;

            if (ca < PLOT_X0) ca = PLOT_X0;
            if (ca > PLOT_X1) ca = PLOT_X1;
            if (cb < PLOT_X0) cb = PLOT_X0;
            if (cb > PLOT_X1) cb = PLOT_X1;

            for (y = PLOT_Y0; y < (u16)(PLOT_Y1 - 3U); y = (u16)(y + 8U))
            {
                ui_fill(ca, y, ca, (u16)(y + 3U), C_EDIT);
                ui_fill(cb, y, cb, (u16)(y + 3U), C_EDIT);
            }
            ui_text((ca > (u16)(PLOT_X1 - 10U)) ? (u16)(PLOT_X1 - 10U) : ca,
                    PLOT_Y0 + 2, 10, 14, 12, C_EDIT, C_BG, "A");
            ui_text((cb > (u16)(PLOT_X1 - 10U)) ? (u16)(PLOT_X1 - 10U) : cb,
                    PLOT_Y0 + 2, 10, 14, 12, C_EDIT, C_BG, "B");

            if (ca > cb) { u16 t = ca; ca = cb; cb = t; }
            dt_us = (u32)(cb - ca) * win_us / (PLOT_W - 1U);
            if (dt_us == 0U) dt_us = 1U;
            f_hz = 1000000UL / dt_us;

            ui_fill_round((u16)(PLOT_X1 - 176U), PLOT_Y0 + 2,
                          (u16)(PLOT_X1 - 2U), (u16)(PLOT_Y0 + 17U),
                          5, C_PANEL);
            sprintf(buf, "dT %.3fms F %.2fkHz", (double)dt_us / 1000.0,
                    (double)f_hz / 1000.0);
            ui_text((u16)(PLOT_X1 - 172U), PLOT_Y0 + 3, 170, 14, 12,
                    C_EDIT, C_PANEL, buf);
        }
        else
        {
            u8 ach = CMD_GetChannelEnabled(0) ? 0U : 1U;
            u16 ya = s_cursor_ya;
            u16 yb = s_cursor_yb;
            float ppd = (float)PLOT_H / 8.0f;
            float cy  = (float)((PLOT_Y0 + PLOT_Y1) / 2);
            float va;
            float vb;
            float dv;

            if (ya < PLOT_Y0) ya = PLOT_Y0;
            if (ya > PLOT_Y1) ya = PLOT_Y1;
            if (yb < PLOT_Y0) yb = PLOT_Y0;
            if (yb > PLOT_Y1) yb = PLOT_Y1;

            for (x = PLOT_X0; x < (u16)(PLOT_X1 - 2U); x = (u16)(x + 8U))
            {
                ui_fill(x, ya, (u16)(x + 3U), ya, C_EDIT);
                ui_fill(x, yb, (u16)(x + 3U), yb, C_EDIT);
            }
            ui_text((u16)(PLOT_X0 + 2U), (u16)(ya > 14U ? ya - 14U : ya),
                    10, 14, 12, C_EDIT, C_BG, "A");
            ui_text((u16)(PLOT_X0 + 2U), (u16)(yb > 14U ? yb - 14U : yb),
                    10, 14, 12, C_EDIT, C_BG, "B");

            va = mid[ach] + (cy - (float)ya) * vdiv[ach] / ppd;
            vb = mid[ach] + (cy - (float)yb) * vdiv[ach] / ppd;
            dv = (va > vb) ? (va - vb) : (vb - va);

            ui_fill_round((u16)(PLOT_X1 - 176U), PLOT_Y0 + 2,
                          (u16)(PLOT_X1 - 2U), (u16)(PLOT_Y0 + 17U),
                          5, C_PANEL);
            sprintf(buf, "CH%u dY %.2fV A%.2f B%.2f", (unsigned)(ach + 1U),
                    (double)dv, (double)va, (double)vb);
            ui_text((u16)(PLOT_X1 - 172U), PLOT_Y0 + 3, 170, 14, 12,
                    C_EDIT, C_PANEL, buf);
        }
    }

    if (full)
    {
        s_axis_tb      = tb;
        s_axis_vdiv[0]    = vdiv_mv[0];
        s_axis_vdiv[1]    = vdiv_mv[1];
        s_axis_vcenter[0] = vcenter_mv[0];
        s_axis_vcenter[1] = vcenter_mv[1];
        s_axis_valid   = 1U;
    }
}

/* Per-acquisition refresh: only erase/redraw the trace and the trigger
 * line.  If the axis scale changed, fall back to a full redraw so the
 * tick labels stay correct. */
static void ui_draw_wave_dynamic(void)
{
    u32 tb = CMD_GetDisplayTimebaseUs();
    u16 vdiv0 = CMD_GetVdivMvCh(0);
    u16 vdiv1 = CMD_GetVdivMvCh(1);
    u16 vcen0 = CMD_GetVcenterMvCh(0);
    u16 vcen1 = CMD_GetVcenterMvCh(1);

    if (!s_axis_valid)
    {
        ui_draw_wave_area(1U);
        return;
    }

    /* Tick labels change far less often than the trace.  Refresh only
     * the affected label strip, then erase/redraw the trace. */
    if (tb != s_axis_tb)
    {
        ui_erase_time_labels();
        ui_draw_time_labels(tb);
        s_axis_tb = tb;
    }
    if (vdiv0 != s_axis_vdiv[0] || vdiv1 != s_axis_vdiv[1] ||
        vcen0 != s_axis_vcenter[0] || vcen1 != s_axis_vcenter[1])
    {
        ui_erase_voltage_labels();
        if (CMD_GetChannelEnabled(0))
            ui_draw_voltage_labels(0U, vdiv0, vcen0);
        if (CMD_GetChannelEnabled(1))
            ui_draw_voltage_labels(1U, vdiv1, vcen1);
        s_axis_vdiv[0] = vdiv0;
        s_axis_vdiv[1] = vdiv1;
        s_axis_vcenter[0] = vcen0;
        s_axis_vcenter[1] = vcen1;
    }

    ui_draw_wave_area(0U);
}

static void ui_draw_ch_card(u8 ch, const char *line1, const char *line2,
                            u16 color, u8 clip, u8 enabled)
{
    static const ui_rect R[2] = {
        {  6, 174, 152, 34},
        {162, 174, 152, 34}
    };
    const ui_rect *r = &R[ch];
    u16 fg  = enabled ? color : C_MUTED;
    u16 vfg = enabled ? color : C_MUTED;
    u16 bd  = enabled ? (clip ? C_ERR : color) : C_BORDER;
    u16 bg  = enabled ? C_PANEL_HI : C_PANEL;

    ui_fill(r->x, r->y, (u16)(r->x + r->w - 1U),
            (u16)(r->y + r->h - 1U), bg);
    ui_frame_round(r->x, r->y, (u16)(r->x + r->w - 1U),
                   (u16)(r->y + r->h - 1U), 5U, bd);

    ui_text((u16)(r->x + 5U), (u16)(r->y + 1U), (u16)(r->w - 10U),
            16U, 16U, fg, bg, line1);
    ui_hz16_text((u16)(r->x + 5U), (u16)(r->y + 17U), line2,
                 vfg, bg);
}

static void ui_draw_params(void)
{
    MEAS_Result m;
    char l1[24];
    char l2[32];
    u8 ch;
    u8 clip;
    u8 enabled;

    for (ch = 0U; ch < 2U; ch++)
    {
        clip = 0U;
        enabled = CMD_GetChannelEnabled(ch);

        if (!enabled)
        {
            sprintf(l1, "CH%u OFF", (unsigned)(ch + 1U));
            sprintf(l2, "P-- D-- --");
        }
        else if (CMD_DisplaySeq() == 0U)
        {
            sprintf(l1, "CH%u --", (unsigned)(ch + 1U));
            sprintf(l2, "P-- D-- --");
        }
        else
        {
            const u16 *f = (ch == 0U) ? CMD_DisplayFrame()
                                      : CMD_DisplayFrame2();
            u8 type_now;
            u8 valid;

            MEAS_Compute(f, ADC_FRAME_LEN,
                         (float)ADC1_GetSampleRate(), &m);

            type_now = (ch == 0U) ? CMD_GetWaveType() : m.type;
            valid = (u8)((m.vpp >= UI_SIG_VPP_MIN &&
                          m.freq >= UI_SIG_FREQ_MIN &&
                          m.type != 3U) ? 1U : 0U);

            if (!valid)
            {
                /* No AC waveform: show the DC level instead of noise. */
                sprintf(l1, "CH%u --", (unsigned)(ch + 1U));
                if (m.dc >= 0.05f)
                    sprintf(l2, "D%.2fV NO SIG", (double)m.dc);
                else
                    sprintf(l2, "NO SIGNAL");
            }
            else
            {
                if (m.freq >= 1000.0f)
                    sprintf(l1, "CH%u %.2fkHz", (unsigned)(ch + 1U),
                            (double)m.freq / 1000.0);
                else
                    sprintf(l1, "CH%u %.1fHz", (unsigned)(ch + 1U),
                            (double)m.freq);

                sprintf(l2, "P%.2fV D%.2fV %s",
                        (double)m.vpp, (double)m.dc,
                        ui_type_short(type_now));
                clip = m.clipped;
            }
        }

        if (strcmp(l1, s_ch_line1[ch]) != 0 ||
            strcmp(l2, s_ch_line2[ch]) != 0 ||
            s_ch_en_cache[ch] != enabled)
        {
            strcpy(s_ch_line1[ch], l1);
            strcpy(s_ch_line2[ch], l2);
            s_ch_en_cache[ch] = enabled;
            ui_draw_ch_card(ch, l1, l2,
                            (ch == 0U) ? C_VALUE : C_OK, clip, enabled);
        }
    }
}

static void ui_draw_nav(void)
{
    u8 running = CMD_IsRunning();
    u8 armed = CMD_GetSingleArmed();

    ui_fill(0, 209, SCR_W - 1, SCR_H - 1, C_BG);

    ui_cn_button(&R_NAV_RUN, running ? CN_RUN : CN_STOP,
                 running ? C_OK : C_PANEL,
                 running ? C_OK : C_ERR,
                 running ? C_BG : C_ERR);
    ui_cn_button(&R_NAV_SINGLE, CN_SINGLE,
                 armed ? C_EDIT : C_PANEL,
                 armed ? C_EDIT : C_BORDER,
                 armed ? C_BG : C_TEXT);
    if (s_frozen)
    {
        ui_cn_button(&R_CUR_X, "X",
                     (s_cursor_mode == 0U) ? C_EDIT : C_PANEL,
                     (s_cursor_mode == 0U) ? C_EDIT : C_BORDER,
                     (s_cursor_mode == 0U) ? C_BG : C_TEXT);
        ui_cn_button(&R_CUR_Y, "Y",
                     (s_cursor_mode == 1U) ? C_EDIT : C_PANEL,
                     (s_cursor_mode == 1U) ? C_EDIT : C_BORDER,
                     (s_cursor_mode == 1U) ? C_BG : C_TEXT);
    }
    else
    {
        ui_cn_button(&R_NAV_EDGE, CMD_GetEdgeRise() ? CN_RISE : CN_FALL,
                     C_PANEL, C_BORDER, C_TEXT);
    }
    ui_cn_button(&R_NAV_HOLD, CN_HOLD,
                 s_frozen ? C_ACCENT : C_PANEL,
                 s_frozen ? C_ACCENT : C_BORDER,
                 s_frozen ? C_BG : C_TEXT);
    ui_cn_button(&R_NAV_MENU, CN_MENU, C_PANEL_HI, C_BORDER, C_TEXT);
}

static u32 ui_scope_signature(void)
{
    u32 sig = 0;

    sig |= CMD_IsRunning() ? 1UL : 0UL;
    sig |= ((u32)CMD_GetMode()) << 1;
    sig |= ((u32)CMD_GetEdgeRise()) << 3;
    sig |= ((u32)CMD_GetSingleArmed()) << 4;
    sig |= ((u32)s_frozen) << 5;
    sig |= ((u32)CMD_GetChannelEnabled(0)) << 7;
    sig |= ((u32)((CMD_GetAutoTimebase() || CMD_GetAutoVdiv()) ? 1 : 0)) << 22;
    sig |= ((u32)CMD_GetChannelEnabled(1)) << 23;

    return sig;
}

/* Hash of every scale/trigger setting (both channels), used to force a
 * full redraw immediately when a parameter changes. */
static u32 ui_param_signature(void)
{
    u32 h = 0U;

    h += CMD_GetTimebaseUs();
    h += (CMD_GetVdivMvCh(0) / 10U) * 131U;
    h += (CMD_GetVdivMvCh(1) / 10U) * 137U;
    h += (CMD_GetTrigLevel() / 16U) * 151U;
    h += ((CMD_GetAutoTimebase() || CMD_GetAutoVdiv()) ? 1U : 0U) * 157U;
    /* A waveform-type change should repaint the whole plot, not just
     * erase the last trace: this guarantees the old sine/square/triangle
     * is gone before the new one appears. */
    h += (u32)CMD_GetWaveType() * 163U;
    return h;
}

static void ui_draw_scope_page(void)
{
    ui_fill(0, 0, SCR_W - 1, SCR_H - 1, C_BG);
    ui_topbar();
    ui_draw_wave_area(1U);
    ui_draw_params();
    ui_draw_nav();
    s_scope_sig = ui_scope_signature();
    s_param_sig = ui_param_signature();
}

/* ------------------------------------------------------------------ */
/* menu page (KEYctrl style)                                          */
/* ------------------------------------------------------------------ */
static void ui_draw_tile(u8 index, const char *main_txt, const char *sub_txt,
                         u8 active)
{
    const ui_rect *r = &R_TILE[index];
    u16 fill = active ? C_OK : C_PANEL;
    u16 border = active ? C_EDIT : C_BORDER;
    u16 fg = active ? C_BG : C_TEXT;
    u16 sub = active ? C_BG : C_MUTED;

    ui_fill_round(r->x, r->y, (u16)(r->x + r->w - 1),
                  (u16)(r->y + r->h - 1), 8, fill);
    ui_frame_round(r->x, r->y, (u16)(r->x + r->w - 1),
                   (u16)(r->y + r->h - 1), 8, border);

    ui_cn_c(r->x, (u16)(r->y + 8U), r->w, main_txt, fg, fill);
    ui_cn_c(r->x, (u16)(r->y + 34U), r->w, sub_txt, sub, fill);
}

static void ui_draw_tile_arrow(u8 index, const char *main_txt, const char *sub_txt)
{
    const ui_rect *r = &R_TILE[index];

    ui_fill_round(r->x, r->y, (u16)(r->x + r->w - 1),
                  (u16)(r->y + r->h - 1), 8, C_PANEL);
    ui_frame_round(r->x, r->y, (u16)(r->x + r->w - 1),
                   (u16)(r->y + r->h - 1), 8, C_BORDER);

    ui_cn_c(r->x, (u16)(r->y + 8U), r->w, main_txt, C_TEXT, C_PANEL);
    ui_text_c((u16)(r->x + 20U), (u16)(r->y + 34U), (u16)(r->w - 40U), 18, 16,
              C_VALUE, C_PANEL, sub_txt);

    ui_text((u16)(r->x + 6U), (u16)(r->y + 34U), 12, 18, 16, C_MUTED, C_PANEL, "<");
    ui_text((u16)(r->x + r->w - 14U), (u16)(r->y + 34U), 12, 18, 16, C_MUTED, C_PANEL, ">");
}

static void ui_draw_slider(void)
{
    u16 level = ui_trig_raw();
    u16 kx;
    u16 fill_w;
    char buf[12];
    u8 auto_lvl = (u8)(CMD_GetTrigManual() ? 0 : 1);

    ui_fill_round(R_SLIDER.x, R_SLIDER.y,
                  (u16)(R_SLIDER.x + R_SLIDER.w - 1),
                  (u16)(R_SLIDER.y + R_SLIDER.h - 1), 8, C_PANEL);
    ui_frame_round(R_SLIDER.x, R_SLIDER.y,
                   (u16)(R_SLIDER.x + R_SLIDER.w - 1),
                   (u16)(R_SLIDER.y + R_SLIDER.h - 1), 8, C_BORDER);

    ui_cn(12, 192, CN_TRIGLEVEL, C_MUTED, C_PANEL);

    ui_cn_pill(&R_AUTO, auto_lvl ? CN_AUTO : CN_MANUAL,
               auto_lvl ? C_OK : C_PANEL_HI,
               C_BORDER,
               auto_lvl ? C_BG : C_TEXT);

    sprintf(buf, "%.2fV", (double)level * 3.3 / 4095.0);
    ui_text(12, 210, 76, 18, 16, C_VALUE, C_PANEL, buf);

    /* track */
    ui_fill_round(SLIDER_X0, (u16)(SLIDER_Y - 3U), SLIDER_X1,
                  (u16)(SLIDER_Y + 3U), 3, C_PANEL_HI);

    kx = (u16)(SLIDER_X0 + ((u32)level * (SLIDER_X1 - SLIDER_X0)) / 4095U);
    fill_w = (u16)(kx - SLIDER_X0);
    if (fill_w > 0U)
    {
        ui_fill_round(SLIDER_X0, (u16)(SLIDER_Y - 3U), kx,
                      (u16)(SLIDER_Y + 3U), 3, C_ACCENT);
    }
    ui_fill_round((u16)(kx - 2U), (u16)(SLIDER_Y - 6U),
                  (u16)(kx + 2U), (u16)(SLIDER_Y + 6U), 2, C_EDIT);

    ui_cn_button(&R_TRIG_MINUS, "-", C_PANEL_HI, C_BORDER, C_TEXT);
    ui_cn_button(&R_TRIG_PLUS,  "+", C_PANEL_HI, C_BORDER, C_TEXT);
}

static void ui_draw_menu_page(void)
{
    char tb[12];
    char vd[12];
    char line[64];
    u8  mode = CMD_GetMode();
    u8  running = CMD_IsRunning();
    u8  armed = CMD_GetSingleArmed();
    u16 trig = ui_trig_raw();

    ui_fill(0, 0, SCR_W - 1, SCR_H - 1, C_BG);

    /* title bar */
    ui_fill(0, 0, SCR_W - 1, TOP_H - 1, C_PANEL);
    ui_cn(8, 4, CN_CONTROL, C_TEXT, C_PANEL);
    ui_cn_button(&R_BTN_THD, "THD", C_PANEL_HI, C_BORDER, C_TEXT);
    ui_cn_button(&R_BTN_CAL,  CN_CAL,  C_PANEL_HI, C_BORDER, C_TEXT);
    ui_cn_button(&R_BTN_BACK, CN_BACK, C_PANEL_HI, C_BORDER, C_TEXT);
    ui_fill(0, TOP_H - 1, SCR_W - 1, TOP_H - 1, C_ACCENT);

    /* status line */
    ui_fmt_tb(CMD_GetTimebaseUs(), tb);
    ui_fmt_vd(CMD_GetVdivMvCh(CMD_GetChannelEnabled(0) ? 0U : 1U), vd);
    sprintf(line, "%s | TB %s | VD %s | T %.2fV | %s",
            ui_mode_name(mode), tb, vd,
            (double)trig * 3.3 / 4095.0,
            CMD_GetEdgeRise() ? "RISE" : "FALL");
    ui_text(8, 30, SCR_W - 16, 14, 12, C_MUTED, C_BG, line);

    /* 3x2 function cards */
    ui_draw_tile(0, CN_RUNSTOP, running ? CN_RUNNING : CN_STOPPED, running);
    ui_draw_tile(1, CN_CAPTURE,
                 armed ? CN_READY :
                 (((mode == CMD_MODE_SINGLE) && !running) ? CN_CAPTURED : CN_READY),
                 armed);
    ui_draw_tile(2, CN_TRIGMODE, ui_mode_cn(mode), 0);

    ui_draw_tile_arrow(3, CN_TIMEBASE, tb);
    ui_draw_tile_arrow(4, CN_VDIV, vd);
    ui_draw_tile(5, CN_EDGE, CMD_GetEdgeRise() ? CN_RISE : CN_FALL, 0);

    ui_draw_slider();
}

/* ------------------------------------------------------------------ */
/* THD / harmonic page                                                */
/* ------------------------------------------------------------------ */
static void ui_draw_thd_page(u8 full)
{
    char buf[48];
    const u16 *f;
    MEAS_Result m;
    MEAS_Result m1;
    MEAS_Result m2;
    float amp[5];
    float thd = 0.0f;
    float f0;
    float err;
    float vmax;
    u8  ch;
    u8  k;
    u8  ok;
    u8  valid1;
    u8  valid2;
    u16 i;
    u16 x;
    u16 h;
    u16 old_top;

    if (full)
    {
        ui_fill(0, 0, SCR_W - 1, SCR_H - 1, C_BG);

        /* title bar */
        ui_fill(0, 0, SCR_W - 1, TOP_H - 1, C_PANEL);
        ui_text(8, 4, 200, 16, 16, C_TEXT, C_PANEL, "THD / HARMONICS");
        ui_cn_button(&R_BTN_BACK, CN_BACK, C_PANEL_HI, C_BORDER, C_TEXT);
        ui_fill(0, TOP_H - 1, SCR_W - 1, TOP_H - 1, C_ACCENT);

        /* bar-chart panel */
        ui_fill_round(6, 50, 313, 210, 7, C_PANEL);
        ui_frame_round(6, 50, 313, 210, 7, C_BORDER);

        for (i = 0U; i < 5U; i++)
        {
            s_thd_bar_top[i] = 189U;
            x = (u16)(46U + i * 48U);
            sprintf(buf, "H%u", (unsigned)(i + 1U));
            ui_text((u16)(x + 8U), 193, 26, 14, 12, C_MUTED, C_PANEL, buf);
        }
        s_thd_ok_prev = 0U;
    }

    MEAS_Compute(CMD_DisplayFrame(), ADC_FRAME_LEN,
                 (float)ADC1_GetSampleRate(), &m1);
    MEAS_Compute(CMD_DisplayFrame2(), ADC_FRAME_LEN,
                 (float)ADC1_GetSampleRate(), &m2);

    /* Choose the channel that actually carries a 1-5 kHz signal.  A
     * CH1 that is enabled but not connected (PA4 problem) must not hide
     * a perfectly good CH2 measurement. */
    valid1 = (u8)((CMD_GetChannelEnabled(0) && m1.vpp >= 0.03f &&
                   m1.freq >= 500.0f && m1.freq <= 5500.0f) ? 1U : 0U);
    valid2 = (u8)((CMD_GetChannelEnabled(1) && m2.vpp >= 0.03f &&
                   m2.freq >= 500.0f && m2.freq <= 5500.0f) ? 1U : 0U);

    if (valid1 && (!valid2 || m1.vpp >= m2.vpp))
    {
        ch = 0U;
        m = m1;
        f = CMD_DisplayFrame();
    }
    else if (valid2)
    {
        ch = 1U;
        m = m2;
        f = CMD_DisplayFrame2();
    }
    else
    {
        ch = CMD_GetChannelEnabled(0) ? 0U : 1U;
        f = (ch == 0U) ? CMD_DisplayFrame() : CMD_DisplayFrame2();
        m = (ch == 0U) ? m1 : m2;
    }

    f0 = m.freq;
    k = (u8)(f0 / 1000.0f + 0.5f);
    err = f0 - (float)(k * 1000U);
    if (err < 0.0f) err = -err;

    ok = 0U;
    if (k >= 1U && k <= 5U && f0 > 500.0f && f0 < 5500.0f &&
        err <= (f0 * 0.08f + 50.0f))
    {
        ok = MEAS_Harmonics(f, ADC_FRAME_LEN,
                            (float)ADC1_GetSampleRate(),
                            f0, 5U, amp, &thd);
    }

    /* Only the info line is rewritten every update. */
    if (ok)
    {
        /* Show the measured fundamental, not just the rounded test point,
         * so 1/2/3/4/5 kHz runs are visibly distinguishable. */
        sprintf(buf, "CH%u   F=%.2fkHz   THD=%.2f%%",
                (unsigned)(ch + 1U), (double)f0 / 1000.0, (double)thd);
    }
    else
    {
        sprintf(buf, "CH%u   F=--   THD=--   OUT OF RANGE (1..5kHz)",
                (unsigned)(ch + 1U));
    }
    ui_text(8, 30, SCR_W - 16, 14, 12, C_MUTED, C_BG, buf);

    if (!ok)
    {
        for (i = 0U; i < 5U; i++)
        {
            x = (u16)(46U + i * 48U);
            old_top = s_thd_bar_top[i];
            if (old_top < 189U)
                ui_fill(x, old_top, (u16)(x + 35U), 189U, C_PANEL);
            s_thd_bar_top[i] = 189U;
            ui_fill(x, 51, (u16)(x + 35U), 65U, C_PANEL);
        }
        ui_fill(40, 105, 280, 140, C_PANEL);
        ui_text(60, 115, 220, 16, 16, C_ERR, C_PANEL,
                "Please input 1/2/3/4/5 kHz");
        s_thd_ok_prev = 0U;
        return;
    }

    if (!s_thd_ok_prev)
    {
        ui_fill(40, 105, 280, 140, C_PANEL);
    }

    vmax = amp[0];
    if (vmax < 0.01f) vmax = 0.01f;

    for (i = 0U; i < 5U; i++)
    {
        float norm = amp[i] / vmax;
        u16 bh;

        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        bh = (u16)(norm * 110.0f);
        if (amp[i] > 0.002f && bh < 3U) bh = 3U;

        x = (u16)(46U + i * 48U);
        h = (u16)(190U - bh);

        old_top = s_thd_bar_top[i];
        ui_fill(x, 51, (u16)(x + 35U), 65U, C_PANEL);

        if (h < 189U)
        {
            if (old_top < 189U && h > old_top)
            {
                /* shorter bar: clear only the part above the new top */
                ui_fill(x, old_top, (u16)(x + 35U), (u16)(h - 1U),
                        C_PANEL);
            }
            if (h != old_top)
            {
                ui_fill_round(x, h, (u16)(x + 35U), 189U, 3,
                              (i == 0U) ? C_VALUE : C_OK);
                ui_frame_round(x, h, (u16)(x + 35U), 189U, 3,
                               (i == 0U) ? C_VALUE : C_BORDER);
            }
        }
        else if (old_top < 189U)
        {
            ui_fill(x, old_top, (u16)(x + 35U), 189U, C_PANEL);
        }
        s_thd_bar_top[i] = h;

        sprintf(buf, "%.2f", (double)amp[i]);
        ui_text((u16)(x + 2U), 51, 34, 14, 12, C_TEXT, C_PANEL, buf);
    }
    s_thd_ok_prev = 1U;
}

static void ui_enter_thd(void)
{
    s_page = THD_PAGE;
    s_frozen = 0U;
    CMD_SetAutoPause(0);
    s_thd_div = 0U;
    s_thd_seq = 0xFFFFFFFFU;
    ui_draw_thd_page(1U);
    s_thd_seq = CMD_DisplaySeq();
}

/* ------------------------------------------------------------------ */
/* page switching / redraw                                            */
/* ------------------------------------------------------------------ */
static void ui_enter_scope(void)
{
    s_page = SCOPE_PAGE;
    ui_draw_scope_page();
    s_last_seq = CMD_DisplaySeq();
}

static void ui_enter_menu(void)
{
    s_page = MENU_PAGE;
    ui_draw_menu_page();
}

/* ------------------------------------------------------------------ */
/* touch                                                              */
/* ------------------------------------------------------------------ */
/* CH1/CH2 cards are independent on/off switches. */
static void ui_toggle_channel(u8 ch)
{
    CMD_ToggleChannel(ch);

    /* any channel change returns to live display */
    s_frozen = 0;
    CMD_SetAutoPause(0);
    s_cursor_drag = 0;
    s_cursor_touch_freeze = 0;

    ui_draw_params();
    ui_draw_wave_area(1U);
    s_scope_sig = ui_scope_signature();
    s_param_sig = ui_param_signature();
}

static void ui_touch_scope_down(u16 x, u16 y)
{
    static const ui_rect R_CH1_CARD = {  6, 174, 152, 34};
    static const ui_rect R_CH2_CARD = {162, 174, 152, 34};

    /* Channel cards: independent CH1 / CH2 on/off. */
    if (ui_hit(&R_CH1_CARD, x, y))
    {
        ui_toggle_channel(0U);
        return;
    }
    if (ui_hit(&R_CH2_CARD, x, y))
    {
        ui_toggle_channel(1U);
        return;
    }

    if (ui_hit(&R_AUTO_RANGE, x, y))
    {
        CMD_ToggleAutoRange();
        ui_topbar();
        ui_draw_wave_area(1U);
        return;
    }
    if (ui_hit(&R_NAV_RUN, x, y))
    {
        CMD_SetRunning((u8)(CMD_IsRunning() ? 0 : 1));
        ui_draw_nav();
        ui_topbar();
        return;
    }
    if (ui_hit(&R_NAV_SINGLE, x, y))
    {
        CMD_ArmSingle();
        s_frozen = 0;
        ui_draw_nav();
        ui_topbar();
        ui_draw_wave_area(1U);
        return;
    }
    if (s_frozen && ui_hit(&R_CUR_X, x, y))
    {
        ui_erase_cursor_layer();
        s_cursor_mode = 0U;
        ui_draw_nav();
        ui_draw_wave_area(0U);
        return;
    }
    if (s_frozen && ui_hit(&R_CUR_Y, x, y))
    {
        ui_erase_cursor_layer();
        s_cursor_mode = 1U;
        ui_draw_nav();
        ui_draw_wave_area(0U);
        return;
    }
    if (!s_frozen && ui_hit(&R_NAV_EDGE, x, y))
    {
        CMD_CycleEdge();
        ui_draw_nav();
        ui_draw_wave_area(1U);
        return;
    }
    if (ui_hit(&R_NAV_HOLD, x, y))
    {
        s_frozen = (u8)(s_frozen ? 0 : 1);
        CMD_SetAutoPause(s_frozen);
        s_cursor_drag = 0;
        s_cursor_touch_freeze = 0;
        if (s_frozen)
        {
            s_cursor_mode = 0U;
            s_cursor_a = (u16)(PLOT_X0 + PLOT_W / 4U);
            s_cursor_b = (u16)(PLOT_X0 + (PLOT_W * 3U) / 4U);
            s_cursor_ya = (u16)(PLOT_Y0 + PLOT_H / 4U);
            s_cursor_yb = (u16)(PLOT_Y0 + (PLOT_H * 3U) / 4U);
        }
        ui_draw_nav();
        ui_draw_wave_area(1U);
        return;
    }
    if (ui_hit(&R_NAV_MENU, x, y))
    {
        ui_enter_menu();
        return;
    }

    /* plot area: tap freezes; while frozen drag the nearest cursor
     * in the current X/Y mode. */
    if (x >= PLOT_X0 && x <= PLOT_X1 && y >= PLOT_Y0 && y <= PLOT_Y1)
    {
        u16 da;
        u16 db;
        u8  was_frozen = s_frozen;

        if (!s_frozen)
        {
            s_frozen = 1;
            /* A tap on the plot freezes the frame persistently; release
             * does not unfreeze.  Use HOLD to return to live display. */
            s_cursor_touch_freeze = 0U;
            CMD_SetAutoPause(1);
            s_cursor_mode = 0U;
            s_cursor_a = (u16)(PLOT_X0 + PLOT_W / 4U);
            s_cursor_b = (u16)(PLOT_X0 + (PLOT_W * 3U) / 4U);
            s_cursor_ya = (u16)(PLOT_Y0 + PLOT_H / 4U);
            s_cursor_yb = (u16)(PLOT_Y0 + (PLOT_H * 3U) / 4U);
        }

        if (s_cursor_mode == 0U)
        {
            da = (u16)((x > s_cursor_a) ? (x - s_cursor_a) : (s_cursor_a - x));
            db = (u16)((x > s_cursor_b) ? (x - s_cursor_b) : (s_cursor_b - x));

            if (da <= db)
            {
                s_cursor_active = 0U;
                s_cursor_grab_x = (s16)((s32)s_cursor_a - (s32)x);
            }
            else
            {
                s_cursor_active = 1U;
                s_cursor_grab_x = (s16)((s32)s_cursor_b - (s32)x);
            }
            s_cursor_grab_y = 0;
        }
        else
        {
            da = (u16)((y > s_cursor_ya) ? (y - s_cursor_ya) : (s_cursor_ya - y));
            db = (u16)((y > s_cursor_yb) ? (y - s_cursor_yb) : (s_cursor_yb - y));

            if (da <= db)
            {
                s_cursor_active = 0U;
                s_cursor_grab_y = (s16)((s32)s_cursor_ya - (s32)y);
            }
            else
            {
                s_cursor_active = 1U;
                s_cursor_grab_y = (s16)((s32)s_cursor_yb - (s32)y);
            }
            s_cursor_grab_x = 0;
        }

        /* Keep following the finger for as long as it stays down; do
         * not move the cursor on touch-down, so it never jumps. */
        s_cursor_drag = 1U;

        if (!was_frozen)
        {
            ui_draw_nav();
            ui_draw_wave_area(1U);
        }
    }
}

static void ui_touch_menu_down(u16 x, u16 y)
{
    const ui_rect *r;

    if (ui_hit(&R_BTN_THD, x, y))
    {
        ui_enter_thd();
        return;
    }
    if (ui_hit(&R_BTN_BACK, x, y))
    {
        ui_enter_scope();
        return;
    }
    if (ui_hit(&R_BTN_CAL, x, y))
    {
        /* resistive screens only; capacitive panels never need calibration */
        if ((tp_dev.touchtype & 0x80) == 0 && tp_dev.adjust != 0)
        {
            tp_dev.adjust();
            LCD_Clear(C_BG);
            BACK_COLOR = C_BG;
            POINT_COLOR = C_TEXT;
            s_page = SCOPE_PAGE;
            ui_draw_scope_page();
            s_last_seq = CMD_DisplaySeq();
        }
        return;
    }

    r = &R_TILE[0];
    if (ui_hit(r, x, y)) { CMD_SetRunning((u8)(CMD_IsRunning() ? 0 : 1)); ui_draw_menu_page(); return; }
    r = &R_TILE[1];
    if (ui_hit(r, x, y)) { CMD_ArmSingle(); s_frozen = 0; CMD_SetAutoPause(0); ui_draw_menu_page(); return; }
    r = &R_TILE[2];
    if (ui_hit(r, x, y)) { CMD_CycleMode(); ui_draw_menu_page(); return; }

    r = &R_TILE[3];
    if (ui_hit(r, x, y))
    {
        CMD_StepTimebase((s8)((x < (u16)(r->x + r->w / 2U)) ? -1 : 1));
        ui_draw_menu_page();
        return;
    }
    r = &R_TILE[4];
    if (ui_hit(r, x, y))
    {
        CMD_StepVdiv((s8)((x < (u16)(r->x + r->w / 2U)) ? -1 : 1));
        ui_draw_menu_page();
        return;
    }
    r = &R_TILE[5];
    if (ui_hit(r, x, y)) { CMD_CycleEdge(); ui_draw_menu_page(); return; }

    if (ui_hit(&R_AUTO, x, y))
    {
        if (CMD_GetTrigManual()) CMD_EnableAutoTrigLevel();
        else                     CMD_SetTrigLevelRaw(ui_trig_raw());
        ui_draw_slider();
        return;
    }
    if (ui_hit(&R_TRIG_MINUS, x, y))
    {
        CMD_StepTrigLevel(-124);
        ui_draw_slider();
        return;
    }
    if (ui_hit(&R_TRIG_PLUS, x, y))
    {
        CMD_StepTrigLevel(124);
        ui_draw_slider();
        return;
    }

    /* slider track: tap / drag */
    if (x >= (u16)(SLIDER_X0 - 6U) && x <= (u16)(SLIDER_X1 + 6U) &&
        y >= (u16)(SLIDER_Y - 12U) && y <= (u16)(SLIDER_Y + 12U))
    {
        u16 raw;
        s_slider_drag = 1;
        if (x < SLIDER_X0) x = SLIDER_X0;
        if (x > SLIDER_X1) x = SLIDER_X1;
        raw = (u16)(((u32)(x - SLIDER_X0) * 4095U) / (SLIDER_X1 - SLIDER_X0));
        CMD_SetTrigLevelRaw(raw);
        ui_draw_slider();
    }
}

static void ui_touch_thd_down(u16 x, u16 y)
{
    if (ui_hit(&R_BTN_BACK, x, y))
    {
        s_page = MENU_PAGE;
        ui_draw_menu_page();
    }
}

static void ui_touch_move(u16 x, u16 y)
{
    (void)y;

    if (s_page == SCOPE_PAGE && s_frozen && s_cursor_drag)
    {
        if (s_cursor_mode == 0U)
        {
            s32 tx = (s32)x + (s32)s_cursor_grab_x;

            if (tx < PLOT_X0) tx = PLOT_X0;
            if (tx > PLOT_X1) tx = PLOT_X1;

            if ((u16)tx == ((s_cursor_active == 0U) ? s_cursor_a : s_cursor_b))
                return;

            ui_erase_cursor_layer();
            if (s_cursor_active == 0U) s_cursor_a = (u16)tx;
            else                       s_cursor_b = (u16)tx;
        }
        else
        {
            s32 ty = (s32)y + (s32)s_cursor_grab_y;

            if (ty < PLOT_Y0) ty = PLOT_Y0;
            if (ty > PLOT_Y1) ty = PLOT_Y1;

            if ((u16)ty == ((s_cursor_active == 0U) ? s_cursor_ya : s_cursor_yb))
                return;

            ui_erase_cursor_layer();
            if (s_cursor_active == 0U) s_cursor_ya = (u16)ty;
            else                       s_cursor_yb = (u16)ty;
        }

        ui_draw_wave_area(0U);
        return;
    }

    if (s_page == MENU_PAGE && s_slider_drag)
    {
        u16 raw;

        if (x < SLIDER_X0) x = SLIDER_X0;
        if (x > SLIDER_X1) x = SLIDER_X1;
        raw = (u16)(((u32)(x - SLIDER_X0) * 4095U) / (SLIDER_X1 - SLIDER_X0));
        CMD_SetTrigLevelRaw(raw);
        ui_draw_slider();
    }
}

static void ui_touch_up(void)
{
    s_slider_drag = 0;
    s_cursor_drag = 0;

    /* Plot tap-freeze is persistent; use HOLD to return to live. */
    if (s_cursor_touch_freeze)
    {
        s_cursor_touch_freeze = 0;
        s_frozen = 0;
        CMD_SetAutoPause(0);
        if (s_page == SCOPE_PAGE)
        {
            ui_draw_nav();
            ui_draw_wave_area(1U);
        }
    }
}

static void ui_touch_scan(void)
{
    u8 pressed;

    tp_dev.scan(0);
    pressed = (u8)((tp_dev.sta & TP_PRES_DOWN) ? 1 : 0);

    if (pressed)
    {
        u16 x = tp_dev.x[0];
        u16 y = tp_dev.y[0];

        if (x >= SCR_W || y >= SCR_H) return;

        if (!s_touch_down)
        {
            s_touch_down = 1;
            if (s_page == SCOPE_PAGE) ui_touch_scope_down(x, y);
            else if (s_page == MENU_PAGE) ui_touch_menu_down(x, y);
            else ui_touch_thd_down(x, y);
        }
        else
        {
            ui_touch_move(x, y);
        }
    }
    else if (s_touch_down)
    {
        s_touch_down = 0;
        ui_touch_up();
    }
}

/* ------------------------------------------------------------------ */
/* public                                                             */
/* ------------------------------------------------------------------ */
void UI_Init(void)
{
    u8 expect;

    LCD_Init();
    LCD_Display_Dir(1);
    LCD_Scan_Dir(L2R_U2D);
    LCD_Clear(C_BG);

    BACK_COLOR  = C_BG;
    POINT_COLOR = C_TEXT;

    tp_dev.init();

    /* The stock touch.c loads calibration data without checking which LCD
     * direction it was captured in.  If the EEPROM marker belongs to the
     * other orientation, re-run the four point calibration once. */
    if ((tp_dev.touchtype & 0x80) == 0)
    {
        expect = (u8)((lcddev.dir & 0x01) ? 0x16 : 0x11);
        if (AT24CXX_ReadOneByte(55) != expect)
        {
            if (tp_dev.adjust != 0) tp_dev.adjust();
            LCD_Clear(C_BG);
            BACK_COLOR  = C_BG;
            POINT_COLOR = C_TEXT;
        }
    }

    s_page = SCOPE_PAGE;
    s_frozen = 0;
    CMD_SetAutoPause(0);
    s_slider_drag = 0;
    s_cursor_drag = 0;
    s_cursor_touch_freeze = 0;
    s_last_seq = 0xFFFFFFFFU;
    s_ch_line1[0][0] = 0;
    s_ch_line1[1][0] = 0;
    s_ch_line2[0][0] = 0;
    s_ch_line2[1][0] = 0;
    s_ch_en_cache[0] = 0xFFU;
    s_ch_en_cache[1] = 0xFFU;

    ui_draw_scope_page();
    s_last_seq = CMD_DisplaySeq();
}

void UI_Task(void)
{
    u32 sig;
    u32 psig;
    u32 seq;

    ui_touch_scan();

    if (s_page == SCOPE_PAGE)
    {
        sig = ui_scope_signature();
        psig = ui_param_signature();
        if (sig != s_scope_sig)
        {
            ui_topbar();
            ui_draw_nav();
            ui_draw_wave_area(1U);
            s_scope_sig = sig;
            s_param_sig = psig;
        }
        else if (psig != s_param_sig)
        {
            /* Scale / trigger / type changed: update only the labels,
             * grid and traces.  A full-panel fill here was the source of
             * the visible flicker while auto-range stepped Vdiv. */
            if (!s_frozen)
            {
                ui_draw_wave_dynamic();
                ui_topbar_info();
            }
            s_param_sig = psig;
        }

        seq = CMD_DisplaySeq();
        if (seq != s_last_seq)
        {
            s_last_seq = seq;
            if (!s_frozen)
            {
                ui_draw_wave_dynamic();
                ui_draw_params();
            }
        }
    }
    else if (s_page == THD_PAGE)
    {
        seq = CMD_DisplaySeq();
        if (seq != s_thd_seq)
        {
            if (++s_thd_div >= 5U)
            {
                s_thd_div = 0U;
                s_thd_seq = seq;
                ui_draw_thd_page(0U);
            }
        }
    }

    delay_ms(20);
}
