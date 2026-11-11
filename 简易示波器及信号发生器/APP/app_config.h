#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

#include "stm32f4xx.h"

/*==============================================================================
 * app_config.h
 *-----------------------------------------------------------------------------
 * 作用:
 *   1) 整个工程的编译开关
 *   2) 定时器 / 引脚映射
 *   3) 题目的硬性指标 (频率范围、占空比步进、误差门限)
 *   4) UI 的颜色、字体、刷新周期、触摸参数
 *
 * 使用原则:
 *   - 所有"魔法数字"都集中到这里, 验收时要求现场改参数(频率上下限、
 *     占空比步进等), 只改本文件即可, 不要散落在各个 .c 里。
 *   - 带 TODO 的宏是留给你调的地方, 默认值只是让框架能跑。
 *   - 改引脚/定时器时, 必须同时检查 bsp_xxx.c 里的 TIM_OCxInit /
 *     GPIO_PinAFConfig 是否对应 (注释里已标出)。
 *============================================================================*/

#define APP_VERSION_STR             "v0.1-framework"

/* TODO-00 (调参总入口, 留给你):
 *   下面所有带"可调"含义的宏都在这个文件里。验收时要求现场改
 *   频率上下限/占空比步进, 只改这里, 不要改到各个 .c 里。
 *   默认值只是让框架能跑, 不是标准答案; 按题目和你的硬件改。 */

/*==============================================================================
 * 1. 编译开关 (先做基本功能, 拓展功能一条条打开)
 *============================================================================*/
#define APP_ENABLE_PHASE            1   /* 拓展1: 双通道相位 */
#define APP_ENABLE_SPWM             1   /* 拓展4: SPWM 50Hz */
#define APP_ENABLE_EXT500K          0   /* 基础要求: 200kHz / 1% 档 */

#define APP_DEBUG_UART              1   /* 串口打印调试信息 */
#define APP_DEBUG_PERIODIC          0   /* 1=每秒打印一次(会拖慢UI), 演示时建议0 */

/* 开机画面/触摸校准/长按彩蛋 */
#define BOOT_SPLASH_TIMEOUT_MS      0UL     /* 0=不自动跳页, 等用户点; >0=超时自动继续 */
#define BOOT_LONG_PRESS_MS          3000UL  /* 长按 START 3 秒 -> 迷宫 */
#define BOOT_TOUCH_DEBUG            1       /* 1=开始界面底部显示 CAL/TP 坐标, 排查触摸 */
#define BOOT_FORCE_CALIB            0       /* 1=每次开机都强制四点校准 (调试用) */

/* 主界面: 自动识别输入信号, 切换输出/捕获显示 */
#define METER_CAP_ON_MS             300UL   /* 连续检测到输入信号多久 -> 捕获模式 */
#define METER_CAP_OFF_MS            2000UL  /* 输入消失多久 -> 回到输出模式 */

#define PWM_BRINGUP_TEST            1   /* 1=上电固定输出 1kHz/50%, 用来先验证硬件 */
#define UI_BRINGUP_TOUCH_DOT        0   /* 1=触摸处画点, 用来验证触摸坐标 */

/* 返回值约定 (在所有模块里统一) */
#define APP_OK                      0
#define APP_ERR_PARAM               1
#define APP_ERR_NOT_IMPL            2

/*==============================================================================
 * 2. 系统时钟事实 (改时钟后必须同步改这里, 否则所有频率都会错)
 *-----------------------------------------------------------------------------
 * HSE = 8MHz, PLL: 8/8*336/2 = 168MHz
 * APB1 = 42MHz  -> TIM2/3/4/5/6/7/12/13/14 的计数时钟 = 84MHz
 * APB2 = 84MHz  -> TIM1/8/9/10/11 的计数时钟 = 168MHz
 *============================================================================*/
#define APP_SYSCLK_HZ               168000000UL
#define APP_APB1_TIMER_HZ           84000000UL
#define APP_APB2_TIMER_HZ           168000000UL

/*==============================================================================
 * 3. 定时器 / 引脚映射
 *-----------------------------------------------------------------------------
 * 下面这套是本框架的默认分配, 都在例程已占用引脚之外:
 *   已占用: LCD(PD/PE/PF12/PG12/PB15), 触摸(PB0/1/2/PC13/PF11),
 *           USART1(PA9/PA10), EEPROM(PB8/PB9), LED(PF9/PF10)
 *
 * TODO: 焊接/接线前先对照开发板排针丝印确认这些脚真的引出来了。
 *       如果某个脚被占用, 改这里 + 对应 bsp 文件里的初始化。
 *============================================================================*/

/* ---- CH1: TIM2_CH4 = PA3, 84MHz, 主定时器 ----
 * PA3 复用 TIM2_CH4 (AF1)。
 * TIM2 是 32 位定时器, 用它做频率主时基; TIM2_CH2 (PA1) 只做内部相位参考,
 * 不输出到引脚。TIM2 TRGO = OC2REF, 在相位点触发 TIM9 从模式复位。 */
#define PWM_TIM                     TIM2
#define PWM_TIM_RCC                 RCC_APB1Periph_TIM2
#define PWM_TIM_HZ                  APP_APB1_TIMER_HZ
#define PWM_GPIO                    GPIOA
#define PWM_GPIO_RCC                RCC_AHB1Periph_GPIOA
#define PWM_PIN                     GPIO_Pin_3
#define PWM_PIN_SRC                 GPIO_PinSource3
#define PWM_AF                      GPIO_AF_TIM2
#define PWM_CHANNEL                 TIM_Channel_4

/* ---- CH2: TIM9_CH1 = PA2, 168MHz, 从定时器 ----
 * PA2 复用 TIM9_CH1 (AF3)。
 * TIM9 从模式 Reset 由 TIM2 TRGO 触发; 相位 = TIM2_CH2 的比较点。
 * 这样两路共用频率, 相位 0..360 度可调, 占空比各自独立。 */
#define PWM2_TIM                    TIM9
#define PWM2_TIM_RCC                RCC_APB2Periph_TIM9
#define PWM2_TIM_HZ                 APP_APB2_TIMER_HZ
#define PWM2_GPIO                   GPIOA
#define PWM2_GPIO_RCC               RCC_AHB1Periph_GPIOA
#define PWM2_PIN                    GPIO_Pin_2
#define PWM2_PIN_SRC                GPIO_PinSource2
#define PWM2_AF                     GPIO_AF_TIM9
#define PWM2_CHANNEL                TIM_Channel_1

/* ---- 输入捕获: TIM5_CH1 = PA0, 32位, 84MHz ----
 * 说明: 1Hz 的周期在 84MHz 下是 8400万个数, 16位定时器必溢出,
 *       所以选 32 位的 TIM5。PA0 在部分开发板上是 WK_UP 按键,
 *       若不合适, 可改 TIM2_CH1(PA5) (TIM2 也是32位), 但 PWM 输入
 *       模式必须用 CH1/TI1, 不能随便换到 CH2。 */
#define CAP_TIM                     TIM5
#define CAP_TIM_RCC                 RCC_APB1Periph_TIM5
#define CAP_TIM_HZ                  APP_APB1_TIMER_HZ
#define CAP_GPIO                    GPIOA
#define CAP_GPIO_RCC                RCC_AHB1Periph_GPIOA
#define CAP_PIN                     GPIO_Pin_0
#define CAP_PIN_SRC                 GPIO_PinSource0
#define CAP_AF                      GPIO_AF_TIM5
#define CAP_IRQn                    TIM5_IRQn

/* ---- SPWM: TIM3_CH1 = PA6, 84MHz ----
 * 载波 20kHz, 正弦 50Hz, 400 点 -> 20k/400 = 50Hz 正好整数。
 * 注意: PA6 也是 SPI1_MISO, 本工程没开 SPI1, 可以用。 */
#define SPWM_TIM                    TIM3
#define SPWM_TIM_RCC                RCC_APB1Periph_TIM3
#define SPWM_TIM_HZ                 APP_APB1_TIMER_HZ
#define SPWM_GPIO                   GPIOA
#define SPWM_GPIO_RCC               RCC_AHB1Periph_GPIOA
#define SPWM_PIN                    GPIO_Pin_6
#define SPWM_PIN_SRC                GPIO_PinSource6
#define SPWM_AF                     GPIO_AF_TIM3
#define SPWM_IRQn                   TIM3_IRQn

/* ---- 1ms 系统节拍: TIM6 (基本定时器, 无引脚) ----
 * 不要用 SysTick! delay.c 把 SysTick 当查询式延时用了,
 * 再开 SysTick 中断会互相打架。 */
#define TICK_TIM                    TIM6
#define TICK_TIM_RCC                RCC_APB1Periph_TIM6
#define TICK_TIM_HZ                 APP_APB1_TIMER_HZ
#define TICK_IRQn                   TIM6_DAC_IRQn

/*==============================================================================
 * 4. 基本 PWM 指标 (题目硬性要求)
 *============================================================================*/
#define PWM_FREQ_MIN_HZ             1UL
#define PWM_EXT_FREQ_MAX_HZ         500000UL
#define PWM_EXT_DUTY_STEP_PCT       5

/* 拓展5: 500kHz 时上限 500k, 占空比 5% 一档 */
#if APP_ENABLE_EXT500K
#define PWM_FREQ_MAX_HZ             PWM_EXT_FREQ_MAX_HZ
#define PWM_DUTY_STEP_PCT           PWM_EXT_DUTY_STEP_PCT
#else
#define PWM_FREQ_MAX_HZ             200000UL
#define PWM_DUTY_STEP_PCT           1
#endif

#define PWM_DUTY_MIN_PCT            0
#define PWM_DUTY_MAX_PCT            100

#define PWM_DEFAULT_FREQ_HZ         1000UL
#define PWM_DEFAULT_DUTY_PCT        50

/*==============================================================================
 * 5. 输入捕获指标
 *============================================================================*/
#define CAP_FREQ_MIN_HZ             1UL
#define CAP_FREQ_MAX_HZ             PWM_FREQ_MAX_HZ
#define CAP_TIMEOUT_MS              2000UL  /* 超过这么久没边沿 -> NO SIGNAL */
#define CAP_FREQ_TOL_PCT            1

/*==============================================================================
 * 6. 秒表
 *============================================================================*/
#define SW_DISPLAY_MAX_MS           359999999UL  /* 99:59:59.999, 再大就回绕 */

/*==============================================================================
 * 7. 自动扫频
 *-----------------------------------------------------------------------------
 * 题目: 10Hz 到 500kHz, 至少 10 个频点, 屏幕列出
 *       设定频率 / 实测频率 / 误差, 全达标 PASS。
 * 注意: 低频点(10Hz)一个周期就是 100ms, 稳定等待不能太短。
 *============================================================================*/
#define SWEEP_POINT_COUNT           12
#define SWEEP_SETTLE_MS             400     /* 每个频点等待时间, 10Hz 时约 4 个周期 */
#define SWEEP_TIMEOUT_MS            3000    /* 单点等捕获超时 */
#define SWEEP_FREQ_TOL_PCT          1

/*==============================================================================
 * 8. SPWM
 *============================================================================*/
#define SPWM_CARRIER_HZ             20000UL
#define SPWM_SINE_HZ                50UL
#define SPWM_POINTS                 400     /* 20kHz / 400 = 50Hz */
#define SPWM_AMP_PCT                90      /* 调制深度, 100 会削顶 */

/*==============================================================================
 * 9. 贪吃蛇 (最后一页)
 *-----------------------------------------------------------------------------
 * 屏幕 240x320, 全屏游戏时:
 *   顶部状态栏 22px + 棋盘 20*11=220px + 底部两排按键 2*31px + 间距 = 320px。
 * 棋盘必须留出底部按键的位置, 所以格子用 11px 而不是 12px。
 * 如果把 SNAKE_CELL_PX 改成 12, 需要同时改 ui_pages.c 里的布局常量。
 *============================================================================*/
#define SNAKE_GRID_W                20      /* 固定 20x20, 改尺寸要重看回路构造 */
#define SNAKE_GRID_H                20
#define SNAKE_CELL_PX               11      /* 20*11=220, 给按键留空间 */
#define SNAKE_MAX_LEN               (SNAKE_GRID_W * SNAKE_GRID_H)
#define SNAKE_STEP_MS               220     /* 手动模式每走一格的时间(ms) */
#define SNAKE_AUTO_STEP_MS          40      /* 自动模式每 40ms 一个时间片 */
#define SNAKE_AUTO_STEPS_PER_MS     1       /* 每个时间片走 1 步 */
#define SNAKE_AUTO_BURST            1       /* 每次 SNAKE_Tick 最多补 1 个时间片 */
#define SNAKE_AUTO_FOOD_FOUND       5       /* 安全捷径检查里预留的"吃到食物"次数 */
#define SNAKE_AUTO_DEFAULT          0       /* 开机默认手动; 按 AUTO 开自动 */
#define SNAKE_KEYS_ENABLE           1       /* 1=用 KEY0/1/2/WK_UP 控制方向 */

/*==============================================================================
 * 9b. 迷宫 (开机长按 START 3 秒的彩蛋)
 *-----------------------------------------------------------------------------
 * 21x21 格, 每格 10px, 棋盘 210x210; 奇数坐标是通道, 偶数坐标是墙。
 * 起点 (1,1), 出口 (19,19); 用迭代 DFS 生成, A* 自动求最短路径。
 *============================================================================*/
#define MAZE_GRID_W                 21      /* 必须是奇数 */
#define MAZE_GRID_H                 21
#define MAZE_CELL_PX                10      /* 21*10=210, 两侧各留 15px */
#define MAZE_AUTO_STEP_MS           90      /* A* 回放: 每 90ms 走一格 */
#define MAZE_AUTO_DEFAULT           0       /* 开机默认手动 */
#define MAZE_KEYS_ENABLE            1       /* 1=用 KEY0/1/2/WK_UP 控制方向 */

/*==============================================================================
 * 10. UI 颜色 (RGB565)
 *============================================================================*/
#define UI_COLOR_BG                 PIX_WOOD_DARK /*木底 */
#define UI_COLOR_PANEL              0x18E3  /* 深灰面板 */
#define UI_COLOR_PANEL_HI           0x2D6B  /* 高亮面板 */
#define UI_COLOR_BORDER             0x5ACB  /* 边框 */
#define UI_COLOR_TEXT               0xF7BE  /* 白字 */
#define UI_COLOR_MUTED              0x8410  /* 灰字 */
#define UI_COLOR_VALUE              0x07FF  /* 青色: 实际生效值 */
#define UI_COLOR_EDIT               0xFFE0  /* 黄色: 正在编辑/未确认值 */
#define UI_COLOR_OK                 0x07E0  /* 绿色 */
#define UI_COLOR_ERR                0xF800  /* 红色 */
#define UI_COLOR_ACCENT             0xFD20  /* 橙色 */
/* 星露谷像素风 */
#define PIX_WOOD_DARK    0x28C1   /* 深木色背景 */
#define PIX_WOOD_SEAM    0x1860   /* 木板缝 */
#define PIX_WOOD_MID     0x4963   /* 中木色 */
#define PIX_WOOD_LIGHT   0x8AC6   /* 亮木边 */
#define PIX_PARCHMENT    0xEE51   /* 羊皮纸 */
#define PIX_CREAM        0xF717   /* 米白文字 */
#define PIX_DARK_TEXT    0x3922   /* 深棕文字 */
#define PIX_GOLD         0xDD28   /* 金色边框 */
#define PIX_GREEN        0x6CE9   /* 绿色 */
#define PIX_DEEP_GREEN   0x3B46   /* 深绿 */
#define PIX_RED          0xB247   /* 红色 */

/*==============================================================================
 * 11. UI 字体与刷新
 *-----------------------------------------------------------------------------
 * 例程字库只有 ASCII 12/16/24 三种可用 (LCD_ShowChar 只认 12/16/24),
 * 汉字只有 4 个 ("伍陆电子"), 所以界面文字请全部用英文, 不要写中文。
 * 12号字: 6x12, 16号: 8x16, 24号: 12x24
 *============================================================================*/
#define UI_FONT_S                   12
#define UI_FONT_M                   16
#define UI_FONT_L                   24

#define UI_SCREEN_W                 240     /* 竖屏: 宽240 */
#define UI_SCREEN_H                 320     /* 竖屏: 高320 */
#define UI_STATUS_H                 22      /* 顶部状态栏高度 */

#define UI_TOUCH_SCAN_MS            10      /* 触摸扫描周期, 触摸扫描本身很慢, 别每轮都扫 */
#define TP_FIX_Y_INVERT             0       /* 1=触摸Y轴上下翻转(这块屏的方向) */
#define TP_FIX_X_INVERT             0       /* 1=触摸X轴左右翻转, 与Y一起构成180度旋转 */
#define UI_REFRESH_MS               40      /* 脏区域重绘周期, 25Hz */
#define UI_MOVE_THRESHOLD_PX        2       /* 拖动判定阈值 */
#define UI_TOUCH_DEBOUNCE_MS        20      /* 预留: 按键消抖 */

/*==============================================================================
 * 12. 数字键盘
 *============================================================================*/
#define UI_KEYPAD_COUNT             12      /* 3x4: 0-9 + DEL + OK */
#define UI_KEYPAD_MAX_DIGITS        7       /* 200000/500000 都是 6 位, 留 1 位余量 */
#define UI_KEYPAD_DEL               9
#define UI_KEYPAD_OK                11

#endif /* __APP_CONFIG_H */
