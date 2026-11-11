#ifndef __UI_HANZI_H
#define __UI_HANZI_H

#include "stm32f4xx.h"

typedef struct
{
    u16 code;
    u8  data[32];
} UI_HZ16;

extern const UI_HZ16 g_ui_hz16[];
extern const u16 g_ui_hz16_count;

const UI_HZ16 *UI_HZ16_Find(u16 code);
void UI_HZ16_DrawChar(u16 x, u16 y, u16 code, u16 fg, u16 bg);
void UI_HZ16_DrawCharScaled(u16 x, u16 y, u16 code,
                            u16 fg, u16 bg, u8 scale);
u16  UI_HZ16_Text(u16 x, u16 y, const char *s, u16 fg, u16 bg);
u16  UI_HZ16_TextScaled(u16 x, u16 y, const char *s,
                        u16 fg, u16 bg, u8 scale);

#endif
