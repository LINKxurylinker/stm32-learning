#include "app_main.h"
#include "app_config.h"
#include "app_boot.h"
#include "bsp_tick.h"
#include "bsp_pwm.h"
#include "bsp_capture.h"
#include "mpu6050.h"
#include "bsp_spwm.h"
#include "app_stopwatch.h"
#include "app_sweep.h"
#include "app_snake.h"
#include "app_maze.h"
#include "ui_core.h"
#include "delay.h"
#include "usart.h"
#include "tftlcd.h"
#include "touch.h"
#include <stdio.h>

/*==============================================================================
 * app_main.c
 *-----------------------------------------------------------------------------
 * 上电流程和主循环。
 *
 * 设计原则:
 *   1) 初始化只做一次, 不在这里跑业务;
 *   2) while(1) 里绝不能有 delay_ms / 长循环, 否则触摸和界面会卡;
 *   3) 周期任务全部用 TICK_Elapsed 做非阻塞调度;
 *   4) 中断只置标志/存数据, 计算和画屏都在主循环。
 *============================================================================*/

static u32 s_last_cap_ms = 0;

void APP_Init(void)
{
    UI_PageId start_page;

    /* NVIC 分组要在任何外设中断初始化之前设置 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    delay_init(168);
    uart_init(115200);

#if APP_DEBUG_UART
    printf("\r\n====================================\r\n");
    printf(" PWM + Capture + UI  framework %s\r\n", APP_VERSION_STR);
    printf(" build: %s %s\r\n", __DATE__, __TIME__);
    printf("====================================\r\n");
#endif

    /* 1ms 节拍: 给 UI/秒表/扫频/迷宫/贪吃蛇用 */
    TICK_Init();

    /* 屏幕: 必须在触摸之前初始化, 因为触摸校准要画十字 */
    TFTLCD_Init();
#if APP_DEBUG_UART
    printf("LCD: %u x %u, dir=%u\r\n",
           (unsigned)tftlcd_data.width,
           (unsigned)tftlcd_data.height,
           (unsigned)tftlcd_data.dir);
#endif

    /* MPU6050: 开始界面落叶风场 */
    MPU6050_Init();
    
    /* 先把屏幕和开机界面跑起来, 再初始化 PWM/CAP。
     * 这样即使 PWM 初始化有问题, 也能先看到 WELCOME, 不会停在白屏。 */
    start_page = BOOT_Run();
#if APP_DEBUG_UART
    printf("Boot: done, start page=%u\r\n", (unsigned)start_page);
#endif

    /* 主 PWM: CH1 = TIM9_CH2 / PA3, 1kHz/50% 自检 */
    PWM_Init();

    /* 输入捕获: TIM5_CH1 / PA0, 32位, PWM输入模式 */
    CAP_Init();

#if APP_ENABLE_PHASE
    /* 第二路: CH2 = TIM9_CH1 / PA2, 和 CH1 共用 TIM9 时基 */
    PWM2_Init();
#endif

#if APP_ENABLE_SPWM
    /* SPWM: TIM3_CH1 / PA6, 只初始化, 默认不启动 */
    SPWM_Init();
#endif

    STOPWATCH_Init();
    SWEEP_Init();
    SNAKE_Init();
    MAZE_Init();

    /* UI: 页面管理 + 开机目标页 (短击=PAGE_PWM, 长按=PAGE_MAZE) */
    UI_InitAt(start_page);

#if APP_DEBUG_UART
    printf("System ready. PWM=%lu Hz, duty=%u%%\r\n",
           (unsigned long)g_pwm.freq_actual_hz,
           (unsigned)g_pwm.duty_actual_pct);
#endif
}

void APP_Loop(void)
{
    /* 1. UI: 触摸扫描 + 页面 tick + 脏区域重绘 */
    UI_Loop();

    /* PWM 主界面的 BACK: 重新进入开始界面 (WELCOME) */
    if (g_ui_go_boot)
    {
        UI_PageId page;
        g_ui_go_boot = 0;
        page = BOOT_Run();
        UI_InitAt(page);
        return;
    }

    /* 2. 输入捕获: 每 100ms 把中断存下的原始值算成频率/占空比。
     *    1Hz 时一个周期 1s, 这里没新数据就保持上次值, 超时才判无效。 */
    if (TICK_Elapsed(&s_last_cap_ms, 100))
    {
        CAP_Update();
    }

    /* 3. 周期串口打印默认关闭: printf 是阻塞的, 115200 下一个字符约 87us,
     *    打印一行几十个字符就会让界面明显卡顿, 演示时不要开。 */
#if APP_DEBUG_PERIODIC
    {
        static u32 last = 0;
        if (TICK_Elapsed(&last, 1000))
        {
            printf("PWM %lu Hz %u%%, CAP %lu Hz %u.%u%% %s\r\n",
                   (unsigned long)g_pwm.freq_actual_hz,
                   (unsigned)g_pwm.duty_actual_pct,
                   (unsigned long)g_cap.freq_hz,
                   (unsigned)(g_cap.duty_permille / 10),
                   (unsigned)(g_cap.duty_permille % 10),
                   g_cap.valid ? "OK" : "NO");
        }
    }
#endif
}
