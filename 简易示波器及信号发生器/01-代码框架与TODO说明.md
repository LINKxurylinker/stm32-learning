# 代码框架说明与 TODO 清单

> 工程位置：`D:\Users\ASUS\Desktop\stm32学习\简易示波器及信号发生器`
> Keil 工程：`USER\PWM_UI_Framework.uvprojx`
> 本文只说明"骨架怎么搭、哪里留白、每个空白是干什么的"，**不给出算法答案**。算法由你自己填。

---

## 0. 这是什么，不是什么

### 已经写好的（基础设施）
- 工程从 `第二题例程` 复制并改名，CORE/FWLIB/SYSTEM/HARDWARE 原样保留。
- 1ms 系统节拍 `TIM6`，非阻塞定时工具 `TICK_Elapsed`。
- 主 PWM / 第二路 PWM / 输入捕获 / SPWM 的**初始化骨架**（时钟、GPIO、AF、定时器、NVIC）。
- 主 PWM 上电自检：固定 1kHz/50%，用来先确认 PA8 有波形。
- 触摸事件层：`TP_Scan` → `DOWN/MOVE/UP` 事件；按键/滑条三态的基础。
- 页面管理：页面表、enter/leave/draw/event/tick、脏标记 + 25Hz 限速重绘。
- 文字/填充/边框/数字的小工具，统一处理前景/背景色，避免残影。
- 数字键盘/滑动条的状态结构与打开/关闭接口。
- **最小可交互 UI**：8 行主菜单、每页统一 BACK、按钮/状态栏、PWM 页 OUT 开关。
- 主循环 `APP_Loop`：UI、捕获更新、周期任务的非阻塞调度。

### 故意留白的（你要写的）
- **PWM 通用算法**：任意频率/占空比 → PSC/ARR/CCR，0%/100% 边界，改频保持占空比。
- **输入捕获算法**：CCR1/CCR2 → 频率/占空比、平均、超时、有效性。
- **秒表状态机**：启动/暂停/重置/圈/分段。
- **扫频状态机**：设频、等待、测量、比较、下一 点、PASS/FAIL。
- **SPWM**：正弦表、启动、20kHz 更新中断。
- **贪吃蛇**：移动、吃食物、碰撞、计分、自动模式。
- **页面美化与操作按钮**：现有页面只是"能看能点"的最小版；键盘、滑条、各页按钮、图表由你继续完善。
- **数字键盘/滑动条的绘制与命中**。
- **相位同步方案**。
- **所有可调参数**：频率上下限、步进、刷新率、颜色、布局、滤波参数等。

> 这个工程**能编译、能上电、能显示 8 行菜单、能进各页并 BACK 返回、能输出 1kHz 自检 PWM**；业务算法和页面美化还要你继续填。

---

## 1. 打开与编译

1. 双击 `USER\PWM_UI_Framework.uvprojx`（Keil MDK）。
2. 确认编译器是 ARMCC V5.06（工程原来就是）。
3. `Project → Build Target`（F7）。
4. 输出在 `OBJ\PWM_UI_Framework.axf/.hex`（OBJ 目录已清空，第一次编译会自动生成）。

工程根目录另有一份当前框架的预编译 `PWM_UI_Framework.hex`，可以直接烧录做硬件自检；改了代码后请重新 Build，不要用旧 hex。

**已验证**：本机 Keil 命令行完整重编译通过：
```
0 Error(s), 6 Warning(s)
Code=29344  RO-data=21992  RW-data=220  ZI-data=3188
```
6 条警告全部来自例程自带的 `tftlcd.c`（未使用变量/不可达 return），不是新代码。新代码 0 警告。

> 如果你看到 `..\APP\xxx.c` 的报错，先检查是否把 `APP` 目录加进了 Include Paths（工程已经加好：`..\APP`）。

---

## 2. 目录结构

```
简易示波器及信号发生器/
├─ CORE/                    启动文件（原例程）
├─ FWLIB/                   ST 标准外设库（原例程）
├─ SYSTEM/
│  ├─ delay/                SysTick 查询式延时（原例程，注意别开 SysTick 中断）
│  ├─ sys/                  位带宏
│  └─ usart/                USART1 + printf 重定向
├─ HARDWARE/
│  ├─ tftlcd/               ILI9341 屏幕驱动 + font.H + picture.h
│  ├─ touch/                电阻触摸 + gt5663/cst716（本工程用电阻屏分支）
│  ├─ 24Cxx/                EEPROM（触摸校准参数）
│  ├─ iic/                  软件 IIC
│  └─ led/                  LED1=PF9, LED2=PF10
├─ APP/                     ← 本框架新增
│  ├─ app_config.h          所有编译开关/引脚/指标/颜色/调参
│  ├─ app_boot.c/.h         开机画面 + 触摸校准流程
│  ├─ app_main.c/.h         上电初始化 + 主循环
│  ├─ bsp_tick.c/.h         TIM6 1ms 节拍
│  ├─ bsp_pwm.c/.h          主 PWM + 第二路/相位
│  ├─ bsp_capture.c/.h      TIM5 输入捕获
│  ├─ bsp_spwm.c/.h         TIM3 SPWM
│  ├─ app_stopwatch.c/.h    秒表
│  ├─ app_sweep.c/.h        自动扫频
│  ├─ app_snake.c/.h        贪吃蛇逻辑
│  ├─ ui_core.c/.h          事件/页面/重绘/文字工具/键盘/滑条
│  └─ ui_pages.c/.h         各页面显示与交互（大部分留白）
├─ USER/
│  ├─ main.c                只调用 APP_Init/APP_Loop
│  ├─ stm32f4xx_it.c        原例程中断向量（SysTick 空）
│  └─ PWM_UI_Framework.uvprojx
├─ OBJ/                     编译输出
├─ 00-题目要求与坑点审查.md
├─ 01-代码框架与TODO说明.md  ← 本文
└─ README.md
```

---

## 3. 分层与调用关系

```
main()
 └─ APP_Init()
     ├─ NVIC 分组 / delay_init / uart_init
     ├─ TICK_Init()            TIM6 1ms
     ├─ TFTLCD_Init()          ILI9341 240x320
     ├─ PWM_Init()             TIM1_CH1/PA8，自检 1kHz/50%
     ├─ CAP_Init()             TIM5_CH1/PA0，PWM 输入模式
     ├─ BOOT_Run()             触摸硬件 + WELCOME 开始界面 + 需要时校准
     ├─ PWM2_Init()            TIM8_CH1/PC6（拓展1）
     ├─ SPWM_Init()            TIM3_CH1/PA6（拓展4，默认不启动）
     ├─ STOPWATCH/SWEEP/SNAKE_Init()
     └─ UI_Init()              页面表 + 首页
 └─ while(1) APP_Loop()
     ├─ UI_Loop()
     │   ├─ 每 15ms: TP_Scan → DOWN/MOVE/UP → 当前页 on_event
     │   ├─ 每轮: 当前页 tick(now)（秒表/扫频/贪吃蛇推进）
     │   └─ 每 40ms: 若 dirty → 当前页 draw()；键盘打开则最后画键盘
     └─ 每 100ms: CAP_Update()（捕获算法在这里）
```

**数据流**：
- UI 改 PWM：`页面 event → PWM_Set()/PWM_SetDuty() → TIM 寄存器`；UI 显示读 `g_pwm`。
- 捕获：`TIM5 ISR → s_period/s_high/seq → CAP_Update() → g_cap → 页面 draw`。
- 秒表：`STOPWATCH_* → TICK_Now()`；页面 tick 只负责 50ms 标脏。
- 扫频：`页面 START → SWEEP_Start()`；`页面 tick → SWEEP_Poll()` 推进状态机；结果在 `s_rows`。
- 贪吃蛇：`页面 tick → SNAKE_Tick()`；页面根据 `g_snake` 画格子。

---

## 4. 上电流程（APP_Init 的顺序很重要）

1. `NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2)`：先分组，再初始化任何中断。
2. `delay_init(168)`：触摸的软件 SPI 用 `delay_us`，必须先初始化。
3. `uart_init(115200)`：printf 调试。
4. `TICK_Init()`：TIM6 1ms。
5. `TFTLCD_Init()`：屏幕；必须在触摸之前，因为校准要画十字。
6. `PWM_Init()`：TIM1/PA8，自检 1kHz/50%。放在触摸之前，万一触摸校准阻塞，PWM 也已经在输出。
7. `CAP_Init()`：TIM5/PA0，PWM 输入模式。
8. `BOOT_Run()`：`TP_InitNoCalib()` 初始化触摸硬件并读校准参数 → 显示 `WELCOME` 开始界面 → 需要时 `TP_Adjust()` 四点校准 → 返回。
9. `PWM2_Init()` / `SPWM_Init()`：拓展，按开关。
10. `STOPWATCH_Init()` / `SWEEP_Init()` / `SNAKE_Init()`。
11. `UI_Init()`：清屏 + 主菜单。

> `TP_Init()` 仍然保留（会自动校准），但开机流程改用 `TP_InitNoCalib()`，这样才能先显示开始界面、再由用户决定是否校准。调试 UI 时如果不想进校准，可以临时让 `BOOT_Run()` 直接返回，或者把 EEPROM 校准好。

---

## 5. 主循环与实时性规则

`APP_Loop()` 只做四件事：
1. `UI_Loop()`：触摸/页面/重绘。
2. 每 100ms `CAP_Update()`：捕获计算。
3. 页面 tick 里的扫频/贪吃蛇推进。
4. 可选的周期 printf（默认关闭）。

**硬规则**：
- 不在主循环里 `delay_ms`、`while` 等触摸、等捕获。
- 不在中断里画屏、printf、浮点、长除法。
- 所有周期任务用 `TICK_Elapsed(&last, period)`。
- 一次重绘只画变化区域；不要每轮 `LCD_Clear`。
- 触摸扫描按 15ms 节流（驱动本身是毫秒级）。

---

## 6. UI 机制

### 6.1 事件
`UI_Loop` 每 15ms 调一次 `TP_Scan(0)`，翻译成：
- `UI_EV_DOWN`：第一次按下（会清 `TP_CATH_PRES`）；
- `UI_EV_MOVE`：移动超过 `UI_MOVE_THRESHOLD_PX`；
- `UI_EV_UP`：松开，坐标用最后一次有效值。

页面只处理这三种事件，不需要直接碰 `tp_dev`。

### 6.2 页面表
`ui_pages.c` 底部：
```c
const UI_PageOps g_ui_pages[PAGE_COUNT] = {
    { Home_Enter, 0, Home_Draw, Home_Event, 0 },
    ...
};
```
每个页面实现 `draw/on_event/tick` 中需要的几个；不需要的填 `0`。

### 6.3 重绘
- 页面改值后调 `UI_MarkDirty()`；
- `UI_Loop` 每 40ms 最多重绘一次；
- 换页时 `UI_GoPage` 整屏清一次（可接受）；同页内绝不整屏清。

### 6.4 键盘覆盖层
`g_keypad.active=1` 时：
- 所有触摸事件先给 `UI_Keypad_OnTouch`；
- 页面画完后，最后画 `UI_Keypad_Draw`（盖在上面）；
- 确认/取消后 `UI_Keypad_Close(commit)`，回调里再改实际参数。

### 6.5 滑条
`UI_Slider` 记录轨道位置、值、按下偏移 `grab_dx`、拖动状态。页面在 `on_event` 里调用 `UI_Slider_OnTouch`，返回 1 表示值变了，再调 `PWM_SetDuty`。

---

## 7. 页面地图与建议布局（240×320 竖屏）

```
HOME
 ├─ PWM         频率/占空比显示、键盘、0%/100%、滑条、ON/OFF
 ├─ CAPTURE     实测频率/占空比/周期/高电平/信号状态
 ├─ STOPWATCH   00:00.000、LAP/SPLIT、START/PAUSE/RESET
 ├─ PHASE       CH2 占空比、相位 0..360（5°档）、预设 0/90/180/270
 ├─ SWEEP       12 行表格：SET/MEAS/ERR + PASS/FAIL
 ├─ SPWM        50Hz 固定、START/STOP、载波/点数说明
 ├─ SNAKE       20×20 棋盘、分数、方向键、自动/重开
 └─ CALIB       触摸校准（平时不要自动进）
```

建议通用布局：
- y=0..21：状态栏（标题 + PWM 状态）
- y=24..250：内容区
- y=256..319：底部按钮/提示

PWM 页建议：
```
y=28   FREQ
y=52   actual freq (24号)
y=88   DUTY
y=112  actual duty (24号)
y=148  SET xxx / ACTUAL xxx (12号, 黄/青)
y=170  [FREQ] [DUTY] [0%] [100%]
y=218  slider
y=262  [ON/OFF] [APPLY] [BACK]
```

扫频表建议：
- 12 号字（6px/字符），一行 40 字符；
- 表头 1 行 + 12 行 ×16px = 208px；
- 底部 PASS/FAIL 大字 + 按钮。

贪吃蛇建议：
- 棋盘 20×20 ×12px = 240×240，y=40..279；
- y=0..39 分数/状态；y=280..319 方向键/暂停/重开/自动。

---

## 8. TODO 清单（按代码里的编号）

> 编号与源码注释里的 `TODO-xx` 对应。**TODO-10/11/14～22 已经给了"最小可用版本"**（按钮、状态栏、8 行菜单、各页数据展示、BACK、PWM OUT 开关）；表里写的是继续完善/美化的目标。TODO-12/13 仍是空白，需要你从零写。

| ID | 文件/函数 | 你要写什么 | 验收标准 |
|---|---|---|---|
| TODO-00 | `APP/app_config.h` | 所有可调参数（频率上下限、步进、超时、刷新率、颜色、布局、滤波参数） | 现场改参数只改这一处 |
| TODO-01 | `bsp_pwm.c: PWM_Set()` | 任意频率/占空比 → PSC/ARR/CCR，写入并反算实际值 | 1Hz～200kHz，10/50/90% 误差达标 |
| TODO-02 | `bsp_pwm.c: PWM_SetDuty()` | 0%/100% 边界、无窄脉冲、预装载/更新事件 | 示波器看恒低/恒高，无毛刺 |
| TODO-03 | `bsp_pwm.c: PWM_SetFreq()` | 改频保持占空比；先算 ARR 再算 CCR | 改频后占空比不跳 |
| TODO-04 | `app_config.h` / `bsp_pwm.c` | 500kHz 模式切换（模式、5% 步进、捕获量程） | 500k 可设可测，200k/1% 仍可用 |
| TODO-05 | `bsp_capture.c: CAP_Update()` | CCR1/CCR2 → 频率/占空比、平均、超时、有效性 | 1Hz～200kHz，2s 跟随，误差达标 |
| TODO-06 | `bsp_spwm.c` | 正弦表、`SPWM_Start()`、`TIM3_IRQHandler` | 50Hz 正弦，误差≤1%，持续输出 |
| TODO-07 | `app_stopwatch.c` | Start/Pause/Reset/Lap 状态机 | 暂停冻结、重置归零、圈/分段正确 |
| TODO-08 | `app_sweep.c: SWEEP_Poll()` | 非阻塞扫频状态机、误差判定、PASS/FAIL | ≥10 点表格，界面不卡 |
| TODO-09 | `app_snake.c` | 移动/吃食物/碰撞/计分/自动模式 | 手动可玩、计分重开、自动通关 |
| TODO-10 | `ui_core.c: UI_DrawButton()` | 最小按钮已实现；可继续美化圆角/按下位移/图标 | 按下/松开视觉明确，无残影 |
| TODO-11 | `ui_core.c: UI_DrawStatusBar()` | 最小状态栏已实现；可加右侧状态 | 标题 + 状态，换页不闪 |
| TODO-12 | `ui_core.c: UI_Keypad_Draw/OnTouch()` | 3×4 键盘绘制与点击逻辑（仍空白） | 布局正确、数字/删除/确认、有效值区分 |
| TODO-13 | `ui_core.c: UI_Slider_Draw/OnTouch()` | 滑条绘制、三态、grab_dx、clamp（仍空白） | 跟手、不跳变、拖出轨道不归零 |
| TODO-14 | `ui_pages.c: Home_*` | 最小 8 行菜单已实现；可加选中态/右侧当前值 | 所有页面可达、可返回 |
| TODO-15 | `ui_pages.c: Pwm_*` | 最小显示 + OUT 开关已实现；键盘/滑条/0%/100% 待补 | 参数可改、显示实际/编辑值 |
| TODO-16 | `ui_pages.c: Cap_*` | 最小数据显示页已实现；可加 HOLD/平均/波形 | NO SIGNAL 不冒充有效值 |
| TODO-17 | `ui_pages.c: Stopwatch_*` | 最小显示页已实现；START/PAUSE/LAP/RESET 按钮待补 | 毫秒在走、按钮正确 |
| TODO-18 | `ui_pages.c: Phase_*` | 最小显示页已实现；±5/预设按钮待补 | 0..360、5°档、调节不断流 |
| TODO-19 | `ui_pages.c: Sweep_*` | 最小状态页已实现；12 行表/START/ABORT 待补 | 表格上屏、判定正确 |
| TODO-20 | `ui_pages.c: Spwm_*` | 最小参数页已实现；START/STOP 待补 | 50Hz 可演示 |
| TODO-21 | `ui_pages.c: Snake_*` | 最小状态页已实现；棋盘/方向键/自动待补 | 手动可玩、自动通关 |
| TODO-22 | `ui_pages.c: Calib_*` | 全局右上角红色十字已实现重新校准；CALIB 页可只保留说明或加按钮 | 不会上电自动进 |
| TODO-23 | `bsp_pwm.c: PWM2_SetPhaseDeg()` | 相位同步方案 | 0/90/180/270/360，5°档，不断流 |
| TODO-24 | `bsp_capture.c`（可选） | 500k 捕获量程/平均/滤波 | 500k 下误差≤3% |
| TODO-25 | 全工程 | 最终集成：页面地图、演示顺序、异常保护 | 不按 RESET 一次演示完 |

### 已经实现、不需要你重写的部分
> 2026-11-09 更新：**TODO-09 / TODO-21（贪吃蛇）已完成** —— 全屏棋盘、触摸滑动/方向键/实体按键、计分/暂停/重开，以及哈密顿回路 + 安全捷径的自动通关模式。原理、参数和板上验收步骤见 [`02-贪吃蛇彩蛋说明.md`](02-贪吃蛇彩蛋说明.md)。
- `app_boot.c`：开机 `WELCOME` 界面、START/CALIBRATE、四点校准流程。
- `touch.c: TP_InitNoCalib()`：只初始化触摸硬件并读校准参数，不阻塞。
- `bsp_tick.c`：TIM6 1ms、`TICK_Now/Elapsed/DelayMs`。
- `bsp_capture.c: CAP_Init()`：TIM5 PWM 输入模式初始化、ISR 只存原始值。
- `bsp_pwm.c: PWM_Init()`：TIM1/PA8 初始化 + 1kHz/50% 自检。
- `bsp_pwm.c: PWM2_Init()`：TIM8/PC6 独立 1kHz/50%（相位未同步）。
- `bsp_spwm.c: SPWM_Init()`：TIM3/PA6 载波时基/OC 初始化（默认不启动）。
- `ui_core.c`：触摸事件、页面分发、脏标记、文字/填充工具、最小按钮/状态栏/BACK、键盘开关、滑条初始化。
- `ui_pages.c: Home_*`：最小 8 行菜单 + 页面跳转；`Pwm_*` 里有一个可用的 OUT ON/OFF。
- `app_main.c`：初始化顺序、主循环调度。

---

## 9. 调参表（都在 `app_config.h`）

| 宏 | 默认 | 作用 | 什么时候改 |
|---|---|---|---|
| `APP_ENABLE_PHASE` | 1 | 是否初始化第二路/相位 | 不做拓展1时改0 |
| `APP_ENABLE_SLIDER` | 1 | 滑动条开关 | 不做拓展2时改0 |
| `APP_ENABLE_SWEEP` | 1 | 扫频开关 | 不做拓展3时改0 |
| `APP_ENABLE_SPWM` | 1 | SPWM 开关 | 不做拓展4时改0 |
| `APP_ENABLE_EXT500K` | 0 | 500k 扩展 | 最后做拓展5时打开 |
| `APP_ENABLE_SNAKE` | 1 | 贪吃蛇开关 | 不做拓展6时改0 |
| `PWM_BRINGUP_TEST` | 1 | 上电固定 1kHz/50% | 正式算法做好后可关 |
| `UI_BRINGUP_HOME` | 0 | 旧版首页占位文字开关（当前已关闭，使用最小菜单） | 一般不用改 |
| `UI_BRINGUP_TOUCH_DOT` | 0 | 触摸画点调试 | 验证坐标时临时开 |
| `PWM_FREQ_MIN_HZ` | 1 | 频率下限 | 现场改参数 |
| `PWM_FREQ_MAX_HZ` | 200000 | 频率上限 | 现场改参数/500k |
| `PWM_DUTY_STEP_PCT` | 1 | 基本占空比步进 | 现场改参数 |
| `PWM_EXT_FREQ_MAX_HZ` | 500000 | 500k 上限 | 拓展5 |
| `PWM_EXT_DUTY_STEP_PCT` | 5 | 500k 步进 | 拓展5 |
| `CAP_TIMEOUT_MS` | 3000 | 捕获超时 | 低频/演示节奏 |
| `CAP_AVG_N` | 8 | 平均点数 | 噪声大/跟随慢时权衡 |
| `SWEEP_POINT_COUNT` | 12 | 扫频频点数 | 题目要求≥10 |
| `SWEEP_SETTLE_MS` | 400 | 每点等待 | 10Hz 以下要加长 |
| `SPWM_CARRIER_HZ` | 20000 | 载波 | 与点数一起改，保证 50Hz |
| `SPWM_POINTS` | 400 | 正弦表点数 | 同上 |
| `SPWM_AMP_PCT` | 90 | 调制深度 | 削顶/幅度小 |
| `SNAKE_STEP_MS` | 160 | 蛇速 | 手感 |
| `UI_TOUCH_SCAN_MS` | 15 | 触摸扫描周期 | UI 卡/响应慢 |
| `UI_REFRESH_MS` | 40 | 重绘周期 | 闪/卡 |
| `UI_MOVE_THRESHOLD_PX` | 2 | 拖动阈值 | 滑条太敏感 |
| `UI_COLOR_*` | 见文件 | 配色 | 视觉调整 |
| `UI_FONT_S/M/L` | 12/16/24 | 字体 | 布局调整 |

> 说明：`APP_ENABLE_PHASE` / `APP_ENABLE_SPWM` 会真正跳过外设初始化；`APP_ENABLE_SLIDER` / `APP_ENABLE_SWEEP` / `APP_ENABLE_SNAKE` 目前只是规划开关，页面骨架仍保留，方便你随时打开。要不要彻底裁掉代码，等基本盘稳定后再做。

---

## 10. 硬件接线

### 10.1 输出与测量

| 信号 | 板子引脚 | 接到 | 注意 |
|---|---|---|---|
| 主 PWM | PA8 (TIM1_CH1) | 示波器 CH1 | 共地；探头 10× |
| 第二路 PWM | PC6 (TIM8_CH1) | 示波器 CH2 | 拓展1；共地 |
| 捕获输入 | PA0 (TIM5_CH1) | 信号发生器输出 | ≤3.3V；共地；串 100Ω/钳位 |
| 扫频回环 | PA0 | 用杜邦线接 PA8 | 扫频时接；测外部信号时拔掉 |
| SPWM | PA6 (TIM3_CH1) | RC 滤波 → 示波器 | 滤波后看 50Hz |
| 串口 | PA9/PA10 | USB-TTL | 可选，调试用 |

> **不要同时**把信号发生器和 PA8 回环线接到 PA0，会互相驱动。用跳线/插拔切换。

### 10.2 简易 RC 滤波（SPWM）

- 目标：滤掉 20kHz 载波，保留 50Hz 正弦。
- 截止频率取 300～800Hz 之间，例如：
  - 一阶：R=1kΩ，C=330nF → fc≈482Hz；
  - 两阶串联更干净（截止频率会变，自己算/仿真）。
- 输出再接示波器；探头电容会影响截止频率。
- 这是"自制简易滤波电路"的硬件分，记得共地、限幅。

### 10.3 信号源设置

- 波形：方波；
- 幅度：**≤3.3V**（先调小再接入）；
- 频率：1Hz、1kHz、200kHz 等；
- 占空比：10%/50%/90%；
- 共地：信号源 GND 必须和板子 GND 相连。

---

## 11. 分阶段开发路线（配合编译开关）

### 阶段 0：上电自检（现在就能做）
1. 烧录当前框架。
2. 看开机是否显示 "WELCOME"：已校准有 START/CALIBRATE，未校准点一下进四点校准；然后进 "MAIN MENU" + 8 行菜单。
3. 示波器看 PA8：应为 1kHz/50%；PWM 页 OUT ON/OFF 应能开关输出。
4. 触摸屏幕，若 `UI_BRINGUP_TOUCH_DOT=1` 可在触点看到十字（默认关）。
5. 点状态栏右上角红色十字，应能重新进入四点校准。

### 阶段 1：PWM（TODO-01/02/03）
- 先做固定频率下的占空比，再做全频段。
- 每改一次，示波器测 10/50/90。
- 做完把 `PWM_BRINGUP_TEST` 改 0，让 `PWM_Set` 接管。

### 阶段 2：捕获（TODO-05）
- 先信号源 1kHz/50%，确认 CCR1/CCR2 数值。
- 再扩 1Hz 和 200kHz。
- 最后做 2s 跟随/超时。

### 阶段 3：UI 基础（TODO-10/11/12/14/15）
- 先画按钮/状态栏/菜单，能跳页。
- 再画 PWM 页 + 键盘。
- 最后加滑条（TODO-13）。

### 阶段 4：秒表（TODO-07/17）
- 用 tick 做状态机；页面 50ms 刷一次。

### 阶段 5：拓展
- 相位（TODO-23/18）：先想方案，再写代码。
- 扫频（TODO-08/19）：依赖捕获稳定。
- SPWM（TODO-06/20）：先做正弦表，再调滤波。
- 贪吃蛇（TODO-09/21）：先手动，再自动。
- 500k（TODO-04/24）：最后做，别破坏 200k/1%。

---

## 12. 编码、字库、Keil 注意事项

1. **界面字符串只用 ASCII**：`LCD_ShowString` 遇到中文会停；汉字只有"伍陆电子"4 个。
2. **新文件用 GBK 保存**：工程里大部分文件是 GBK；混用 UTF-8 会导致 Keil 里中文注释乱码。
3. **字体大小只有 12/16/24**：32 号字库有数据但 `LCD_ShowChar` 不支持。
4. **文字背景色**：`LCD_ShowChar` 用全局 `BACK_COLOR` 填背景，画字前必须设对。
5. **数字残影**：固定 `len`，或用 `LCD_ShowxNum(...,0x80)`。
6. **不要每轮 `LCD_Clear`**：只换页清一次。
7. **栈只有 1KB**：大数组全局；不要 malloc；不要深递归。
8. **不要用 SysTick 做节拍**：`delay.c` 已占用；用 TIM6。
9. **高级定时器要开 MOE**：TIM1/TIM8。
10. **中断要短**：捕获 20 万次/秒；SPWM 2 万次/秒。

---

## 13. 怎么加一个新页面/按钮/字段

### 加页面
1. `ui_core.h` 的 `UI_PageId` 加枚举（放在 `PAGE_COUNT` 前）。
2. `ui_pages.c` 写 `Xxx_Draw/Xxx_Event/Xxx_Tick`。
3. 在 `g_ui_pages[]` 里加一行。
4. 在 Home 菜单里加跳转。

### 加按钮
1. 定义 `UI_Rect r = {x,y,w,h};`
2. `UI_DrawButton(&r, "LABEL", pressed)`（已可用）。
3. 在 `on_event` 的 `UI_EV_DOWN` 里 `UI_HitTest(&r, ev->x, ev->y)`。
4. 按下时记 `pressed_id`，UP 时同键才触发（或 DOWN 直接触发，二选一）。

### 加数值字段
1. 固定位置/宽度/字体；
2. 用 `UI_NumColor`/`UI_NumPadColor` 画，`UI_COLOR_VALUE` 表示实际值，`UI_COLOR_EDIT` 表示编辑值；
3. 更新前先判断值有没有变，避免无意义重画；
4. 改完 `UI_MarkDirty()`。

---

## 14. 上电自检顺序（建议）

1. 串口 115200 看启动打印：LCD 尺寸、Touch init、System ready。
2. 屏幕首页文字是否完整、无花屏。
3. 示波器 PA8：1kHz/50%。
4. 触摸：开 `UI_BRINGUP_TOUCH_DOT`，点四个角，看十字是否跟手。
5. 信号源 1kHz/50% 接 PA0，串口看 `g_cap`（TODO-05 完成后）。
6. 再把 PA8 接 PA0，做扫频回环。

---

## 15. 下一步建议

1. 先打开 `APP/app_config.h`，确认引脚和你的板子一致（尤其 PA0/PA8/PC6/PA6）。
2. 烧当前框架，确认屏幕、触摸、PA8 自检波形。
3. 按 `TODO-01 → TODO-05 → TODO-12/14/15` 的顺序填。
4. 每完成一个 TODO，就在 `00-题目要求与坑点审查.md` 的自测表里打勾。
5. 基本盘 43 分稳了，再按性价比做拓展。

> 记住：**架构的价值是让你每次只改一个模块**。不要一上来把所有 TODO 都填了；先让 PWM 和捕获在示波器上准，再让界面好看。
