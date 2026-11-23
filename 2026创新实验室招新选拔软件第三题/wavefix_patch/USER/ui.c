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
#define PLOT_X1     310
#define PLOT_Y0     33
#define PLOT_Y1     143
#define PLOT_W      (PLOT_X1 - PLOT_X0 + 1)
#define PLOT_H      (PLOT_Y1 - PLOT_Y0 + 1)

#define SCOPE_PAGE  0
#define MENU_PAGE   1

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
static const ui_rect R_NAV_HOLD   = {192, 212, 58, 26};
static const ui_rect R_NAV_MENU   = {254, 212, 60, 26};

/* scope top bar badges */
static const ui_rect R_TOP_RUN    = {202,  3, 50, 18};
static const ui_rect R_TOP_MODE   = {256,  3, 58, 18};
static const ui_rect R_AUTO_RANGE = {150,  3, 46, 18};

/* menu page buttons */
static const ui_rect R_BTN_CAL    = {190,  3, 58, 19};
static const ui_rect R_BTN_BACK   = {252,  3, 62, 19};
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

static char s_v_f[16] = "";
static char s_v_vpp[16] = "";
static char s_v_vdc[16] = "";
static char s_v_type[16] = "";

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
    sprintf(buf, "%.2fV", (double)mv / 1000.0);
}

/* time axis label: 0, 0.50ms, 1.00ms ... */
static void ui_fmt_axis_time(u32 us, char *buf)
{
    if (us == 0U)        sprintf(buf, "0");
    else if (us >= 1000U) sprintf(buf, "%.2fms", (double)us / 1000.0);
    else                  sprintf(buf, "%luus", (unsigned long)us);
}

/* ------------------------------------------------------------------ */
/* trigger helpers                                                    */
/* ------------------------------------------------------------------ */
static u16 ui_frame_min(void)
{
    const u16 *f = CMD_DisplayFrame();
    u16 i;
    u16 v = 4095;

    for (i = 0; i < ADC_FRAME_LEN; i++) if (f[i] < v) v = f[i];
    return v;
}

static u16 ui_frame_max(void)
{
    const u16 *f = CMD_DisplayFrame();
    u16 i;
    u16 v = 0;

    for (i = 0; i < ADC_FRAME_LEN; i++) if (f[i] > v) v = f[i];
    return v;
}

static u16 ui_trig_raw(void)
{
    u16 mn;
    u16 mx;

    if (CMD_GetTrigManual()) return CMD_GetTrigLevel();
    if (CMD_DisplaySeq() == 0U) return 2048U;

    mn = ui_frame_min();
    mx = ui_frame_max();
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

    ui_fill(0, 0, SCR_W - 1, TOP_H - 1, C_PANEL);
    ui_cn(8, 4, CN_SCOPE, C_TEXT, C_PANEL);
    ui_text(68, 7, 80, 14, 12, C_MUTED, C_PANEL, "CH1 PA4");

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

/* ------------------------------------------------------------------ */
/* scope page                                                         */
/* ------------------------------------------------------------------ */
static void ui_draw_wave_area(void)
{
    u16 i;
    u16 x;
    u16 y;
    u16 level_y;
    u16 level_raw;
    u16 vdiv_mv = CMD_GetVdivMv();
    u16 vcenter_mv = CMD_GetVcenterMv();
    u32 tb = CMD_GetTimebaseUs();
    float vdiv;
    char buf[12];
    const u16 *frame;
    u16 trig;
    u16 span;
    u16 px;
    u16 mn;
    u16 mx;
    float mid;

    if (vdiv_mv < 20U) vdiv_mv = 20U;
    vdiv = (float)vdiv_mv / 1000.0f;

    span = CMD_DisplaySpan();
    if (span < 16U) span = 16U;
    if (span > ADC_FRAME_LEN) span = ADC_FRAME_LEN;

    /* panel background + grid */
    ui_fill(PAN_X0, PAN_Y0, PAN_X1, PAN_Y1, C_BG);

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

    /* voltage labels (left side) */
    mid = (float)vcenter_mv / 1000.0f;
    for (i = 0; i <= 4U; i++)
    {
        float v = mid + 4.0f * vdiv - (float)i * 2.0f * vdiv;
        if (v >= 10.0f || v <= -10.0f) sprintf(buf, "%.1f", (double)v);
        else                           sprintf(buf, "%.2f", (double)v);
        y = (u16)(PLOT_Y0 + ((u32)i * (PLOT_H - 1U)) / 4U);
        if (y >= 6U) y -= 6U;
        ui_text(7, y, 32, 13, 12, C_MUTED, C_BG, buf);
    }

    /* time labels (bottom, every 2 divisions; last one shortened so it
     * does not collide with the 8-division label) */
    for (i = 0; i <= 5U; i++)
    {
        u32 t = (u32)i * 2U * tb;
        u16 tw;
        u16 tx;

        if (i == 5U)
        {
            if (t >= 1000U) sprintf(buf, "%.0fms", (double)t / 1000.0);
            else            sprintf(buf, "%luus", (unsigned long)t);
        }
        else
        {
            ui_fmt_axis_time(t, buf);
        }

        tw = ui_str_w(buf, 12);
        x = (u16)(PLOT_X0 + ((u32)i * (PLOT_W - 1U)) / 5U);
        tx = (x > (u16)(tw / 2U)) ? (u16)(x - tw / 2U) : 2U;
        if (tx > (u16)(PAN_X1 - tw - 2U)) tx = (u16)(PAN_X1 - tw - 2U);
        ui_text(tx, 151, (u16)(tw + 2U), 14, 12, C_MUTED, C_BG, buf);
    }

    /* trigger level: dashed orange line */
    level_raw = ui_trig_raw();
    level_y = ui_raw_to_y(level_raw, vdiv, mid);
    for (x = PLOT_X0; x < (u16)(PLOT_X1 - 2U); x = (u16)(x + 8U))
    {
        ui_fill(x, level_y, (u16)(x + 3U), level_y, C_ACCENT);
    }
    ui_text((u16)(PLOT_X1 - 10U), (u16)(level_y > 14U ? level_y - 14U : PLOT_Y0),
            12, 14, 12, C_ACCENT, C_BG, "T");

    /* waveform, peak-detect per pixel column */
    if (CMD_DisplaySeq() != 0U)
    {
        frame = CMD_DisplayFrame();
        trig = CMD_DisplayTrig();

        if (span < PLOT_W)
        {
            /* When only a few samples cover the screen, connect the
             * samples directly. Per-column peak hold would otherwise
             * duplicate the same sample and create visible stairs. */
            u16 k;
            u16 xp = 0, yp = 0;

            POINT_COLOR = C_VALUE;
            for (k = 0; k < span; k++)
            {
                u32 idx = (u32)trig + k;
                u16 v;
                u16 xn;
                u16 yn;

                if (idx >= ADC_FRAME_LEN) idx -= ADC_FRAME_LEN;
                v = frame[idx];
                xn = (u16)(PLOT_X0 + ((u32)k * (PLOT_W - 1U)) / (span - 1U));
                yn = ui_raw_to_y(v, vdiv, mid);

                if (k != 0U) LCD_DrawLine(xp, yp, xn, yn);
                xp = xn;
                yp = yn;
            }
        }
        else
        {
            for (px = 0; px < PLOT_W; px++)
            {
                u32 s0 = (u32)trig + ((u32)px * span) / PLOT_W;
                u32 s1 = (u32)trig + ((u32)(px + 1U) * span) / PLOT_W;
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

                ytop = ui_raw_to_y(mx, vdiv, mid);
                ybot = ui_raw_to_y(mn, vdiv, mid);
                if (ytop > ybot)
                {
                    u16 t = ytop; ytop = ybot; ybot = t;
                }
                x = (u16)(PLOT_X0 + px);
                ui_fill(x, ytop, x, ybot, C_VALUE);
            }
        }
    }

    /* panel frame */
    ui_frame_round(PAN_X0, PAN_Y0, PAN_X1, PAN_Y1, 7, C_BORDER);

    /* frozen: dual cursors + delta-T readout */
    if (s_frozen)
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
        ui_text(ca, PLOT_Y0 + 2, 10, 14, 12, C_EDIT, C_BG, "A");
        ui_text(cb, PLOT_Y0 + 2, 10, 14, 12, C_EDIT, C_BG, "B");

        if (ca > cb) { u16 t = ca; ca = cb; cb = t; }
        dt_us = (u32)(cb - ca) * win_us / (PLOT_W - 1U);
        if (dt_us == 0U) dt_us = 1U;
        f_hz = 1000000UL / dt_us;

        if (dt_us >= 1000U) sprintf(buf, "%.3fms", (double)dt_us / 1000.0);
        else                sprintf(buf, "%luus", (unsigned long)dt_us);

        ui_fill_round((u16)(PLOT_X1 - 176U), PLOT_Y0 + 2,
                      (u16)(PLOT_X1 - 2U), (u16)(PLOT_Y0 + 17U),
                      5, C_PANEL);
        sprintf(buf, "dT %.3fms F %.2fkHz", (double)dt_us / 1000.0,
                (double)f_hz / 1000.0);
        ui_text((u16)(PLOT_X1 - 172U), PLOT_Y0 + 3, 170, 14, 12,
                C_EDIT, C_PANEL, buf);
    }
}

static void ui_draw_card(const ui_rect *r, const char *label_cn,
                         const char *value, u8 value_is_cn, u16 fg)
{
    ui_fill_round(r->x, r->y, (u16)(r->x + r->w - 1),
                  (u16)(r->y + r->h - 1), 5, C_PANEL);

    if (label_cn != 0)
    {
        ui_cn_c(r->x, (u16)(r->y + 1U), r->w, label_cn, C_MUTED, C_PANEL);
    }

    if (value_is_cn)
    {
        ui_cn_c(r->x, (u16)(r->y + 18U), r->w, value, fg, C_PANEL);
    }
    else
    {
        ui_text_c(r->x, (u16)(r->y + 18U), r->w, 18, 16, fg, C_PANEL, value);
    }
}

static void ui_draw_params(void)
{
    MEAS_Result m;
    char bf[16];
    char bv[16];
    char bd[16];
    char bt[16];
    static const ui_rect R_F   = {  6, 174, 92, 34};
    static const ui_rect R_VPP = {102, 174, 72, 34};
    static const ui_rect R_VDC = {178, 174, 72, 34};
    static const ui_rect R_TYP = {254, 174, 62, 34};

    if (CMD_DisplaySeq() == 0U)
    {
        strcpy(bf, "--");
        strcpy(bv, "--");
        strcpy(bd, "--");
        strcpy(bt, "--");
    }
    else
    {
        MEAS_Compute(CMD_DisplayFrame(), ADC_FRAME_LEN,
                     (float)ADC1_GetSampleRate(), &m);

        if (m.freq >= 1000.0f) sprintf(bf, "%.2fkHz", (double)m.freq / 1000.0);
        else                   sprintf(bf, "%.1fHz", (double)m.freq);
        sprintf(bv, "%.2fV", (double)m.vpp);
        sprintf(bd, "%.2fV", (double)m.dc);
        strcpy(bt, ui_type_short(m.type));
    }

    if (strcmp(bf, s_v_f) != 0)
    {
        strcpy(s_v_f, bf);
        ui_draw_card(&R_F, CN_FREQ, bf, 0, C_VALUE);
    }
    if (strcmp(bv, s_v_vpp) != 0)
    {
        strcpy(s_v_vpp, bv);
        ui_draw_card(&R_VPP, CN_VPP, bv, 0, C_VALUE);
    }
    if (strcmp(bd, s_v_vdc) != 0)
    {
        strcpy(s_v_vdc, bd);
        ui_draw_card(&R_VDC, CN_VDC, bd, 0, C_VALUE);
    }
    if (strcmp(bt, s_v_type) != 0)
    {
        strcpy(s_v_type, bt);
        ui_draw_card(&R_TYP, CN_TYPE, bt, 1,
                     (bt[0] == '-') ? C_MUTED : C_OK);
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
    ui_cn_button(&R_NAV_EDGE, CMD_GetEdgeRise() ? CN_RISE : CN_FALL,
                 C_PANEL, C_BORDER, C_TEXT);
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
    sig |= ((u32)CMD_GetTrigManual()) << 6;
    sig |= ((u32)(CMD_GetTimebaseUs() / 100U)) << 8;
    sig |= ((u32)(CMD_GetVdivMv() / 100U)) << 16;
    sig |= ((u32)((CMD_GetAutoTimebase() || CMD_GetAutoVdiv()) ? 1 : 0)) << 22;
    sig |= ((u32)(CMD_GetTrigLevel() / 16U)) << 24;

    return sig;
}

static void ui_draw_scope_page(void)
{
    ui_fill(0, 0, SCR_W - 1, SCR_H - 1, C_BG);
    ui_topbar();
    ui_draw_wave_area();
    ui_draw_params();
    ui_draw_nav();
    s_scope_sig = ui_scope_signature();
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
    ui_cn_button(&R_BTN_CAL,  CN_CAL,  C_PANEL_HI, C_BORDER, C_TEXT);
    ui_cn_button(&R_BTN_BACK, CN_BACK, C_PANEL_HI, C_BORDER, C_TEXT);
    ui_fill(0, TOP_H - 1, SCR_W - 1, TOP_H - 1, C_ACCENT);

    /* status line */
    ui_fmt_tb(CMD_GetTimebaseUs(), tb);
    ui_fmt_vd(CMD_GetVdivMv(), vd);
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
static void ui_touch_scope_down(u16 x, u16 y)
{
    if (ui_hit(&R_AUTO_RANGE, x, y))
    {
        CMD_ToggleAutoRange();
        ui_topbar();
        ui_draw_wave_area();
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
        ui_draw_wave_area();
        return;
    }
    if (ui_hit(&R_NAV_EDGE, x, y))
    {
        CMD_CycleEdge();
        ui_draw_nav();
        ui_draw_wave_area();
        return;
    }
    if (ui_hit(&R_NAV_HOLD, x, y))
    {
        s_frozen = (u8)(s_frozen ? 0 : 1);
        CMD_SetAutoPause(s_frozen);
        if (s_frozen)
        {
            s_cursor_a = (u16)(PLOT_X0 + PLOT_W / 4U);
            s_cursor_b = (u16)(PLOT_X0 + (PLOT_W * 3U) / 4U);
        }
        ui_draw_nav();
        ui_draw_wave_area();
        return;
    }
    if (ui_hit(&R_NAV_MENU, x, y))
    {
        ui_enter_menu();
        return;
    }

    /* plot area: tap freezes; while frozen moves the nearest cursor */
    if (x >= PLOT_X0 && x <= PLOT_X1 && y >= PLOT_Y0 && y <= PLOT_Y1)
    {
        if (!s_frozen)
        {
            s_frozen = 1;
            CMD_SetAutoPause(1);
            s_cursor_a = (u16)(PLOT_X0 + PLOT_W / 4U);
            s_cursor_b = (u16)(PLOT_X0 + (PLOT_W * 3U) / 4U);
        }
        else
        {
            u16 da = (u16)((x > s_cursor_a) ? (x - s_cursor_a) : (s_cursor_a - x));
            u16 db = (u16)((x > s_cursor_b) ? (x - s_cursor_b) : (s_cursor_b - x));

            if (da <= db) s_cursor_a = x;
            else          s_cursor_b = x;
        }
        ui_draw_nav();
        ui_draw_wave_area();
    }
}

static void ui_touch_menu_down(u16 x, u16 y)
{
    const ui_rect *r;

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

static void ui_touch_move(u16 x, u16 y)
{
    (void)y;

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
            else                      ui_touch_menu_down(x, y);
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
    s_last_seq = 0xFFFFFFFFU;
    s_v_f[0] = 0;
    s_v_vpp[0] = 0;
    s_v_vdc[0] = 0;
    s_v_type[0] = 0;

    ui_draw_scope_page();
    s_last_seq = CMD_DisplaySeq();
}

void UI_Task(void)
{
    u32 sig;
    u32 seq;

    ui_touch_scan();

    if (s_page == SCOPE_PAGE)
    {
        sig = ui_scope_signature();
        if (sig != s_scope_sig)
        {
            ui_topbar();
            ui_draw_nav();
            ui_draw_wave_area();
            s_scope_sig = sig;
        }

        seq = CMD_DisplaySeq();
        if (seq != s_last_seq)
        {
            s_last_seq = seq;
            if (!s_frozen)
            {
                ui_draw_wave_area();
                ui_draw_params();
            }
        }
    }

    delay_ms(20);
}
