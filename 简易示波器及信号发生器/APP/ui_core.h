#ifndef __UI_CORE_H
#define __UI_CORE_H

#include "stm32f4xx.h"

/* 页面编号。新增页面时:
 *   1) 在这里加一个枚举
 *   2) 在 ui_pages.c 里加 draw/event/tick 并填进表
 *   3) 别忘了 PAGE_COUNT 永远在最后 */
typedef enum
{
    PAGE_PWM = 0,
    PAGE_CAPTURE,
    PAGE_STOPWATCH,
    PAGE_SWEEP,
    PAGE_SPWM,
    PAGE_MAZE,
    PAGE_SNAKE,
    PAGE_COUNT
} UI_PageId;

/* 触摸事件类型: 按下 / 拖动 / 抬起 */
typedef enum
{
    UI_EV_NONE = 0,
    UI_EV_DOWN,
    UI_EV_MOVE,
    UI_EV_UP
} UI_EventType;

typedef struct
{
    UI_EventType type;
    u16 x;          /* 当前坐标 */
    u16 y;
    u16 down_x;     /* 按下时的坐标(拖动/滑条用) */
    u16 down_y;
    u32 ms;         /* 事件时间 */
} UI_Event;

typedef struct
{
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} UI_Rect;

/* 一个页面要实现这几个函数; 不用的可以填 0。 */
typedef struct
{
    void (*enter)(void);
    void (*leave)(void);
    void (*draw)(void);
    void (*on_event)(const UI_Event *ev);
    void (*tick)(u32 now_ms);
} UI_PageOps;

/*------------------------------ 数字键盘 ----------------------------------*/
typedef struct
{
    u8  active;
    u8  target;         /* 当前在编辑哪个字段, 页面自己定义含义 */
    u8  digits;         /* 已输入位数 */
    u8  max_digits;
    int32_t value;      /* 编辑缓冲, 确认前不生效 */
    void (*on_confirm)(int32_t value);
} UI_Keypad;

extern UI_Keypad g_keypad;
extern volatile u8 g_ui_go_boot;   /* 1=请求回到开始界面 */
extern volatile u8 g_ui_drag_lock; /* 1=滑条拖动中, 页面重绘和导航都让路 */
extern volatile u8 g_ui_full_redraw; /* 1=页面下次绘制先清一次整屏 */
extern const char *const g_ui_keypad_labels[12];

/* 3x4 键盘的 12 个键: 1 2 3 / 4 5 6 / 7 8 9 / DEL 0 OK */
#define UI_KEY_DEL      9
#define UI_KEY_OK       11

/*------------------------------ 核心接口 ----------------------------------*/
void UI_Init(void);
void UI_InitAt(UI_PageId id);               /* 开机直接进入指定页面 */
void UI_Loop(void);                         /* 主循环里反复调用, 非阻塞 */
void UI_GoPage(UI_PageId id);
void UI_GoRoot(UI_PageId id);               /* 清空返回历史后切页 (彩蛋退出用) */
void UI_NavSetVisible(u8 visible);          /* 0=隐藏底部 BACK/NEXT 导航条 (全屏游戏) */
void UI_MarkDirty(void);
UI_PageId UI_GetPage(void);

/* 绘图小工具: 统一处理 FRONT_COLOR / BACK_COLOR, 防止残影 */
void UI_TextColor(u16 x, u16 y, u16 w, u16 h, u8 size,
                  u16 fg, u16 bg, const char *s);
void UI_NumColor(u16 x, u16 y, u32 num, u8 len, u8 size,
                 u16 fg, u16 bg);
void UI_NumPadColor(u16 x, u16 y, u32 num, u8 len, u8 size,
                    u16 fg, u16 bg);
void UI_Fill(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void UI_Frame(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
u8   UI_HitTest(const UI_Rect *r, u16 x, u16 y);

/* 通用控件 (显示部分留给你画) */
void UI_DrawButton(const UI_Rect *r, const char *label, u8 pressed);
void UI_DrawPixelBackground(void);
void UI_DrawStatusBar(const char *title);

/* 键盘 */
void UI_Keypad_Open(u8 target, int32_t initial, u8 max_digits,
                    void (*on_confirm)(int32_t value));
void UI_Keypad_Close(u8 commit);
void UI_Keypad_Draw(void);
u8   UI_Keypad_OnTouch(const UI_Event *ev);

#endif
