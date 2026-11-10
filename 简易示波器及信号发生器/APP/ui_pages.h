#ifndef __UI_PAGES_H
#define __UI_PAGES_H

#include "ui_core.h"

/* 页面表, 定义在 ui_pages.c。
 * ui_core.c 通过它来分发 draw/event/tick。 */
extern const UI_PageOps g_ui_pages[PAGE_COUNT];

#endif
