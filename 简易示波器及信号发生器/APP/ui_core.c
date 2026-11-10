#include "ui_core.h"
#include "ui_pages.h"
#include "app_config.h"
#include "bsp_tick.h"
#include "tftlcd.h"
#include "touch.h"
#include <stdlib.h>

/*==============================================================================
 * ui_core.c
 *-----------------------------------------------------------------------------
 * ????????"????", ????"????????":
 *   - ?????? -> ????? DOWN / MOVE / UP ???
 *   - ?????????, ??????
 *   - ???? + ?????? (???????????)
 *   - ????????/????/?????С????, ?????????/?????
 *   - ???????/??????????????
 *
 * ???????沼??????λ?á??????, ?? ui_pages.c ????????д??
 *============================================================================*/

UI_Keypad g_keypad;
volatile u8 g_ui_go_boot = 0;
volatile u8 g_ui_drag_lock = 0;
volatile u8 g_ui_full_redraw = 0;

const char *const g_ui_keypad_labels[UI_KEYPAD_COUNT] =
{
    "1", "2", "3",
    "4", "5", "6",
    "7", "8", "9",
    "DEL", "0", "OK"
};

static UI_PageId s_page_id = PAGE_PWM;
static u8  s_inited = 0;
static u8  s_dirty = 1;
static u32 s_last_touch_ms = 0;
static u32 s_last_draw_ms = 0;

/* ??????: BACK ????????, NEXT ?????????????? */
#define UI_HIST_MAX 8
static UI_PageId s_page_hist[UI_HIST_MAX];
static u8 s_hist_len = 0;

static const UI_PageId s_page_order[] =
{
    PAGE_PWM,
    PAGE_CAPTURE,
    PAGE_STOPWATCH,
    PAGE_SWEEP,
    PAGE_SPWM,
    PAGE_SNAKE
};
#define UI_PAGE_ORDER_COUNT (sizeof(s_page_order) / sizeof(s_page_order[0]))

static u8  s_touch_down = 0;
static u16 s_last_x = 0;
static u16 s_last_y = 0;
static u16 s_down_x = 0;
static u16 s_down_y = 0;
static u8  s_keypad_full = 0;    /* ??????/?????????????? */
static u8  s_nav_visible = 1;    /* 0=底部 BACK/NEXT 导航条隐藏(贪吃蛇全屏) */

/*------------------------------ ????/??ι??? ------------------------------*/
void UI_TextColor(u16 x, u16 y, u16 w, u16 h, u8 size,
                  u16 fg, u16 bg, const char *s)
{
    FRONT_COLOR = fg;
    BACK_COLOR  = bg;
    LCD_ShowString(x, y, w, h, size, (u8 *)s);
}

void UI_NumColor(u16 x, u16 y, u32 num, u8 len, u8 size,
                 u16 fg, u16 bg)
{
    FRONT_COLOR = fg;
    BACK_COLOR  = bg;
    LCD_ShowNum(x, y, num, len, size);
}

/* ??λ??0???????????????????"?????????"????,
 * ??? LCD_ShowNum ???λ??????, ??????????? */
void UI_NumPadColor(u16 x, u16 y, u32 num, u8 len, u8 size,
                    u16 fg, u16 bg)
{
    FRONT_COLOR = fg;
    BACK_COLOR  = bg;
    LCD_ShowxNum(x, y, num, len, size, 0x80);
}

void UI_Fill(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
    if (x1 > x2) { u16 t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { u16 t = y1; y1 = y2; y2 = t; }
    if (x2 >= UI_SCREEN_W) x2 = UI_SCREEN_W - 1;
    if (y2 >= UI_SCREEN_H) y2 = UI_SCREEN_H - 1;

    LCD_Fill(x1, y1, x2, y2, color);
}

void UI_Frame(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
    if (x1 > x2) { u16 t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { u16 t = y1; y1 = y2; y2 = t; }
    if (x2 >= UI_SCREEN_W) x2 = UI_SCREEN_W - 1;
    if (y2 >= UI_SCREEN_H) y2 = UI_SCREEN_H - 1;

    FRONT_COLOR = color;
    LCD_DrawRectangle(x1, y1, x2, y2);
}

u8 UI_HitTest(const UI_Rect *r, u16 x, u16 y)
{
    if (r == 0) return 0;
    if (x < r->x || x >= (u16)(r->x + r->w)) return 0;
    if (y < r->y || y >= (u16)(r->y + r->h)) return 0;
    return 1;
}

/*----------------------------------------------------------------------------
 * TODO-10 (??С?汾?????, ?????????)
 * ??e??: ??? + ??? + ????????, pressed ??????
 * ??????????:
 *   - ???? UI_Fill ???, ?? UI_Frame ????, ??? UI_TextColor д??;
 *   - ??????????????????, ???????????(???);
 *   - ?????????????????????
 *----------------------------------------------------------------------------*/
void UI_DrawButton(const UI_Rect *r, const char *label, u8 pressed)
{
    u8  len = 0;
    u8  font;
    u16 tw;
    u16 tx;
    u16 ty;
    u16 bg;
    u16 fg;
    u16 border;

    if (r == 0 || label == 0) return;

    while (label[len] != '\0') len++;

    /* ??? 16 ??????, ???????? 12 ???? */
    font = UI_FONT_M;
    if ((u16)len * (UI_FONT_M / 2) > (u16)(r->w - 8))
    {
        font = UI_FONT_S;
    }
    tw = (u16)len * (font / 2);
    if (tw > (u16)(r->w - 4)) tw = (u16)(r->w - 4);
    tx = (u16)(r->x + (r->w - tw) / 2);
    ty = (u16)(r->y + (r->h - font) / 2);

    if (pressed)
    {
        bg = UI_COLOR_ACCENT;
        fg = UI_COLOR_BG;
        border = UI_COLOR_TEXT;
    }
    else
    {
        bg = UI_COLOR_PANEL;
        fg = UI_COLOR_TEXT;
        border = UI_COLOR_BORDER;
    }

    UI_Fill(r->x, r->y, (u16)(r->x + r->w - 1), (u16)(r->y + r->h - 1), bg);
    UI_Frame(r->x, r->y, (u16)(r->x + r->w - 1), (u16)(r->y + r->h - 1), border);
    UI_TextColor(tx, ty, r->w, font, font, fg, bg, label);
}

/*----------------------------------------------------------------------------
 * TODO-11 (??С?汾?????, ?????????)
 * ????????: ???? + ????????????? BACK + ?????????
 * У???? BOOT ?????????, ?????治??? CAL??
 * ??? UI_STATUS_H, ?? UI_COLOR_PANEL ????
 *----------------------------------------------------------------------------*/
void UI_DrawStatusBar(const char *title)
{
    UI_Fill(0, 0, UI_SCREEN_W - 1, UI_STATUS_H - 1, UI_COLOR_PANEL);

    /* ??????????; BACK/NEXT ???????????? */
    UI_TextColor(6, 2, (u16)(UI_SCREEN_W - 12),
                 UI_STATUS_H - 4, UI_FONT_M,
                 UI_COLOR_TEXT, UI_COLOR_PANEL, title);

    /* ???????????, ?????????????????? */
    UI_Fill(0, UI_STATUS_H - 1, UI_SCREEN_W - 1, UI_STATUS_H - 1, UI_COLOR_ACCENT);
}

/*------------------------------ ????????? --------------------------------*/
#define UI_NAV_Y 286
static const UI_Rect s_nav_back = { 4, 290, 78, 26 };
static const UI_Rect s_nav_next = { 158, 290, 78, 26 };
static u8 s_nav_pressed = 0;       /* 0=??, 1=BACK, 2=NEXT */

static void UI_DrawNavBar(void)
{
    if (!s_nav_visible) return;

    UI_Fill(0, UI_NAV_Y, UI_SCREEN_W - 1, UI_SCREEN_H - 1, UI_COLOR_PANEL);
    UI_Fill(0, UI_NAV_Y, UI_SCREEN_W - 1, UI_NAV_Y, UI_COLOR_BORDER);

    UI_DrawButton(&s_nav_back, "BACK", 0);
    UI_DrawButton(&s_nav_next, "NEXT", 0);
}

/*------------------------------ ?????? ----------------------------------*/
void UI_MarkDirty(void)
{
    /* ????????????????????????, ?????????????? */
    if (!g_ui_drag_lock) s_dirty = 1;
}

UI_PageId UI_GetPage(void)
{
    return s_page_id;
}

static void UI_SetPage(UI_PageId id)
{
    if (id >= PAGE_COUNT) return;

    if (s_inited && g_ui_pages[s_page_id].leave != 0)
    {
        g_ui_pages[s_page_id].leave();
    }

    s_page_id = id;
    s_nav_pressed = 0;
    s_nav_visible = 1;              /* 每个页面默认显示导航条, enter 里可隐藏 */
    g_ui_drag_lock = 0;
    g_ui_full_redraw = 0;

    /* ??????????????????????; ?????????????????塣 */
    LCD_Clear(UI_COLOR_BG);

    if (g_ui_pages[s_page_id].enter != 0)
    {
        g_ui_pages[s_page_id].enter();
    }

    s_inited = 1;
    s_dirty  = 1;
}

void UI_GoPage(UI_PageId id)
{
    if (id >= PAGE_COUNT || id == s_page_id) return;

    if (s_hist_len < UI_HIST_MAX)
    {
        s_page_hist[s_hist_len++] = s_page_id;
    }
    UI_SetPage(id);
}

/* 清空返回历史后切页。给全屏彩蛋的 BACK 用:
 * 退出贪吃蛇后回到 PWM 主界面, 再按 BACK 就回开始界面, 不会又掉进游戏。 */
void UI_GoRoot(UI_PageId id)
{
    if (id >= PAGE_COUNT) return;

    s_hist_len = 0;

    if (id == s_page_id)
    {
        UI_MarkDirty();
        return;
    }
    UI_SetPage(id);
}

/* 显示/隐藏底部 BACK/NEXT 导航条。贪吃蛇页面进入时隐藏, 换页时
 * UI_SetPage 会自动恢复成 1, 所以其他页面不受影响。 */
void UI_NavSetVisible(u8 visible)
{
    visible = visible ? 1 : 0;
    if (s_nav_visible == visible) return;

    s_nav_visible = visible;
    UI_MarkDirty();
}

static void UI_BackPage(void)
{
    if (s_hist_len > 0)
    {
        UI_SetPage(s_page_hist[--s_hist_len]);
    }
    else if (s_page_id == PAGE_PWM)
    {
        g_ui_go_boot = 1;
    }
    else
    {
        UI_SetPage(PAGE_PWM);
    }
}

static void UI_NextPage(void)
{
    u8 i;
    UI_PageId next;

    for (i = 0; i < UI_PAGE_ORDER_COUNT; i++)
    {
        if (s_page_order[i] == s_page_id) break;
    }

    if (i < UI_PAGE_ORDER_COUNT)
    {
        next = s_page_order[(i + 1) % UI_PAGE_ORDER_COUNT];
    }
    else
    {
        next = PAGE_PWM;
    }

    UI_GoPage(next);
}

/*------------------------------ ??????? ----------------------------------*/
static void UI_Dispatch(UI_Event *ev)
{
    /* ?????????????, ??????????? */
    if (g_keypad.active)
    {
        UI_Keypad_OnTouch(ev);
        return;
    }

    /* ??????????: ???????????涼??·, ?????????????? */
    if (g_ui_drag_lock)
    {
        if (g_ui_pages[s_page_id].on_event != 0)
        {
            g_ui_pages[s_page_id].on_event(ev);
        }
        return;
    }

    /* ?????????: ??????, ???????????????????,
     * ????????????????????, ???????????????? */
    if (s_nav_visible)
    {
        u8 back_hit = UI_HitTest(&s_nav_back, ev->x, ev->y);
        u8 next_hit = UI_HitTest(&s_nav_next, ev->x, ev->y);

        if (ev->type == UI_EV_DOWN)
        {
            if (back_hit)
            {
                s_nav_pressed = 1;
                return;
            }
            if (next_hit)
            {
                s_nav_pressed = 2;
                return;
            }
        }
        else if (s_nav_pressed != 0)
        {
            if (ev->type == UI_EV_UP)
            {
                u8 which = s_nav_pressed;
                s_nav_pressed = 0;

                if (which == 1 && back_hit) UI_BackPage();
                else if (which == 2 && next_hit) UI_NextPage();
                return;
            }
            else if (ev->type == UI_EV_MOVE &&
                     ((s_nav_pressed == 1 && !back_hit) ||
                      (s_nav_pressed == 2 && !next_hit)))
            {
                s_nav_pressed = 0;
            }
        }
    }

    if (g_ui_pages[s_page_id].on_event != 0)
    {
        g_ui_pages[s_page_id].on_event(ev);
    }
}

static void UI_TouchPoll(void)
{
    u8 pressed = TP_Scan(0);   /* 0=??????????? */
    u16 x;
    u16 y;

    if (pressed)
    {
        x = tp_dev.x[0];
        y = tp_dev.y[0];

        /* ??????????????? 0xFFFF, ????????????; ???????? */
        if (x == 0xFFFF || y == 0xFFFF) return;
        if (x >= UI_SCREEN_W || y >= UI_SCREEN_H) return;

        if (!s_touch_down)
        {
            s_touch_down = 1;
            s_down_x = x;
            s_down_y = y;
            s_last_x = x;
            s_last_y = y;

            /* ???"?°???"???, ?????????趼????????°??? */
            tp_dev.sta &= ~TP_CATH_PRES;

            {
                UI_Event ev;
                ev.type   = UI_EV_DOWN;
                ev.x      = x;
                ev.y      = y;
                ev.down_x = s_down_x;
                ev.down_y = s_down_y;
                ev.ms     = TICK_Now();
                UI_Dispatch(&ev);
            }
        }
        else
        {
            int dx = (int)x - (int)s_last_x;
            int dy = (int)y - (int)s_last_y;

            if (abs(dx) >= UI_MOVE_THRESHOLD_PX || abs(dy) >= UI_MOVE_THRESHOLD_PX)
            {
                s_last_x = x;
                s_last_y = y;

                {
                    UI_Event ev;
                    ev.type   = UI_EV_MOVE;
                    ev.x      = x;
                    ev.y      = y;
                    ev.down_x = s_down_x;
                    ev.down_y = s_down_y;
                    ev.ms     = TICK_Now();
                    UI_Dispatch(&ev);
                }
            }
        }
    }
    else if (s_touch_down)
    {
        s_touch_down = 0;

        {
            UI_Event ev;
            ev.type   = UI_EV_UP;
            ev.x      = s_last_x;
            ev.y      = s_last_y;
            ev.down_x = s_down_x;
            ev.down_y = s_down_y;
            ev.ms     = TICK_Now();
            UI_Dispatch(&ev);
        }
    }

#if UI_BRINGUP_TOUCH_DOT
    /* ??????: ??????????С???, ??????????
     * ?????o??, ????????? UI_BRINGUP_TOUCH_DOT ???0?? */
    if (pressed)
    {
        UI_Fill(x > 2 ? x - 2 : 0, y, x < UI_SCREEN_W - 3 ? x + 2 : UI_SCREEN_W - 1, y, UI_COLOR_ACCENT);
        UI_Fill(x, y > 2 ? y - 2 : 0, x, y < UI_SCREEN_H - 3 ? y + 2 : UI_SCREEN_H - 1, UI_COLOR_ACCENT);
    }
#endif
}

/*------------------------------ ?????/????? -----------------------------*/
void UI_InitAt(UI_PageId id)
{
    if (id >= PAGE_COUNT) id = PAGE_PWM;

    s_hist_len      = 0;
    s_page_id       = id;
    s_inited        = 0;
    s_dirty         = 1;
    s_last_touch_ms = 0;
    s_last_draw_ms  = 0;
    s_touch_down    = 0;
    s_last_x = s_last_y = 0;
    s_down_x = s_down_y = 0;

    g_keypad.active = 0;
    g_ui_go_boot = 0;
    g_ui_drag_lock = 0;

    UI_SetPage(id);
}

void UI_Init(void)
{
    UI_InitAt(PAGE_PWM);
}

void UI_Loop(void)
{
    u32 now = TICK_Now();

    /* 1. ???????: ????????????? SPI + ??? AD + ????, ???κ???????,
     *    ???????????????, ?? UI_TOUCH_SCAN_MS ?????? */
    if ((u32)(now - s_last_touch_ms) >= UI_TOUCH_SCAN_MS)
    {
        s_last_touch_ms = now;
        UI_TouchPoll();
    }

    /* 2. ???????????????? (???/?????/?????????????) */
    if (g_ui_pages[s_page_id].tick != 0)
    {
        g_ui_pages[s_page_id].tick(now);
    }

    /* 3. ???? + ???????; ?????????, ????????? */
    if (s_dirty && (u32)(now - s_last_draw_ms) >= UI_REFRESH_MS)
    {
        s_last_draw_ms = now;
        s_dirty = 0;

        /* ???????????????, ?????????????????????? */
        if (g_keypad.active)
        {
            UI_Keypad_Draw();
        }
        else
        {
            if (g_ui_pages[s_page_id].draw != 0)
            {
                g_ui_pages[s_page_id].draw();
            }
            UI_DrawNavBar();
        }
    }
}

/*------------------------------ ??????? ----------------------------------*/
void UI_Keypad_Open(u8 target, int32_t initial, u8 max_digits,
                    void (*on_confirm)(int32_t value))
{
    g_keypad.active     = 1;
    g_keypad.target     = target;
    g_keypad.digits     = 0;
    g_keypad.max_digits = max_digits;
    g_keypad.value      = initial;
    g_keypad.on_confirm = on_confirm;
    s_keypad_full       = 0;
    UI_MarkDirty();
}

void UI_Keypad_Close(u8 commit)
{
    if (!g_keypad.active) return;

    if (commit && g_keypad.on_confirm != 0)
    {
        g_keypad.on_confirm(g_keypad.value);
    }

    g_keypad.active = 0;
    s_keypad_full   = 0;
    g_ui_full_redraw = 1;   /* ?????????????????? */
    UI_MarkDirty();
}

/*----------------------------------------------------------------------------
 * TODO-12 (????????: ???????, ?????)
 *
 * ?? 3x4 ????:
 *   ????? ~70x40, ??????????????; ??????? 2~4px;
 *   ?? UI_DrawButton ??; ????? pressed=1 ???, ??????;
 *   ????????????? g_keypad.value, ?? UI_COLOR_EDIT ???
 *
 * ??????? UI_Keypad_OnTouch:
 *   - ??? UI_EV_DOWN ????? (?? DOWN ???? + UP ??, ????, ????);
 *   - ?????: value = value*10 + digit, ???? max_digits ????;
 *   - DEL: value /= 10;
 *   - OK: UI_Keypad_Close(1);
 *   - ?????????: ???? UI_Keypad_Close(0) ?????
 *   - ??θ?? UI_MarkDirty()??
 *----------------------------------------------------------------------------*/
/* 3x4 ????: 10,110 ??, ??? 70x44, ?м?? 5, ?м?? 6 */
static void UI_Keypad_Rect(u8 index, UI_Rect *r)
{
    u8 col = (u8)(index % 3);
    u8 row = (u8)(index / 3);

    r->x = (u16)(10 + col * 75);
    r->y = (u16)(68 + row * 50);
    r->w = 70;
    r->h = 44;
}

void UI_Keypad_Draw(void)
{
    u8 i;
    UI_Rect r;

    if (!s_keypad_full)
    {
        /* ?????????????ε??????а??? */
        UI_Fill(0, UI_STATUS_H, UI_SCREEN_W - 1, UI_SCREEN_H - 1, UI_COLOR_BG);
        UI_Fill(0, 24, UI_SCREEN_W - 1, 64, UI_COLOR_PANEL);

        for (i = 0; i < UI_KEYPAD_COUNT; i++)
        {
            UI_Keypad_Rect(i, &r);
            UI_DrawButton(&r, g_ui_keypad_labels[i], 0);
        }
        s_keypad_full = 1;
    }
    else
    {
        /* ????仯??????????????, ??????? */
        UI_Fill(0, 24, UI_SCREEN_W - 1, 64, UI_COLOR_PANEL);
    }

    UI_TextColor(8, 26, 120, 12, UI_FONT_S,
                 UI_COLOR_MUTED, UI_COLOR_PANEL, "ENTER VALUE");
    UI_NumColor(8, 38, (u32)((g_keypad.value < 0) ? 0 : g_keypad.value),
                g_keypad.max_digits, UI_FONT_L,
                UI_COLOR_EDIT, UI_COLOR_PANEL);
}

u8 UI_Keypad_OnTouch(const UI_Event *ev)
{
    u8 i;
    u8 digit;
    UI_Rect r;

    if (ev->type != UI_EV_DOWN) return 1;

    for (i = 0; i < UI_KEYPAD_COUNT; i++)
    {
        UI_Keypad_Rect(i, &r);
        if (!UI_HitTest(&r, ev->x, ev->y)) continue;

        if (i == UI_KEY_OK)
        {
            UI_Keypad_Close(1);
            return 1;
        }

        if (i == UI_KEY_DEL)
        {
            if (g_keypad.digits > 0)
            {
                g_keypad.value /= 10;
                g_keypad.digits--;
            }
            else
            {
                g_keypad.value = 0;
            }
            UI_MarkDirty();
            return 1;
        }

        digit = (i == 10) ? 0 : (u8)(i + 1);   /* 0 ????????10 */

        if (g_keypad.digits < g_keypad.max_digits)
        {
            if (g_keypad.digits == 0)
            {
                g_keypad.value = digit;       /* ?????????滻???? */
            }
            else
            {
                g_keypad.value = g_keypad.value * 10 + digit;
            }
            g_keypad.digits++;
            UI_MarkDirty();
        }
        return 1;
    }

    /* ?????????: ??? */
    UI_Keypad_Close(0);
    return 1;
}

