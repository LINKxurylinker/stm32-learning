#ifndef __APP_BOOT_H
#define __APP_BOOT_H

#include "ui_core.h"

/* 开机流程: 触摸硬件初始化 -> 开始界面 -> 短击/长按/校准 -> 返回目标页面
 * 返回值:
 *   PAGE_PWM  = 短击 START, 进入"微型输入输出仪器"主界面
 *   PAGE_SNAKE= 长按 START 3 秒, 进入贪吃蛇
 */
UI_PageId BOOT_Run(void);

#endif
