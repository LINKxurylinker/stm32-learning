#include "ui_core.h"

#include "ui_pages.h"

#include "ui_hanzi.h"

#include "app_config.h"

#include "bsp_tick.h"

#include "tftlcd.h"

#include "touch.h"

#include <stdlib.h>



/*==============================================================================

 * ui_core.c

 *-----------------------------------------------------------------------------

 * 这一层只做"机制", 不做"界面内容":

 *   - 读触摸 -> 翻译成 DOWN / MOVE / UP 事件

 *   - 维护当前页面, 转发事件

 *   - 脏标记 + 定时重绘 (不是每轮都重画)

 *   - 提供画文字/矩形/数字的小工具, 统一处理前景/背景色

 *   - 数字键盘的状态与接口

 *

 * 具体的页面布局、按钮位置、要画什么, 在 ui_pages.c 里留白给你写。

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

    "删除", "0", "确定"

};



static UI_PageId s_page_id = PAGE_PWM;

static u8  s_inited = 0;

static u8  s_dirty = 1;

static u32 s_last_touch_ms = 0;

static u32 s_last_draw_ms = 0;



/* 页面历史: BACK 返回上一页, NEXT 按固定顺序切下一页 */

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

    PAGE_MAZE,

    PAGE_SNAKE

};

#define UI_PAGE_ORDER_COUNT (sizeof(s_page_order) / sizeof(s_page_order[0]))



static u8  s_touch_down = 0;

static u16 s_last_x = 0;

static u16 s_last_y = 0;

static u16 s_down_x = 0;

static u16 s_down_y = 0;

static u8  s_keypad_full = 0;    /* 键盘底板/按键是否已经画过 */

static u8  s_nav_visible = 1;    /* 0=底部 BACK/NEXT 导航条隐藏(全屏游戏) */



/*------------------------------ 文字/图形工具 ------------------------------*/

static u8 UI_TextHasChinese(const char *s)

{

    while (*s != '\0')

    {

        if ((u8)*s >= 0x80) return 1;

        s++;

    }

    return 0;

}



static u16 UI_TextWidth(const char *s, u8 ascii_size)

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



void UI_TextColor(u16 x, u16 y, u16 w, u16 h, u8 size,

                  u16 fg, u16 bg, const char *s)

{

    if (UI_TextHasChinese(s))

    {

        UI_HZ16_Text(x, y, s, fg, bg);

        return;

    }



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



/* 高位补0的固定宽度数字。用于需要"擦掉旧数字"的地方,

 * 因为 LCD_ShowNum 对高位是画空格, 宽度也是固定的。 */

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




/*------------------------------ 圆角/圆形工具 ------------------------------*/
/* 整数平方根: 只用于半径 0..40 的圆角/圆, 线性循环足够快 */
static u16 UI_Isqrt(u16 v)
{
    u16 r = 0;

    while ((u32)(r + 1) * (u32)(r + 1) <= (u32)v)
    {
        r++;
    }
    return r;
}

/* 填充圆角矩形: 中间矩形 + 上下两条圆角扫描线 */
void UI_FillRound(u16 x1, u16 y1, u16 x2, u16 y2, u8 radius, u16 color)
{
    u16 t;
    u16 dy;
    u16 half;
    u16 inset;
    u8  i;

    if (x1 > x2) { t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; }
    if (x2 >= UI_SCREEN_W) x2 = UI_SCREEN_W - 1;
    if (y2 >= UI_SCREEN_H) y2 = UI_SCREEN_H - 1;
    if (x1 > x2 || y1 > y2) return;

    if (radius > (u8)((u16)(x2 - x1 + 1) / 2))
    {
        radius = (u8)((u16)(x2 - x1 + 1) / 2);
    }
    if (radius > (u8)((u16)(y2 - y1 + 1) / 2))
    {
        radius = (u8)((u16)(y2 - y1 + 1) / 2);
    }

    if (radius == 0)
    {
        UI_Fill(x1, y1, x2, y2, color);
        return;
    }

    UI_Fill(x1, (u16)(y1 + radius), x2, (u16)(y2 - radius), color);

    for (i = 0; i < radius; i++)
    {
        dy = (u16)(radius - i);
        half = UI_Isqrt((u16)((u16)radius * radius - dy * dy));
        inset = (u16)(radius - half);

        UI_Fill((u16)(x1 + inset), (u16)(y1 + i),
                (u16)(x2 - inset), (u16)(y1 + i), color);
        UI_Fill((u16)(x1 + inset), (u16)(y2 - i),
                (u16)(x2 - inset), (u16)(y2 - i), color);
    }
}

/* 1px 圆角边框: 四条直边 + 四个 1/4 圆角 */
void UI_FrameRound(u16 x1, u16 y1, u16 x2, u16 y2, u8 radius, u16 color)
{
    u16 t;
    u16 dy;
    u16 half;
    u16 inset;
    u8  i;

    if (x1 > x2) { t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; }
    if (x2 >= UI_SCREEN_W) x2 = UI_SCREEN_W - 1;
    if (y2 >= UI_SCREEN_H) y2 = UI_SCREEN_H - 1;
    if (x1 > x2 || y1 > y2) return;

    if (radius > (u8)((u16)(x2 - x1 + 1) / 2))
    {
        radius = (u8)((u16)(x2 - x1 + 1) / 2);
    }
    if (radius > (u8)((u16)(y2 - y1 + 1) / 2))
    {
        radius = (u8)((u16)(y2 - y1 + 1) / 2);
    }

    if (radius == 0)
    {
        UI_Frame(x1, y1, x2, y2, color);
        return;
    }

    UI_Fill((u16)(x1 + radius), y1, (u16)(x2 - radius), y1, color);
    UI_Fill((u16)(x1 + radius), y2, (u16)(x2 - radius), y2, color);
    UI_Fill(x1, (u16)(y1 + radius), x1, (u16)(y2 - radius), color);
    UI_Fill(x2, (u16)(y1 + radius), x2, (u16)(y2 - radius), color);

    for (i = 0; i < radius; i++)
    {
        dy = (u16)(radius - i);
        half = UI_Isqrt((u16)((u16)radius * radius - dy * dy));
        inset = (u16)(radius - half);

        UI_Fill((u16)(x1 + inset), (u16)(y1 + i),
                (u16)(x1 + inset), (u16)(y1 + i), color);
        UI_Fill((u16)(x2 - inset), (u16)(y1 + i),
                (u16)(x2 - inset), (u16)(y1 + i), color);
        UI_Fill((u16)(x1 + inset), (u16)(y2 - i),
                (u16)(x1 + inset), (u16)(y2 - i), color);
        UI_Fill((u16)(x2 - inset), (u16)(y2 - i),
                (u16)(x2 - inset), (u16)(y2 - i), color);
    }
}

/* 实心圆, 用于圆形按钮 */
static void UI_FillCircle(s16 cx, s16 cy, s16 radius, u16 color)
{
    s16 dy;
    s16 dx;
    s16 rr;

    if (radius < 0) return;

    rr = (s16)(radius * radius);

    for (dy = -radius; dy <= radius; dy++)
    {
        dx = (s16)UI_Isqrt((u16)(rr - dy * dy));
        UI_Fill((u16)(cx - dx), (u16)(cy + dy),
                (u16)(cx + dx), (u16)(cy + dy), color);
    }
}

u8 UI_HitTest(const UI_Rect *r, u16 x, u16 y)

{

    if (r == 0) return 0;

    if (x < r->x || x >= (u16)(r->x + r->w)) return 0;

    if (y < r->y || y >= (u16)(r->y + r->h)) return 0;

    return 1;

}



/*----------------------------------------------------------------------------

 * TODO-10 (最小版本已实现, 可继续美化)

 * 通用按钮: 外框 + 底色 + 居中文字, pressed 时反色。

 * 画的时候注意:

 *   - 先用 UI_Fill 铺底, 再 UI_Frame 画边, 最后 UI_TextColor 写字;

 *   - 背景色必须和按钮底色一致, 否则字会带白块(残影);

 *   - 不要在每次重绘时先清全屏。

 *----------------------------------------------------------------------------*/

/* 通用圆角按钮: 外框 + 底色 + 居中文字, pressed 时反色。
 * outside 是按钮外面的底色(页面背景/面板/夜空), 用来清掉圆角外的旧像素。 */
void UI_DrawButtonEx(const UI_Rect *r, const char *label, u8 pressed, u16 outside)
{
    u8  font;
    u8  radius;
    u16 tw;
    u16 tx;
    u16 ty;
    u16 x2;
    u16 y2;
    u16 bg;
    u16 fg;
    u16 border;

    if (r == 0 || label == 0) return;

    x2 = (u16)(r->x + r->w - 1);
    y2 = (u16)(r->y + r->h - 1);

    /* 中文按 16 号字排; 英文先按 16 号字, 放不下再换 12 号字 */
    font = UI_FONT_M;
    if (!UI_TextHasChinese(label) &&
        (UI_TextWidth(label, UI_FONT_M) > (u16)(r->w - 8)))
    {
        font = UI_FONT_S;
    }

    tw = UI_TextWidth(label, font);
    if (tw > (u16)(r->w - 4)) tw = (u16)(r->w - 4);
    tx = (u16)(r->x + (r->w - tw) / 2);
    ty = (u16)(r->y + (r->h - (UI_TextHasChinese(label) ? 16 : font)) / 2);

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

    /* 先铺掉整个矩形, 再画圆角, 避免切换按下状态时四角残留旧底色 */
    UI_Fill(r->x, r->y, x2, y2, outside);

    radius = (u8)(r->h / 3);
    if (radius < 4) radius = 4;
    if (radius > 10) radius = 10;
    if (radius > (u8)(r->w / 2)) radius = (u8)(r->w / 2);

    UI_FillRound(r->x, r->y, x2, y2, radius, bg);
    UI_FrameRound(r->x, r->y, x2, y2, radius, border);

    UI_TextColor(tx, ty, r->w, font, font, fg, bg, label);
}

void UI_DrawButton(const UI_Rect *r, const char *label, u8 pressed)
{
    UI_DrawButtonEx(r, label, pressed, UI_COLOR_BG);
}

/* 圆形按钮: 计时器页的 开始/计圈/复位 用。
 * 圆内文字居中; 中文用 16x16 字库, 英文按 16/12 号字。 */
void UI_DrawCircleButton(const UI_Rect *r, const char *label, u8 pressed, u16 outside)
{
    u8  font;
    u8  cn;
    u16 tw;
    s16 cx;
    s16 cy;
    s16 radius;
    s16 ty;
    u16 bg;
    u16 fg;
    u16 border;

    if (r == 0 || label == 0) return;

    radius = (s16)((r->w < r->h ? r->w : r->h) / 2);
    if (radius < 6) return;

    cx = (s16)(r->x + r->w / 2);
    cy = (s16)(r->y + r->h / 2);

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

    /* 清掉圆外的旧像素 */
    UI_Fill(r->x, r->y, (u16)(r->x + r->w - 1),
            (u16)(r->y + r->h - 1), outside);

    UI_FillCircle(cx, cy, radius, border);
    UI_FillCircle(cx, cy, (s16)(radius - 2), bg);

    cn = UI_TextHasChinese(label);
    font = UI_FONT_M;
    if (!cn && UI_TextWidth(label, UI_FONT_M) > (u16)(2 * radius - 8))
    {
        font = UI_FONT_S;
    }

    tw = UI_TextWidth(label, font);
    ty = cn ? (s16)(cy - 8) : (s16)(cy - (s16)(font / 2));

    UI_TextColor((u16)(cx - (s16)(tw / 2)), (u16)ty,
                 (u16)(tw + 2), font, font, fg, bg, label);
}

/*----------------------------------------------------------------------------

 * TODO-11 (最小版本已实现, 可继续美化)

 * 顶部状态栏: 标题 + 底部强调线; BACK/NEXT 放在底部导航栏。

 * 校准只在 BOOT 开始界面提供, 内嵌页面不再放 CAL。

 * 高度 UI_STATUS_H, 用 UI_COLOR_PANEL 铺底。

 *----------------------------------------------------------------------------*/

void UI_DrawStatusBar(const char *title)

{

    UI_Fill(0, 0, UI_SCREEN_W - 1, UI_STATUS_H - 1, UI_COLOR_PANEL);



    /* 顶部只留标题; BACK/NEXT 放到底部导航栏 */

    UI_TextColor(6, 2, (u16)(UI_SCREEN_W - 12),

                 UI_STATUS_H - 4, UI_FONT_M,

                 UI_COLOR_TEXT, UI_COLOR_PANEL, title);



    /* 底部一条强调线, 让标题栏和内容区分开 */

    UI_Fill(0, UI_STATUS_H - 1, UI_SCREEN_W - 1, UI_STATUS_H - 1, UI_COLOR_ACCENT);

}



/*------------------------------ 底部导航栏 --------------------------------*/

#define UI_NAV_Y 286

static const UI_Rect s_nav_back = { 4, 290, 78, 26 };

static const UI_Rect s_nav_next = { 158, 290, 78, 26 };

static u8 s_nav_pressed = 0;       /* 0=无, 1=BACK, 2=NEXT */



static void UI_DrawNavBar(void)

{

    if (!s_nav_visible) return;



    UI_Fill(0, UI_NAV_Y, UI_SCREEN_W - 1, UI_SCREEN_H - 1, UI_COLOR_PANEL);

    UI_Fill(0, UI_NAV_Y, UI_SCREEN_W - 1, UI_NAV_Y, UI_COLOR_BORDER);



    UI_DrawButtonEx(&s_nav_back, "返回", 0, UI_COLOR_PANEL);

    UI_DrawButtonEx(&s_nav_next, "下页", 0, UI_COLOR_PANEL);

}



/*------------------------------ 页面管理 ----------------------------------*/

void UI_MarkDirty(void)

{

    /* 拖动滑条期间不接受整页重绘请求, 只让页面自己局部画 */

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



    /* 换页时整屏清一次是可以接受的; 同一页面内绝对不要整屏清。 */

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



/* 清空返回历史后切页。给全屏游戏页的 BACK 用:

 * 退出迷宫/贪吃蛇后回到 PWM 主界面, 再按 BACK 就回开始界面, 不会又掉进游戏。 */

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



/* 显示/隐藏底部 BACK/NEXT 导航条。MAZE/SNAKE 全屏页进入时隐藏, 换页时

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



/*------------------------------ 触摸事件 ----------------------------------*/

static void UI_Dispatch(UI_Event *ev)

{

    /* 键盘是全局覆盖层, 打开时优先吃事件 */

    if (g_keypad.active)

    {

        UI_Keypad_OnTouch(ev);

        return;

    }



    /* 拖动滑条期间: 导航和整页重绘都让路, 事件只交给当前页面 */

    if (g_ui_drag_lock)

    {

        if (g_ui_pages[s_page_id].on_event != 0)

        {

            g_ui_pages[s_page_id].on_event(ev);

        }

        return;

    }



    /* 底部导航栏: 按下记录, 松手在同一个按钮上才执行,

     * 避免把按压状态带进开始界面, 也避免拖动滑条误触。 */

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

    u8 pressed = TP_Scan(0);   /* 0=返回屏幕坐标 */

    u16 x;

    u16 y;



    if (pressed)

    {

        x = tp_dev.x[0];

        y = tp_dev.y[0];



        /* 松开时库会把坐标设成 0xFFFF, 按下时不应出现; 越界也丢掉 */

        if (x == 0xFFFF || y == 0xFFFF) return;

        if (x >= UI_SCREEN_W || y >= UI_SCREEN_H) return;



        if (!s_touch_down)

        {

            s_touch_down = 1;

            s_down_x = x;

            s_down_y = y;

            s_last_x = x;

            s_last_y = y;



            /* 清掉"新按下"标志, 否则每次扫描都当成一次新按下 */

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

    /* 调试用: 在触摸点画一个小十字, 验证坐标映射。

     * 会留下痕迹, 正式演示前把 UI_BRINGUP_TOUCH_DOT 改成0。 */

    if (pressed)

    {

        UI_Fill(x > 2 ? x - 2 : 0, y, x < UI_SCREEN_W - 3 ? x + 2 : UI_SCREEN_W - 1, y, UI_COLOR_ACCENT);

        UI_Fill(x, y > 2 ? y - 2 : 0, x, y < UI_SCREEN_H - 3 ? y + 2 : UI_SCREEN_H - 1, UI_COLOR_ACCENT);

    }

#endif

}



/*------------------------------ 初始化/主循环 -----------------------------*/

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



    /* 1. 触摸扫描: 触摸驱动是软件 SPI + 多次 AD + 排序, 单次耗时是毫秒级,

     *    所以不要每轮循环都扫, 按 UI_TOUCH_SCAN_MS 节流。 */

    if ((u32)(now - s_last_touch_ms) >= UI_TOUCH_SCAN_MS)

    {

        s_last_touch_ms = now;

        UI_TouchPoll();

    }



    /* 2. 页面自己的周期任务 (秒表/贪吃蛇/扫频等都靠这个推进) */

    if (g_ui_pages[s_page_id].tick != 0)

    {

        g_ui_pages[s_page_id].tick(now);

    }



    /* 3. 脏标记 + 限速重绘; 键盘打开时最后画, 盖在页面上 */

    if (s_dirty && (u32)(now - s_last_draw_ms) >= UI_REFRESH_MS)

    {

        s_last_draw_ms = now;

        s_dirty = 0;



        /* 键盘打开时不要重画整页, 否则每按一个数字都会闪一次 */

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



/*------------------------------ 数字键盘 ----------------------------------*/

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

    g_ui_full_redraw = 1;   /* 键盘盖住的区域要清一次 */

    UI_MarkDirty();

}



/*----------------------------------------------------------------------------

 * TODO-12 (基本功能: 数字键盘, 留给你)

 *

 * 画 3x4 键盘:

 *   每个键 70x44, 从 y=68 开始往下排; 键之间留 5~6px;

 *   用 UI_DrawButton 画; 按下时 pressed=1 反色, 松手恢复;

 *   顶部显示当前编辑值 g_keypad.value, 用 UI_COLOR_EDIT 色。

 *

 * 点击处理 UI_Keypad_OnTouch:

 *   - 只在 UI_EV_DOWN 时触发 (或 DOWN 高亮 + UP 提交, 二选一, 要一致);

 *   - 数字键: value = value*10 + digit, 超过 max_digits 忽略;

 *   - DEL: value /= 10;

 *   - OK: UI_Keypad_Close(1);

 *   - 点键盘外面: 建议 UI_Keypad_Close(0) 取消。

 *   - 每次改动 UI_MarkDirty()。

 *----------------------------------------------------------------------------*/

/* 3x4 键盘: 10,68 起, 每键 70x44, 列间距 5, 行间距 6 */

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

        /* 只在打开键盘时画一次底板和所有按键 */

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

        /* 数字变化时只重画顶部数值区, 键盘不动 */

        UI_Fill(0, 24, UI_SCREEN_W - 1, 64, UI_COLOR_PANEL);

    }



    UI_TextColor(8, 26, 120, 12, UI_FONT_S,

                 UI_COLOR_MUTED, UI_COLOR_PANEL, "输入数值");

    UI_NumColor(8, 38, (u32)((g_keypad.value < 0) ? 0 : g_keypad.value),

                g_keypad.max_digits, UI_FONT_L,

                UI_COLOR_EDIT, UI_COLOR_PANEL);

}



void UI_DrawPixelBackground(void)

{

    u16 i;

    u16 y;

    u16 x;



    /* 底色 */

    UI_Fill(0, 0, UI_SCREEN_W - 1, UI_SCREEN_H - 1,

            PIX_WOOD_DARK);



    /* 8 块横向木板，每块 40px */

    for (i = 0; i < 8; i++)

    {

        y = (u16)(i * 40);



        /* 木板缝 */

        UI_Fill(0, y, UI_SCREEN_W - 1, y + 1,

                PIX_WOOD_SEAM);



        /* 木板上沿高光 */

        UI_Fill(0, y + 2, UI_SCREEN_W - 1, y + 2,

                PIX_WOOD_MID);



        /* 竖向接缝，左右交错 */

        x = (i & 1) ? 64 : 176;



        UI_Fill(x, y + 3, x + 1, y + 38,

                PIX_WOOD_SEAM);



        UI_Fill(x + 2, y + 3, x + 2, y + 38,

                PIX_WOOD_MID);



        /* 木纹短线 */

        UI_Fill(16,  y + 12, 63,  y + 12, PIX_WOOD_SEAM);

        UI_Fill(96,  y + 26, 151, y + 26, PIX_WOOD_MID);

        UI_Fill(168, y + 34, 215, y + 34, PIX_WOOD_SEAM);



        /* 节疤 */

        UI_Fill(112, y + 14, 115, y + 15, PIX_WOOD_SEAM);

        UI_Fill(113, y + 16, 114, y + 16, PIX_WOOD_LIGHT);

    }

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



        digit = (i == 10) ? 0 : (u8)(i + 1);   /* 0 键在索引10 */



        if (g_keypad.digits < g_keypad.max_digits)

        {

            if (g_keypad.digits == 0)

            {

                g_keypad.value = digit;       /* 第一次按键替换初始值 */

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



    /* 点键盘外面: 取消 */

    UI_Keypad_Close(0);

    return 1;

}



