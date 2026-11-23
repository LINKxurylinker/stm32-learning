# STM32F407 示波器工程 —— 外设时钟 / GPIO 引脚 / 外设初始化 代码级溯源审计报告

| 项 | 内容 |
|---|---|
| 审计对象 | `2026创新实验室招新选拔软件第三题\2026创新实验室招新选拔软件第三题\ADC\ADC`（Keil 工程 `USER\Base.uvprojx`） |
| 芯片 / 器件 | STM32F407ZGTx（`USER\Base.uvprojx:17`） |
| 工程宏 | `STM32F40_41xxx, USE_STDPERIPH_DRIVER, __FPU_PRESENT=1, ARM_MATH_CM4`（`USER\Base.uvprojx:337`） |
| 标准外设库 | STM32F4xx StdPeriph V1.4.0（`USER\stm32f4xx_conf.h:5`） |
| 审计方式 | 只读源码；`read` / `grep` 仅用于定位，最终行号由对当前磁盘文件逐行重读核对；未修改任何 `.c/.h/工程` 文件 |
| 编码说明 | 旧 ALIENTEK 驱动为 GBK 编码；`USER\*.c`、`USER\system_stm32f4xx.c` 等为纯 ASCII。两种解码下换行结构一致，行号一致 |
| 报告写入 | 本文件为本次审计唯一写入文件 |
| 参考路径基准 | 下文 `文件:行` 均相对工程根目录 `...\ADC\ADC` |

## 0. 需求覆盖对照

| 审计要求 | 本报告位置 |
|---|---|
| RCC 时钟树（HSE/PLL/SYSCLK/HCLK/PCLK1/PCLK2）、ADC 时钟分频 | §1、§2 |
| 各外设总线时钟使能 | §2 |
| GPIO 每个实际使用引脚（模式/复用/上下拉/速度） | §3 |
| ADC1/ADC2 + DMA2 Stream0 + TIM3 | §4.1 |
| USART1 / USART2 | §4.2 |
| FSMC / LCD | §4.3 |
| 触摸 IIC、IIC/EEPROM | §4.4、§4.5 |
| SysTick / Delay | §4.6 |
| LED / KEY（仅列实际用到的） | §4.7、§7 |
| 每项：文件:行 + 变量名/类型/定义位置 + 库函数 + 关键参数 | §1~§7 |
| 全局引脚表 / 外设时钟表 / 调用链 / 变量索引 / 库函数索引 | §3 / §2 / §4 / §5 / §6 |
| 当前实际使用 vs 未使用/旧方案 | §7 |
| 配置与注释/文档矛盾 | §8 |
| 未确认项 | §9 |

## 1. 结论速览

- **系统时钟**：HSE 8 MHz → PLL（M=8，N=336，P=2，Q=7）→ SYSCLK 168 MHz；HCLK=168 MHz，PCLK1=42 MHz，PCLK2=84 MHz（`USER\system_stm32f4xx.c:316,326,329,331,371,598,602,605`；`USER\stm32f4xx.h:123`）。
- **ADC 时钟**：PCLK2 84 MHz / `ADC_Prescaler_Div4` = **21 MHz**（`USER\adc.c:261`），低于 F407 的 36 MHz 上限。
- **当前实际采集方案**：ADC1_IN4(PA4) + ADC2_IN5(PA5) **双规则同步**；TIM3 TRGO（84 MHz/420 = **200 kHz**）统一触发；ADC1 经 **DMA2 Stream0 / Channel0 / 32-bit / Circular / DMA mode 2** 从 `ADC->CDR` 打包搬运（`USER\adc.c:221-222,253,260-278,286-301,326-332`）。
- **当前实际串口**：**USART2** PA2/PA3（AF7），**460800** 8N1，TX 走 **DMA1 Stream6 / Channel4**，RX 走 512 B 环形缓冲 + USART2 中断；`printf` 被重定向到 USART2（`USER\main.c:14`；`SYSTEM\usart\usart.c:52-57,165-227,266-309`）。
- **当前实际显示**：**FSMC Bank1 NE4 + 16-bit SRAM + 模式 A**，`LCD_BASE = 0x6C000000 | 0x7E`（A6 作 RS），PB15 背光；使用 PD/PE/PF/PG 共 **20 个 FSMC 复用脚 + 1 个背光脚**（`HARDWARE\LCD\lcd.c:582-690,2402`；`HARDWARE\LCD\lcd.h:53,62`）。
- **当前实际触摸**：`UI_Init()` 通过函数指针 `tp_dev.init()` 调用 `TP_Init()`；`lcddev.id == 0x5510` 时走**电容屏**（GT9147，失败回退 OTT2001A，软件 IIC 用 **PB0/PF11**），否则走**电阻屏**（PB1/PB2/PB0/PC13/PF11 + 24C02 软件 IIC **PB8/PB9**）（`USER\ui.c:2163-2171`；`HARDWARE\TOUCH\touch.c:24-37,504-560`）。
- **当前实际延时**：**SysTick 轮询**（HCLK/8 = 21 MHz，fac_us=21），未开启 SysTick 中断（`SYSTEM\delay\delay.c:115-133,179-224`；`USER\stm32f4xx_it.c:142-145`）。
- **存在但当前未执行/未使用**：`uart_init`（USART1 PA9/PA10）、`LED_Init`（PF9/PF10）、`KEY_Init`/`KEY_Scan`（PE2/3/4、PA0）、DAC（已显式关闭）、DCMI（仅注释提及）、`HARDWARE\adc.h`（0 字节空文件）、4 个 `.wavefix_backup_*` 历史目录（未加入工程）。
- **旧方案证据**：备份 `.wavefix_backup_dual_pa5pa6_20261119\adc.c:219-220,239` 为 **PA5/ADC1_IN5 + PA6/ADC2_IN6**；当前代码已改为 **PA4/PA5**。**PA0 从未作为 ADC 通道出现在任何代码中**，只出现在文档备选（`..\dual-channel-pin.md:9,12`）和 `KEY` 的 WK_UP 宏（`HARDWARE\KEY\key.h:21`）。

## 2. RCC 时钟树与总线时钟

### 2.1 时钟树推导表

| 节点 | 频率 | 计算/依据 | 代码位置 |
|---|---|---|---|
| HSE 外部晶振 | 8 MHz | `HSE_VALUE 8000000` | `USER\stm32f4xx.h:123` |
| PLL 输入分频 M | 8 | VCO 输入 = 8/8 = 1 MHz | `USER\system_stm32f4xx.c:316` |
| PLL 倍频 N | 336 | VCO = 1 MHz × 336 = 336 MHz | `USER\system_stm32f4xx.c:329` |
| PLL 分频 P | 2 | SYSCLK = 336/2 = **168 MHz** | `USER\system_stm32f4xx.c:331` |
| PLL 分频 Q | 7 | 48 MHz（USB/SDIO/RNG 用，本工程未用） | `USER\system_stm32f4xx.c:326` |
| SYSCLK | **168 MHz** | 选择 PLL 作系统时钟，`SystemCoreClock=168000000` | `USER\system_stm32f4xx.c:617-618,653-657,371` |
| HCLK（AHB1） | **168 MHz** | `RCC_CFGR_HPRE_DIV1` | `USER\system_stm32f4xx.c:598` |
| PCLK2（APB2） | **84 MHz** | `RCC_CFGR_PPRE2_DIV2` | `USER\system_stm32f4xx.c:602` |
| PCLK1（APB1） | **42 MHz** | `RCC_CFGR_PPRE1_DIV4` | `USER\system_stm32f4xx.c:605` |
| APB1 定时器时钟 | **84 MHz** | F4 规则：APB 预分频≠1 时定时器时钟 ×2 | 推导自 `system_stm32f4xx.c:605`；`USER\adc.c:9,342` |
| APB2 定时器时钟 | 168 MHz | 同上（本工程未用 APB2 定时器） | 推导自 `system_stm32f4xx.c:602` |
| Flash 等待周期 | 5 WS + 预取/I-Cache/D-Cache | `FLASH_ACR_LATENCY_5WS` | `USER\system_stm32f4xx.c:644` |
| 调压器 | Scale 1 | `PWR->CR |= PWR_CR_VOS`（先开 PWR 时钟） | `USER\system_stm32f4xx.c:594-595` |
| SysTick 时钟 | **21 MHz** | HCLK/8（`SysTick_CLKSource_HCLK_Div8`） | `SYSTEM\delay\delay.c:120-121` |
| ADC 时钟（ADCCLK） | **21 MHz** | PCLK2 84 MHz / `ADC_Prescaler_Div4` | `USER\adc.c:261`；`FWLIB\src\stm32f4xx_adc.c`（`ADC_CommonInit` 写 CCR.ADCPRE） |
| FSMC 时钟 | 168 MHz | AHB3 无预分频，= HCLK | `HARDWARE\LCD\lcd.c:592` |
| USB/SDIO | 48 MHz | PLL_Q=7，但工程未使能 | `USER\system_stm32f4xx.c:326` |

> 时钟初始化入口：`SystemInit()` 在 `USER\system_stm32f4xx.c:448` 调用 `SetSysClock()`；`SetSysClock()` 依次开 HSE（`:573`）、配 APB 分频（`:598-605`）、写 PLLCFGR（`:617-618`）、开 PLL（`:621`）、切 SYSCLK 到 PLL（`:653-657`）。`main()` 中**没有**再次配置时钟（`USER\main.c:9-25`）。

### 2.2 外设总线时钟使能表

| 外设 | 总线 | 总线频率 | 使能函数 | 代码位置 | 状态 / 备注 |
|---|---|---|---|---|---|
| GPIOA | AHB1 | 168 MHz | `RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,…)` | `USER\adc.c:235`；`SYSTEM\usart\usart.c:165` | 实际使用（重复使能，无害） |
| GPIOB | AHB1 | 168 MHz | `RCC_AHB1PeriphClockCmd(…GPIOB…)` | `HARDWARE\LCD\lcd.c:591`；`HARDWARE\IIC\myiic.c:21`；`HARDWARE\TOUCH\ctiic.c:31`；`HARDWARE\TOUCH\touch.c:525`；`HARDWARE\TOUCH\gt9147.c:115`；`HARDWARE\TOUCH\ott2001a.c:83` | 实际使用（LCD PB15 + 触摸/EEPROM） |
| GPIOC | AHB1 | 168 MHz | `RCC_AHB1PeriphClockCmd(…GPIOC…)` | `HARDWARE\TOUCH\touch.c:525`；`gt9147.c:115`；`ott2001a.c:83` | 实际使用（PC13 触摸复位/片选） |
| GPIOD | AHB1 | 168 MHz | `RCC_AHB1PeriphClockCmd(…GPIOD…)` | `HARDWARE\LCD\lcd.c:591` | 实际使用（FSMC 数据/控制） |
| GPIOE | AHB1 | 168 MHz | `RCC_AHB1PeriphClockCmd(…GPIOE…)` | `HARDWARE\LCD\lcd.c:591` | 实际使用（FSMC D4~D12）；`KEY` 的再次使能未执行 |
| GPIOF | AHB1 | 168 MHz | `RCC_AHB1PeriphClockCmd(…GPIOF…)` | `HARDWARE\LCD\lcd.c:591`；`ctiic.c:31`；`myiic.c:21` | 实际使用（PF12 FSMC_A6、PF11 触摸数据） |
| GPIOG | AHB1 | 168 MHz | `RCC_AHB1PeriphClockCmd(…GPIOG…)` | `HARDWARE\LCD\lcd.c:591` | 实际使用（PG12 FSMC_NE4） |
| DMA1 | AHB1 | 168 MHz | `RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1,…)` | `SYSTEM\usart\usart.c:195` | 实际使用（Stream6 USART2_TX） |
| DMA2 | AHB1 | 168 MHz | `RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2,…)` | `USER\adc.c:235` | 实际使用（Stream0 ADC1） |
| FSMC | AHB3 | 168 MHz | `RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC,…)` | `HARDWARE\LCD\lcd.c:592` | 实际使用 |
| TIM3 | APB1 | 84 MHz（定时器时钟） | `RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,…)` | `USER\adc.c:237` | 实际使用（TRGO 触发） |
| DAC | APB1 | 42 MHz | `RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE)` | `USER\adc.c:237` | **先开后退**：`USER\adc.c:248` 显式 `DISABLE`，仅用于清 DAC 寄存器 |
| USART2 | APB1 | 42 MHz | `RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,…)` | `SYSTEM\usart\usart.c:166` | 实际使用 |
| ADC1 / ADC2 | APB2 | 84 MHz | `RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1\|…ADC2,…)` | `USER\adc.c:236` | 实际使用 |
| USART1 | APB2 | 84 MHz | `RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,…)` | `SYSTEM\usart\usart.c:79` | **未执行**（`uart_init` 无调用点） |
| PWR | APB1 | 42 MHz | 寄存器直写 `RCC->APB1ENR \|= RCC_APB1ENR_PWREN` | `USER\system_stm32f4xx.c:594` | 实际使用（VOS 配置） |

**未使能、也未使用的外设**：I2C1/I2C2（两套 IIC 均为 GPIO 软件模拟；`FWLIB\src\stm32f4xx_i2c.c` 不在工程文件列表中）、DCMI、SYSCFG/EXTI（无外部中断）、SPI、CAN、RTC、IWDG、USB。

## 3. GPIO 全局引脚表（仅当前实际生效的配置）

> 说明：模式/输出类型/上下拉/速度均取自对应 `GPIO_InitStructure` 赋值行；未单独写速度的以结构体实际赋值行所列值为准。AF 号：`GPIO_AF_USART1/2`=AF7，`GPIO_AF_FSMC`=AF12。

### 3.1 主执行路径（`main()` 一定执行到）

| 端口.脚 | 模式 / OType / PuPd / Speed | 复用信号 | 方向/用途 | 配置代码 |
|---|---|---|---|---|
| PA2 | AF / PP / UP / 50 MHz | USART2_TX（AF7） | 输出：命令与 WAVE/MEAS 应答（经 DMA1 Stream6） | `SYSTEM\usart\usart.c:168,171-176` |
| PA3 | AF / PP / UP / 50 MHz | USART2_RX（AF7） | 输入：串口命令接收 | `SYSTEM\usart\usart.c:169,171-176` |
| PA4 | 模拟 / — / NOPULL / 50 MHz | ADC1_IN4 | 输入：CH1 模拟采样（同时是 DAC_OUT1、DCMI_HSYNC） | `USER\adc.c:253-258` |
| PA5 | 模拟 / — / NOPULL / 50 MHz | ADC2_IN5 | 输入：CH2 模拟采样（同时是 DAC_OUT2，代码已关闭 DAC） | `USER\adc.c:253-258` |
| PB15 | OUT / PP / UP / 50 MHz | GPIO（`LCD_LED`） | 输出：LCD 背光，初始化点灯 | `HARDWARE\LCD\lcd.c:595-600`；`lcd.h:53`；点灯 `lcd.c:2402` |
| PD0 | AF / PP / UP / 100 MHz | FSMC_D2（AF12） | 双向：LCD 16 位数据 | `HARDWARE\LCD\lcd.c:602,607,630` |
| PD1 | AF / PP / UP / 100 MHz | FSMC_D3（AF12） | 双向：LCD 数据 | `lcd.c:602,607,631` |
| PD4 | AF / PP / UP / 100 MHz | FSMC_NOE（AF12） | 输出：LCD 读选通 | `lcd.c:602,607,632` |
| PD5 | AF / PP / UP / 100 MHz | FSMC_NWE（AF12） | 输出：LCD 写选通 | `lcd.c:602,607,633` |
| PD8 | AF / PP / UP / 100 MHz | FSMC_D13（AF12） | 双向：LCD 数据 | `lcd.c:602,607,634` |
| PD9 | AF / PP / UP / 100 MHz | FSMC_D14（AF12） | 双向：LCD 数据 | `lcd.c:602,607,635` |
| PD10 | AF / PP / UP / 100 MHz | FSMC_D15（AF12） | 双向：LCD 数据 | `lcd.c:602,607,636` |
| PD14 | AF / PP / UP / 100 MHz | FSMC_D0（AF12） | 双向：LCD 数据 | `lcd.c:602,607,637` |
| PD15 | AF / PP / UP / 100 MHz | FSMC_D1（AF12） | 双向：LCD 数据 | `lcd.c:602,607,638` |
| PE7 | AF / PP / UP / 100 MHz | FSMC_D4（AF12） | 双向：LCD 数据 | `lcd.c:609,614,640` |
| PE8 | AF / PP / UP / 100 MHz | FSMC_D5（AF12） | 双向：LCD 数据 | `lcd.c:609,614,641` |
| PE9 | AF / PP / UP / 100 MHz | FSMC_D6（AF12） | 双向：LCD 数据 | `lcd.c:609,614,642` |
| PE10 | AF / PP / UP / 100 MHz | FSMC_D7（AF12） | 双向：LCD 数据 | `lcd.c:609,614,643` |
| PE11 | AF / PP / UP / 100 MHz | FSMC_D8（AF12） | 双向：LCD 数据 | `lcd.c:609,614,644` |
| PE12 | AF / PP / UP / 100 MHz | FSMC_D9（AF12） | 双向：LCD 数据 | `lcd.c:609,614,645` |
| PE13 | AF / PP / UP / 100 MHz | FSMC_D10（AF12） | 双向：LCD 数据 | `lcd.c:609,614,646` |
| PE14 | AF / PP / UP / 100 MHz | FSMC_D11（AF12） | 双向：LCD 数据 | `lcd.c:609,614,647` |
| PE15 | AF / PP / UP / 100 MHz | FSMC_D12（AF12） | 双向：LCD 数据 | `lcd.c:609,614,648` |
| PF12 | AF / PP / UP / 100 MHz | FSMC_A6（AF12） | 输出：LCD 命令/数据选择（RS） | `lcd.c:616-621,650` |
| PG12 | AF / PP / UP / 100 MHz | FSMC_NE4（AF12） | 输出：LCD 片选（Bank1 NORSRAM4） | `lcd.c:623-628,651` |

> 引脚掩码核对：PD 掩码 `(3<<0)|(3<<4)|(7<<8)|(3<<14)` = PD0,1,4,5,8,9,10,14,15（`lcd.c:602`）；PE 掩码 `0x1FF<<7` = PE7~15（`lcd.c:609`）。

### 3.2 触摸相关引脚（由 `lcddev.id` 在运行时决定分支）

`TP_Init()` 首先判断 `lcddev.id == 0X5510`（`HARDWARE\TOUCH\touch.c:508`）：是 → 电容屏分支；否 → 电阻屏分支。两分支引脚集合不同：

| 分支 | 端口.脚 | 模式 / OType / PuPd / Speed | 用途 | 配置代码 |
|---|---|---|---|---|
| 电容屏（0x5510） | PB1 | IN / PP / UP（后改 NOPULL）/ 100 MHz | GT9147/OTT2001A 中断脚（`GT_INT`/`OTT_INT`） | `gt9147.c:117-122,134-137`；`ott2001a.c:86-91`；`gt9147.h:20` |
| 电容屏 | PC13 | OUT / PP / UP / 100 MHz | 触摸芯片复位（`GT_RST`/`OTT_RST`） | `gt9147.c:124-126,129-132`；`ott2001a.c:93-101`；`gt9147.h:19` |
| 电容屏 | PB0 | OUT / PP / UP / 速度未赋值 | 电容触摸软件 IIC SCL（`CT_IIC_SCL`） | `ctiic.c:33-37`；`ctiic.h:26` |
| 电容屏 | PF11 | OUT / PP / UP / 速度未赋值 | 电容触摸软件 IIC SDA（`CT_IIC_SDA`，可切输入） | `ctiic.c:39-41`；`ctiic.h:23-28` |
| 电阻屏（其它 ID） | PB1 | IN / PP / UP / 100 MHz | T_PEN（触摸笔中断） | `touch.c:528-533`；`touch.h:58` |
| 电阻屏 | PB2 | IN / PP / UP / 100 MHz | T_MISO | `touch.c:528-533`；`touch.h:59` |
| 电阻屏 | PB0 | OUT / PP / UP / 100 MHz | T_SCK | `touch.c:535-537`；`touch.h:61` |
| 电阻屏 | PC13 | OUT / PP / UP / 100 MHz | T_CS | `touch.c:539-541`；`touch.h:62` |
| 电阻屏 | PF11 | OUT / PP / UP / 100 MHz | T_MOSI | `touch.c:543-545`；`touch.h:60` |
| 电阻屏（EEPROM） | PB8 | OUT / PP / UP / 100 MHz | 24C02 软件 IIC SCL（`IIC_SCL`） | `myiic.c:21-29`；`myiic.h:21` |
| 电阻屏（EEPROM） | PB9 | OUT / PP / UP / 100 MHz | 24C02 软件 IIC SDA（`IIC_SDA`，可切输入） | `myiic.c:21-29`；`myiic.h:18-23` |

> 电容屏分支在 `touch.c:520` 直接 `return`，**不会执行** `AT24CXX_Init()`（`touch.c:549`），即电容屏下 PB8/PB9 不被配置。EEPROM 的 24C02 校准参数保存地址：基址 40，标记在 40+14=54，方向标记在 55（`touch.c:226-231,239-245`；`ui.c:2179` 读地址 55）。

## 4. 按外设的配置调用链

### 4.1 ADC1 + ADC2 + DMA2 Stream0 + TIM3（当前主采集链路）

**调用链**：
`main()`（`USER\main.c:9`）→ `NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2)`（`:11`）→ `delay_init(168)`（`:12`）→ `uart2_init(460800)`（`:14`）→ `ADC1_Init()`（`:16`）→ `CMD_Init()`（`:17`）→ `UI_Init()`（`:18`）→ 主循环 `CMD_Task()`/`UI_Task()`（`:22-23`）。

`ADC1_Init()`（`USER\adc.c:225-333`）内部顺序：

| 步骤 | 内容 | 代码位置 |
|---|---|---|
| 1 | 复位软件采样率 `s_sample_rate = 200000` | `adc.c:227` |
| 2 | 声明局部配置结构体：`GPIO_InitTypeDef`、`ADC_CommonInitTypeDef`、`ADC_InitTypeDef`、`DMA_InitTypeDef`、`TIM_TimeBaseInitTypeDef`、`NVIC_InitTypeDef` | `adc.c:228-233` |
| 3 | 使能 GPIOA、DMA2（AHB1），ADC1、ADC2（APB2），TIM3、DAC（APB1） | `adc.c:235-237` |
| 4 | **中和 DAC**：`DAC->CR = DAC_CR_BOFF1\|DAC_CR_BOFF2`、`DHR12R1/2=0`，然后关闭 DAC 时钟 | `adc.c:243-248` |
| 5 | PA4、PA5 配为模拟输入（NOPULL，50 MHz） | `adc.c:253-258` |
| 6 | ADC 公共配置：双规则同步、Div4、DMA mode 2、两采样间隔 5 cycles | `adc.c:260-264` |
| 7 | ADC1：12 bit、非扫描、单次、上升沿、`T3_TRGO`、右对齐、1 个转换 | `adc.c:266-273` |
| 8 | ADC2：沿用上面参数，仅把外部触发边沿改为 `None`（从机由 ADC1 触发） | `adc.c:275-278` |
| 9 | 设置采样时间并配置通道：ADC1→`ADC_Channel_4` rank1；ADC2→`ADC_Channel_5` rank1 | `adc.c:280,221-222` |
| 10 | DMA2 Stream0：Channel0、外设 `&ADC->CDR`、内存 `g_adc_dma_buf`、P2M、4096 字、Word/Word、Circular、High、FIFO off、单次 burst | `adc.c:284-301` |
| 11 | DMA 中断 HT/TC 使能并使能流 | `adc.c:303-304` |
| 12 | NVIC：`DMA2_Stream0_IRQn`，抢占 1 / 子 0 | `adc.c:306-310` |
| 13 | 清 OVR → `ADC_DMACmd(ADC1)` → 置 DDS（`ADC_MultiModeDMARequestAfterLastTransferCmd`）→ 先使能 ADC2 再 ADC1 | `adc.c:312-320` |
| 14 | `delay_ms(1)` 等 ADC 稳定 | `adc.c:322` |
| 15 | TIM3：PSC=0、ARR=419、向上计数、CKD/1；TRGO=Update；启动 | `adc.c:325-332` |

**关键参数表**

| 参数 | 值 | 代码位置 |
|---|---|---|
| ADC 模式 | `ADC_DualMode_RegSimult`（双规则同时） | `adc.c:260` |
| ADC 时钟分频 | `ADC_Prescaler_Div4`（84/4=21 MHz） | `adc.c:261` |
| 多重 ADC DMA 模式 | `ADC_DMAAccessMode_2`（32 位打包，ADC1 低 16 位 / ADC2 高 16 位） | `adc.c:262`；解包 `adc.c:69-70` |
| 两采样延迟 | `ADC_TwoSamplingDelay_5Cycles` | `adc.c:263` |
| 分辨率 | `ADC_Resolution_12b` | `adc.c:266` |
| 扫描 / 连续 | `DISABLE` / `DISABLE` | `adc.c:267-268` |
| 外部触发 | `ADC_ExternalTrigConv_T3_TRGO`，`Rising` | `adc.c:269-270` |
| 对齐 / 转换数 | `ADC_DataAlign_Right` / 1 | `adc.c:271-272` |
| ADC2 触发边沿 | `ADC_ExternalTrigConvEdge_None` | `adc.c:277` |
| 采样时间分档 | fs≤40 k：`480Cycles`；fs≤100 k：`84Cycles`；否则 `15Cycles` | `adc.c:214-219` |
| DMA 数据宽度 | 外设/内存均 `Word`（4 B/次） | `adc.c:293-294` |
| DMA 缓冲 | `g_adc_dma_buf[4096]`（16 KB，2 帧×2048 对） | `adc.c:11`；`adc.h:10-11` |
| DMA 模式/优先级 | `DMA_Mode_Circular` / `DMA_Priority_High` | `adc.c:295-296` |
| DMA 中断处理 | HT → `s_frame_offset=0`；TC → `s_frame_offset=2048`；均置 `s_frame_ready=1` | `adc.c:18-33` |
| TIM3 计数 | PSC=0，ARR=419，计数器 84 MHz → 200 kHz | `adc.c:326-327`；注释 `adc.c:9` |
| TIM3 TRGO | `TIM_TRGOSource_Update` | `adc.c:331` |
| 帧长度 | `ADC_FRAME_LEN 2048` | `adc.h:11` |
| 软件启动 | **未使用** `ADC_SoftwareStartConvCmd`（全工程无调用） | grep 结果为空 |

**采样率运行期修改**（`ADC1_SetSampleRate()`，`adc.c:335-388`）：限制 fs∈[1000, 500000]（`:339-340`）→ `arr = 84 MHz/fs - 1`（`:342-344`）→ 停 ADC1/ADC2/DMA/TIM3（`:350-353`）→ 直接写 `TIM3->PSC/ARR`，`EGR=UG` 重载，清更新标志（`:355-358`）→ 清 OVR/EOC/DMA 标志（`:360-365`）→ 应用新采样时间（`:371-372`）→ 重新使能 DMA/DDS/ADC（`:374-380`）→ `delay_us(10)`（`:383`）→ 开 TIM3（`:385`）→ 记录 `s_sample_rate`（`:387`）。该函数被 `cmd.c:162`（手动时基）、`cmd.c:742`（自动时基低频逐次减半）、`cmd.c:928`（自动量程改采样率）调用。

**中断优先级**（`NVIC_PriorityGroup_2`，`main.c:11`；抢占级 0~3）：

| 中断 | 抢占 / 子 | 代码位置 | 状态 |
|---|---|---|---|
| DMA2_Stream0 | 1 / 0 | `adc.c:306-310` | 使用中 |
| DMA1_Stream6（USART2 TX） | 2 / 0 | `usart.c:221-225` | 使用中 |
| USART2 | 3 / 0 | `usart.c:188-192` | 使用中 |
| USART1 | 3 / 3 | `usart.c:110-114` | 未使用（`uart_init` 未调用） |

### 4.2 USART1 / USART2

**USART2（当前实际使用）** —— `main.c:14` → `uart2_init(460800)` → `usart.c:158-228`：

| 项 | 内容 | 代码位置 |
|---|---|---|
| 时钟 | GPIOA（AHB1）、USART2（APB1 42 MHz） | `usart.c:165-166` |
| 引脚复用 | PA2→`GPIO_AF_USART2`，PA3→`GPIO_AF_USART2` | `usart.c:168-169` |
| GPIO | PA2\|PA3，AF/PP/UP/50 MHz | `usart.c:171-176` |
| 帧格式 | 8 数据位、1 停止位、无校验、无流控、Rx\|Tx | `usart.c:178-183` |
| 波特率 | **460800**（PCLK1 42 MHz、16 倍过采样；理论 USARTDIV=5.696，实际约 461.5 kbps，误差约 +0.16%） | `usart.c:178`；`main.c:14` |
| 中断 | `USART_IT_RXNE`，NVIC 抢占 3 / 子 0 | `usart.c:187-192` |
| TX DMA | `DMA1_Stream6`，`DMA_Channel_4`，外设 `&USART2->DR`，内存 `g_uart2_tx_buf`，M2P，Byte/Byte，`DMA_Mode_Normal`，Medium，FIFO off | `usart.c:197-214` |
| DMA 中断/NVIC | TC 中断；NVIC 抢占 2 / 子 0 | `usart.c:219-225` |
| USART DMA 请求 | `USART_DMAReq_Tx` | `usart.c:227` |
| 发送缓冲 | `char g_uart2_tx_buf[12288]` | `usart.c:230`；`SYSTEM\usart\usart.h:11` |
| 发送 API | `uart2_send_dma(len)`：禁用流→清标志→设长度→置忙→使能流 | `usart.c:238-254` |
| 发送完成 | `DMA1_Stream6_IRQHandler` 清 TC、关流、清忙标志 | `usart.c:256-264` |
| 接收 | 512 B 环形缓冲 + `USART2_IRQHandler`（满则置溢出） | `usart.c:267-272,292-309` |
| `printf` | `fputc` 轮询 `USART2->SR.TXE` 后写 `USART2->DR`（阻塞式，非 DMA） | `usart.c:52-57` |

**USART1（代码存在、当前未使用）** —— `uart_init()`（`usart.c:72-118`）：

| 项 | 内容 | 代码位置 |
|---|---|---|
| 时钟 | GPIOA、USART1（APB2 84 MHz） | `usart.c:78-79` |
| 引脚 | PA9/PA10 → `GPIO_AF_USART1`，AF/PP/UP/50 MHz | `usart.c:82-91` |
| 参数 | 波特率由入参决定；8N1；Rx\|Tx | `usart.c:94-99` |
| 中断 | RXNE + NVIC 抢占 3 / 子 3 | `usart.c:107-114` |
| 接收缓冲 | `USART_RX_BUF[200]`、`USART_RX_STA`，`EN_USART1_RX=1` | `usart.c:63-68`；`SYSTEM\usart\usart.h:7-8` |
| 调用点 | **无**（全工程只有定义与声明） | grep 仅 `usart.c:72`、`SYSTEM\usart\usart.h:18` |

### 4.3 FSMC / LCD

**调用链**：`UI_Init()`（`USER\ui.c:2159`）→ `LCD_Init()`（`ui.c:2163`）→ `LCD_Display_Dir(1)`（`:2164`）→ `LCD_Scan_Dir(L2R_U2D)`（`:2165`）→ `LCD_Clear(C_BG)`（`:2166`）。

`LCD_Init()`（`HARDWARE\LCD\lcd.c:582-2404`）：

| 步骤 | 内容 | 代码位置 |
|---|---|---|
| 1 | 使能 GPIOB/D/E/F/G（AHB1）与 FSMC（AHB3） | `lcd.c:591-592` |
| 2 | PB15 推挽输出（背光） | `lcd.c:595-600` |
| 3 | PD0,1,4,5,8,9,10,14,15 复用 AF/PP/UP/100 MHz | `lcd.c:602-607` |
| 4 | PE7~15 复用 AF/PP/UP/100 MHz | `lcd.c:609-614` |
| 5 | PF12 复用 AF/PP/UP/100 MHz | `lcd.c:616-621` |
| 6 | PG12 复用 AF/PP/UP/100 MHz | `lcd.c:623-628` |
| 7 | 20 条 `GPIO_PinAFConfig(..., GPIO_AF_FSMC)`（PD×9、PE×9、PF12、PG12） | `lcd.c:630-651` |
| 8 | 读时序：ADDSET=0xF、DATAST=60、模式 A | `lcd.c:654-660` |
| 9 | 写时序：ADDSET=9、DATAST=8、模式 A | `lcd.c:663-669` |
| 10 | `FSMC_NORSRAMInit`：Bank1 NORSRAM4、SRAM、16 bit、非复用、写使能、扩展模式（读写不同时序） | `lcd.c:672-688` |
| 11 | `FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE)` | `lcd.c:690` |
| 12 | `delay_ms(50)`，读 ID（9341/7789/5310/5510/1963/7796/9806 等） | `lcd.c:692-772` |
| 13 | 命中支持列表后重写 `FSMC_Bank1E->BWTR[6]`（ADDSET 字段=4、DATAST 字段=4） | `lcd.c:774-788` |
| 14 | `printf(" LCD ID:%x\r\n", ...)`（走 USART2） | `lcd.c:789` |
| 15 | 按 ID 做屏初始化，默认竖屏 `LCD_Display_Dir(0)`，`LCD_LED=1` 开背光，`LCD_Clear(WHITE)` | `lcd.c:2399-2403` |

**LCD 地址映射**：`LCD_BASE = (u32)(0x6C000000 | 0x0000007E)`（`HARDWARE\LCD\lcd.h:62`），`LCD_TypeDef{LCD_REG;LCD_RAM;}`（`lcd.h:55-59`），`LCD_LED = PBout(15)`（`lcd.h:53`）。即 FSMC Bank1 NE4 基址 0x6C000000，A6 作命令/数据选择。

### 4.4 触摸（GT9147 / OTT2001A / 电阻屏）与触摸 IIC

**调用链**：`UI_Init()`（`ui.c:2159`）→ `tp_dev.init()`（`ui.c:2171`）→ 函数指针 `TP_Init`（`touch.c:24-37` 初始化，定义在 `:504`）。

`TP_Init()` 分支（`touch.c:504-560`）：

| 分支 | 条件 | 关键动作 | 代码位置 |
|---|---|---|---|
| 电容屏 | `lcddev.id == 0X5510` | ① `GT9147_Init()==0` → `tp_dev.scan=GT9147_Scan`；② 否则 `OTT2001A_Init()` → `tp_dev.scan=OTT2001A_Scan`；③ 置 `touchtype` 电容位与横竖屏位；④ 直接 `return 0`（不做四点校准、**不初始化 24C02**） | `touch.c:508-520` |
| 电阻屏 | 其它 ID | ① 使能 GPIOB/C/F；② PB1/PB2 上拉输入，PB0/PC13/PF11 推挽输出；③ 读一次坐标；④ `AT24CXX_Init()`；⑤ `TP_Get_Adjdata()` 判断是否已校准 | `touch.c:525-560` |

**电容屏子链**：

| 外设/步骤 | 内容 | 代码位置 |
|---|---|---|
| GT9147 初始化 | 使能 GPIOB/C；PB1 上拉输入（后改浮空）；PC13 推挽输出；`CT_IIC_Init()`；`GT_RST=0/1` 复位；读 PID `0x8140`，判 `"9147"`；必要时下发配置表 | `gt9147.c:110-159` |
| GT9147 引脚宏 | `GT_RST=PCout(13)`，`GT_INT=PBin(1)` | `gt9147.h:19-20` |
| OTT2001A 初始化 | 使能 GPIOB/C；PB1 上拉输入；PC13 推挽输出；`CT_IIC_Init()`；复位延时 100 ms；读控制寄存器判定 | `ott2001a.c:78-107` |
| OTT 引脚宏 | `OTT_RST=PCout(13)`，`OTT_INT=PBin(1)` | `ott2001a.h:19-20` |
| 电容触摸 IIC | 使能 GPIOB/F；PB0 推挽输出（SCL）；PF11 推挽输出（SDA，可切输入）；位带宏 `CT_IIC_SCL/CT_IIC_SDA/CT_READ_SDA` | `ctiic.c:27-43`；`ctiic.h:23-28` |
| 触摸扫描 | `ui_touch_scan()` 每轮调用 `tp_dev.scan(0)`，读 `tp_dev.sta & TP_PRES_DOWN`、`tp_dev.x[0]/y[0]` | `ui.c:2123-2153` |

### 4.5 IIC / EEPROM（AT24C02）

| 项 | 内容 | 代码位置 |
|---|---|---|
| 调用链 | 电阻屏分支 `TP_Init()` → `AT24CXX_Init()` → `IIC_Init()` | `touch.c:549`；`24cxx.c:17-20`；`myiic.c:17` |
| 引脚 | PB8=SCL，PB9=SDA（推挽输出，上拉，100 MHz） | `myiic.c:21-29`；`myiic.h:21-23` |
| 方向切换 | `SDA_IN()/SDA_OUT()` 直接写 `GPIOB->MODER` 第 9 脚 | `myiic.h:18-19` |
| 器件类型 | `EE_TYPE = AT24C02`（256 B） | `24cxx.h:27` |
| 器件地址 | 写 `0xA0`、读 `0xA1`（≤24C16 时用 `0xA0+((addr/256)<<1)`） | `24cxx.c:33,38,55` |
| 读取接口 | `AT24CXX_ReadOneByte/Read/ReadLenByte/Check` | `24cxx.c:24-137` |
| 校准数据 | 基址 40，14 B 校准参数 + 地址 54 标记 `0x0A` + 地址 55 方向标记（横屏 `0x16`/竖屏 `0x11`） | `touch.c:226-231` |
| UI 方向校验 | 仅电阻屏（`touchtype&0x80==0`）读地址 55 与当前方向比对，不一致则重校准 | `ui.c:2176-2186` |

### 4.6 SysTick / Delay

| 项 | 内容 | 代码位置 |
|---|---|---|
| 初始化调用 | `delay_init(168)` | `main.c:12` |
| SysTick 时钟源 | `SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8)` → 21 MHz | `delay.c:120` |
| us 系数 | `fac_us = SYSCLK/8 = 21` | `delay.c:121` |
| ms 系数 | `fac_ms = (u16)fac_us*1000 = 21000` | `delay.c:131` |
| 中断 | 非 OS 路径**不开启** SysTick 中断；`SysTick_Handler` 为空函数 | `delay.c:117-132`；`stm32f4xx_it.c:142-145` |
| `delay_us` | 直接写 `SysTick->LOAD=nus*fac_us`，轮询 `COUNTFLAG`，用后关计数器并清 VAL | `delay.c:179-191` |
| `delay_ms` | 拆成 540 ms 段调用 `delay_xms()`（避免 24 位 LOAD 溢出） | `delay.c:213-224` |
| `delay_xms` | `LOAD=nms*fac_ms`，轮询 COUNTFLAG | `delay.c:198-210` |
| 上限 | `2^24/21 ≈ 798915 µs`（注释） | `delay.c:138,178,194-197` |

### 4.7 LED / KEY（代码存在、当前未初始化）

见 §7 表格：`LED_Init()`（PF9/PF10，`led.c:17-33`，宏 `led.h:20-21`）与 `KEY_Init()`（PE2/3/4 上拉输入、PA0 下拉输入，`key.c:17-35`；`KEY_Scan()` `key.c:45-59`；宏 `key.h:18-21`）**均无调用点**，因此 PF9/PF10/PE2/PE3/PE4/PA0 在当前固件中保持复位后的默认状态（浮空输入），不构成实际占用。

## 5. 变量 / 类型索引

### 5.1 类型别名与位带宏

| 名称 | 类型/宏 | 定义位置 | 说明 |
|---|---|---|---|
| `u8/u16/u32` | `uint8_t/uint16_t/uint32_t` | `SYSTEM\sys\sys.h:26-28` | 工程统一整型别名 |
| `s32/s16/s8` | `int32_t/int16_t/int8_t` | `sys.h:29-31` | 有符号别名 |
| `vu16` | `volatile unsigned short` | `sys.h:32` | LCD 读写用 |
| `PAout(n)…PIin(n)` | 位带宏 | `sys.h:38-89` | 软件 IIC/触摸/LED 的引脚操作基础 |
| `_lcd_dev` | 结构体（width/height/id/dir/wramcmd/setxcmd/setycmd） | `HARDWARE\LCD\lcd.h:33-42` | LCD 参数集 |
| `LCD_TypeDef` | 结构体（LCD_REG/LCD_RAM） | `lcd.h:55-59` | FSMC 地址映射 |
| `_m_tp_dev` | 结构体（init/scan/adjust/x/y/sta/xfac/yfac/xoff/yoff/touchtype） | `HARDWARE\TOUCH\touch.h:28-53` | 触摸设备对象 |

### 5.2 全局/静态变量

| 变量 | 类型 | 定义位置 | 用途 |
|---|---|---|---|
| `g_adc_dma_buf[4096]` | `volatile u32` 数组 | `USER\adc.c:11`（长度宏 `adc.h:10`） | DMA2 Stream0 目的缓冲（2 帧×2048 对） |
| `s_frame_ready` | `static volatile u8` | `adc.c:12` | 半满/全满帧就绪标志 |
| `s_frame_offset` | `static volatile u16` | `adc.c:13` | 0 或 2048，指向 DMA 缓冲半区 |
| `s_frame[2][2048]` | `static u16` | `adc.c:14` | 中值滤波后的显示帧 |
| `s_raw_frame[2][2048]` | `static u16` | `adc.c:15` | 解包后的原始帧 |
| `s_sample_rate` | `static u32` | `adc.c:16`（初值 200000） | 当前采样率，供 `ADC1_GetSampleRate()` |
| `g_uart2_tx_buf[12288]` | `char` 数组 | `SYSTEM\usart\usart.c:230`（长度宏 `SYSTEM\usart\usart.h:11`） | USART2 DMA 发送缓冲 |
| `s_uart2_tx_busy` | `static volatile u8` | `usart.c:231` | 发送忙标志 |
| `s_rx_ring[512]` | `static volatile u8` | `usart.c:269` | USART2 接收环形缓冲 |
| `s_rx_head/s_rx_tail` | `static volatile u16` | `usart.c:270-271` | 环形缓冲头尾 |
| `s_rx_overflow` | `static volatile u8` | `usart.c:272` | 接收溢出标志 |
| `USART_RX_BUF[200]` | `u8` | `usart.c:63`（长度宏 `SYSTEM\usart\usart.h:7`） | USART1 接收缓冲（未使用） |
| `USART_RX_STA` | `u16` | `usart.c:68` | USART1 接收状态（未使用） |
| `lcddev` | `_lcd_dev` | `HARDWARE\LCD\lcd.c:39`（类型 `lcd.h:33-42`，外部声明 `lcd.h:45`） | LCD 宽高/ID/方向 |
| `POINT_COLOR` / `BACK_COLOR` | `u16` | `lcd.c:34-35` | 画笔/背景色 |
| `tp_dev` | `_m_tp_dev` | `HARDWARE\TOUCH\touch.c:24-37`（类型 `touch.h:28-53`） | 触摸设备，函数指针 init/scan/adjust |
| `CMD_RDX` / `CMD_RDY` | `u8` | `touch.c:39-40` | 电阻屏 X/Y 读命令字 |
| `fac_us` | `static u8` | `SYSTEM\delay\delay.c:35` | 每 µs 的 SysTick 节拍数（21） |
| `fac_ms` | `static u16` | `delay.c:36` | 每 ms 的 SysTick 节拍数（21000） |
| `g_tb_us` / `g_capture_tb_us` | `static u32` | `USER\cmd.c:94-95` | 当前时基（µs/div）与采集时基 |
| `g_disp_span` | `static u16` | `cmd.c:96` | 显示窗口点数 |

### 5.3 局部配置结构体（配置模板，栈上变量）

| 变量 | 类型 | 定义位置 |
|---|---|---|
| `GPIO_InitStructure` | `GPIO_InitTypeDef` | `adc.c:228`；`usart.c:74,160`；`lcd.c:586`；`touch.c:506`；`gt9147.c:113`；`ott2001a.c:81`；`ctiic.c:29`；`myiic.c:19`；`led.c:19`；`key.c:20` |
| `ADC_CommonInitStructure` | `ADC_CommonInitTypeDef` | `adc.c:229` |
| `ADC_InitStructure` | `ADC_InitTypeDef` | `adc.c:230` |
| `DMA_InitStructure` | `DMA_InitTypeDef` | `adc.c:231`；`usart.c:162` |
| `TIM_TimeBaseStructure` | `TIM_TimeBaseInitTypeDef` | `adc.c:232` |
| `NVIC_InitStructure` | `NVIC_InitTypeDef` | `adc.c:233`；`usart.c:76,163` |
| `FSMC_NORSRAMInitStructure` | `FSMC_NORSRAMInitTypeDef` | `lcd.c:587` |
| `readWriteTiming` / `writeTiming` | `FSMC_NORSRAMTimingInitTypeDef` | `lcd.c:588-589` |

## 6. 标准外设库函数索引

| 库函数 | 调用位置（文件:行） | 关键实参/说明 |
|---|---|---|
| `RCC_AHB1PeriphClockCmd` | `adc.c:235`；`usart.c:165,195`；`lcd.c:591`；`touch.c:525`；`gt9147.c:115`；`ott2001a.c:83`；`ctiic.c:31`；`myiic.c:21`；`led.c:21`；`key.c:22` | GPIOA~G、DMA1、DMA2 |
| `RCC_APB1PeriphClockCmd` | `adc.c:237,248`；`usart.c:166` | TIM3、DAC（ENABLE 后 DISABLE）、USART2 |
| `RCC_APB2PeriphClockCmd` | `adc.c:236`；`usart.c:79` | ADC1\|ADC2；USART1（未执行） |
| `RCC_AHB3PeriphClockCmd` | `lcd.c:592` | FSMC |
| `GPIO_PinAFConfig` | `usart.c:82,83,168,169`；`lcd.c:630-651` | USART1/2 的 AF7；FSMC 的 AF12 |
| `GPIO_Init` | `adc.c:258`；`usart.c:91,176`；`lcd.c:600,607,614,621,628`；`touch.c:533,537,541,545`；`gt9147.c:122,126,137`；`ott2001a.c:91,95`；`ctiic.c:37,41`；`myiic.c:29`；`led.c:29`；`key.c:28,33` | 各端口/引脚/模式见 §3 |
| `GPIO_SetBits` | `led.c:31` | PF9\|PF10 置高（未调用） |
| `ADC_CommonInit` | `adc.c:264` | 双规则同步 / Div4 / DMA mode 2 / 5 cycles |
| `ADC_Init` | `adc.c:273,278` | ADC1 主、ADC2 从（边沿 None） |
| `ADC_RegularChannelConfig` | `adc.c:221,222` | ADC1 通道4 rank1；ADC2 通道5 rank1 |
| `ADC_DMACmd` | `adc.c:314,352,374` | ADC1 DMA 请求 |
| `ADC_Cmd` | `adc.c:319,320,350,351,379,380` | 使能/停止 ADC2、ADC1 |
| `ADC_MultiModeDMARequestAfterLastTransferCmd` | `adc.c:318,378` | 置 DDS，保证循环模式下持续发 DMA 请求 |
| `ADC_ClearFlag` / `ADC_GetFlagStatus` | `adc.c:312-313,360-362` / `adc.c:51,55` | OVR/EOC 标志清/查 |
| `DMA_DeInit` / `DMA_StructInit` | `adc.c:284-285`；`usart.c:197-198` | 复位并填默认值 |
| `DMA_Init` | `adc.c:301`；`usart.c:214` | Stream0 / Stream6 |
| `DMA_Cmd` | `adc.c:304`；`usart.c:245,252,261` | 使能/停止流 |
| `DMA_ITConfig` | `adc.c:303`；`usart.c:219` | HT\|TC；TC |
| `DMA_ClearFlag` | `adc.c:363-365`；`usart.c:216-218,246-248` | TC/HT/TE/DME/FE |
| `DMA_GetITStatus` / `DMA_ClearITPendingBit` | `adc.c:20,27,22,29`；`usart.c:258,260` | 中断状态 |
| `DMA_SetCurrDataCounter` | `usart.c:250` | 每次发送重设长度 |
| `TIM_TimeBaseStructInit` / `TIM_TimeBaseInit` | `adc.c:325` / `adc.c:330` | TIM3 PSC=0、ARR=419 |
| `TIM_SelectOutputTrigger` | `adc.c:331` | `TIM_TRGOSource_Update` |
| `TIM_Cmd` | `adc.c:332,353,385` | 启动/停止 TIM3 |
| `USART_Init` / `USART_Cmd` | `usart.c:100,184` / `usart.c:102,185` | USART1 / USART2 |
| `USART_ITConfig` | `usart.c:107,187` | RXNE |
| `USART_DMACmd` | `usart.c:227` | USART2 TX DMA 请求 |
| `USART_ReceiveData` | `usart.c:129,296` | ISR 读 DR |
| `USART_GetITStatus` | `usart.c:127,294` | RXNE 判定 |
| `NVIC_PriorityGroupConfig` | `main.c:11` | `NVIC_PriorityGroup_2`（4 抢占级） |
| `NVIC_Init` | `usart.c:114,192,225`；`adc.c:310` | 见 §4.1 优先级表 |
| `FSMC_NORSRAMInit` / `FSMC_NORSRAMCmd` | `lcd.c:688` / `lcd.c:690` | Bank1 NORSRAM4 |
| `FSMC_Bank1E->BWTR[6]` 直写 | `lcd.c:777-786` | 写时序重设（ADDSET 字段=4、DATAST 字段=4） |
| `SysTick_CLKSourceConfig` | `delay.c:120` | HCLK/8 |
| `SysTick->LOAD/VAL/CTRL` 直写 | `delay.c:127-129,182-190,201-209` | 轮询式延时 |
| `DAC->CR/DHR12R1/DHR12R2` 直写 | `adc.c:243-245` | 关闭 DAC 通道与输出缓冲（**未调用任何 `DAC_*` 库函数**，`stm32f4xx_dac.c` 不在工程） |
| 无 `SysTick_Config` / 无 `ADC_SoftwareStartConvCmd` / 无 `EXTI_*` 调用 | — | grep 全工程确认 |

## 7. “当前实际使用” vs “代码存在但未使用 / 旧方案”

| 项目 | 当前实际状态 | 证据（文件:行） | 旧方案 / 未使用说明 |
|---|---|---|---|
| ADC 通道 | **CH1=PA4/ADC1_IN4，CH2=PA5/ADC2_IN5**，双规则同时 | `USER\adc.c:221-222,250-258`；`USER\adc.h:6` | 旧：`.wavefix_backup_dual_pa5pa6_20261119\adc.c:219-220,239` 为 **PA5/ADC1_IN5 + PA6/ADC2_IN6**；另有备份单通道 PA4（`.wavefix_backup_20261119\adc.c:172`）与单通道 PA5（`.wavefix_backup_fsm_20261119\adc.c:185`） |
| PA6 | 未使用 | 仅出现在备份与文档 `..\dual-channel-pin.md:62,73,105` | 旧第二通道；**未加入当前工程**（`USER\Base.uvprojx` 文件列表无备份目录） |
| PA0 | 未使用；仅 `KEY` 的 WK_UP 宏 | `HARDWARE\KEY\key.h:21`；`key.c:31-33`（`KEY_Init` 无调用） | 文档备选（`..\dual-channel-pin.md:9,12` “CH1 改到 PA0/ADC1_IN0”），**代码中不存在 `ADC_Channel_0`**（全工程 grep 为空） |
| DAC | 先开时钟，写 `CR` 关通道+关输出缓冲，再**关时钟** | `USER\adc.c:237,243-248` | 不使用；`stm32f4xx_dac.c` 不在工程文件列表 |
| DCMI | 不使用 | 仅注释 `USER\adc.c:5` 提到 PA4 复用 `DCMI_HSYNC`；无 DCMI 时钟/初始化；`stm32f4xx_dcmi.c` 不在工程 | 插摄像头时 PA4（及旧方案 PA6）会被 DCMI 驱动，与 ADC 冲突 |
| USART1 | 代码完整但**未调用** | `SYSTEM\usart\usart.c:72-118,121-153`；`SYSTEM\usart\usart.h:7-8,18`；无调用点 | PA9/PA10 不会被配置为 AF7，USART1 时钟不会开；`EN_USART1_RX=1` 仍编译中断与 200 B 缓冲 |
| USART2 | 实际使用，460800，DMA TX + 中断 RX | `USER\main.c:14`；`SYSTEM\usart\usart.c:158-309` | 文档 `readme.txt:30` 仍写 115200 |
| LED | 代码存在但**未调用** | `HARDWARE\LED\led.c:17-33`；`led.h:20-21`（LED0=PF9，LED1=PF10） | PF9/PF10 实际未被初始化 |
| KEY | 代码存在但**未调用** | `HARDWARE\KEY\key.c:17-35,45-59`；`key.h:18-21`（KEY0=PE4，KEY1=PE3，KEY2=PE2，WK_UP=PA0） | PE2/3/4、PA0 实际未被初始化 |
| 硬件 I2C | 不使用 | 工程文件列表无 `stm32f4xx_i2c.c`；两套 IIC 均为 GPIO 模拟 | 软件 IIC：PB8/PB9（24C02）、PB0/PF11（触摸） |
| `HARDWARE\adc.h` | **0 字节空文件** | 文件大小 0；工程实际引用 `USER\adc.h`（`USER\Base.uvprojx:489-491`），包含路径无 `..\HARDWARE` 根目录（`uvprojx:339`） | 历史残留，不参与编译 |
| 备份目录 | 4 个 `.wavefix_backup_*` | 目录名带日期 20261119；未出现在 `Base.uvprojx` | 仅作历史对照，不编译 |

## 8. 风险 / 不一致

### 8.1 代码级风险

| # | 风险 | 证据 | 影响 | 建议 |
|---|---|---|---|---|
| R1（高） | **TIM3 ARR 为 16 位，低采样率时溢出截断**。`arr=84e6/fs-1`，fs 允许低至 1000 Hz（`cmd.c:913`、`cmd.c:742` 逐次减半），但 ARR 最大 65535，fs<约 1281.7 Hz 即越界 | `adc.c:342-344`；`adc.c:356`；`cmd.c:740-743,913` | 例：fs=1000 时 `arr=83999`→写入 16 位 ARR 实际为 18463，真实采样率约 **4.55 kHz**，而 `s_sample_rate` 仍记 1000（`adc.c:387`），导致时基、FFT 频率全部错误 | 把下限改为 ≥1282 Hz，或改用 32 位 TIM2/TIM5，或对 ARR 做 65535 上限保护并回读实际值 |
| R2（中） | **无帧恢复路径 `ADC1_Init()` 会把采样率重置为 200 kSa/s**，但 `cmd.c` 的 `g_tb_us`/`g_capture_tb_us` 不刷新 | `cmd.c:1082-1096` 调用；`adc.c:227` 重置 `s_sample_rate`；`adc.c:327` 重置 ARR=419 | 掉帧恢复后屏幕时基与实测采样率不一致，测量值/频率刻度偏差 | 恢复后调用 `ADC1_SetSampleRate(原 fs)`，或让 `ADC1_Init` 保留现有 `s_sample_rate` |
| R3（低） | **`CT_IIC_Init()` 未给 `GPIO_InitStructure.GPIO_Speed` 赋值**，该局部结构体其余字段被使用 | `ctiic.c:29-41`（只赋 Pin/Mode/OType/PuPd） | 输出速度取决于栈上残留值；功能通常正常，但属于未定义行为 | 补 `.GPIO_Speed = GPIO_Speed_50MHz` |
| R4（中） | **PA4 三重身份**：ADC1_IN4 / DAC_OUT1 / DCMI_HSYNC（J4 `STM_DAC`）。代码仅做了 DAC 中和，DCMI 无法由软件“关闭”引脚上的外部驱动 | `adc.c:5,243-248`；`..\dual-channel-pin.md:7,12,88`；`ui.c:1628`（“PA4 problem”） | 板上插摄像头时 PA4 被摄像头驱动，CH1 读数无效；DAC 若被外部/调试器使能同样污染读数 | 验收前拔掉摄像头模块；用万用表确认 PA4 静态电位；必要时按文档把 CH1 改到 PA0/PA6（需同时改代码） |
| R5（低） | **`printf` 与 USART2 DMA 发送共用同一 USART**：`fputc` 轮询 TXE 直接写 DR，不检查 DMA 忙 | `usart.c:52-57`；DMA 发送 `usart.c:238-254` | 若运行期调用 `printf`，会与 WAVE 帧 DMA 数据交错，破坏协议 | 运行期只用 `sprintf`+`uart2_send_dma`；`printf` 仅限初始化阶段 |
| R6（低） | **HSE 启动失败无兜底**：`SetSysClock()` 的 `else` 为空，`SystemCoreClock` 仍为 168 MHz | `system_stm32f4xx.c:661-664,371` | 晶振故障时实际以 HSI 16 MHz 运行，延时/波特率均偏约 10.5 倍 | 增加 HSE 失败处理或时钟自检 |
| R7（低） | **ADC1_Init 可在运行中被再次调用**（`cmd.c:1095`），与 DMA2 中断、UI 刷新并发；且重复写 DAC 寄存器 | `cmd.c:1095`；`adc.c:225-333` | 极端时序下可能丢一帧或短暂错帧；一般不致命 | 恢复路径加临界区或标志，避免与 UI/串口重入 |
| R8（低） | `lcd.c:780-787` 的 `if/else` 两个分支代码完全相同（仅注释区分 ST7789） | `lcd.c:780-787` | 冗余代码，易误读 | 合并分支 |

### 8.2 配置与注释 / 文档矛盾

| # | 文档/注释位置 | 文档说法 | 代码事实 | 影响 |
|---|---|---|---|---|
| D1 | `UI说明-新版界面.md:137` | “采样时间固定为 15 cycles……不再在运行中改 ADC 采样周期” | `adc.c:214-219` 三档（480/84/15），且 `ADC1_SetSampleRate()` 会在运行中重设采样时间（`adc.c:368-372`） | 验收/答辩口径与代码不符 |
| D2 | `USER\adc.c:9-10` 文件头注释 | “15 + 12 cycles at 21MHz = 1.29us” | 仅默认 200 kSa/s 成立；≤40 k 用 480 cycles（约 23.4 µs），≤100 k 用 84 cycles（约 4.57 µs） | 注释不完整，易被误解为恒为 15 cycles |
| D3 | `..\九问解答-代码核对版.md:20,85` | “200 kSa/s 时 84 cycles；>205 kSa/s 才 15 cycles” | 代码阈值是 **100 kSa/s**（`adc.c:216-219`），200 kSa/s 实际用 15 cycles | 对采样时间/余量的分析前提错误 |
| D4 | `..\九问解答-代码核对版.md:95` | “`adc.c:122-138`（采样时间选择）” | 当前采样时间选择在 `adc.c:210-223`，122-138 已是触发搜索代码 | 行号过期，审计引用失效 |
| D5 | `readme.txt:30-33` | main 做“168MHz、串口 115200、LED 初始化” | `main.c:11-18`：`uart2_init(460800)`，**无 LED 初始化** | 入门文档过时 |
| D6 | `readme.txt:62-65` | “PA9/PA10 USART1（TX/RX）”；“建议 ADC 用 PA1（IN1）、PA4（IN4）” | 实际串口为 USART2 PA2/PA3；ADC 为 PA4/PA5，PA1 未用 | 接线指导错误 |
| D7 | `readme.txt:47-49` | “LCD 用到了 PB3/PB4，需在 LCD_Init 前配置或关 JTAG” | `lcd.c` 从未配置 PB3/PB4（仅 PB15、PD、PE、PF12、PG12） | 错误指引，浪费排查时间 |
| D8 | `..\九问解答-代码核对版.md:24,255` | “Base 串口 115200” | `main.c:14` 为 460800 | 与 `UI说明` 460800 自相矛盾 |
| D9 | `..\dual-channel-pin.md:3` vs `:62,72-73,104-108` | 头部称当前代码 CH1=PA4/CH2=PA5；正文仍按 CH1=PA5/CH2=PA6 讲解与接线 | 代码为 PA4/PA5（`adc.c:221-222,253`） | 文档内部自相矛盾，按正文接线会接错 |
| D10 | `HARDWARE\LCD\lcd.c:591` 注释 | “使能 PD,PE,PF,PG 时钟” | 实际同时使能 **GPIOB**（PB15 背光） | 注释遗漏 |
| D11 | `HARDWARE\LCD\lcd.c:623` 注释 | “PF12,FSMC_A6” | 该段实际配置 **GPIOG Pin12 = FSMC_NE4** | 注释复制错误 |
| D12 | `HARDWARE\LCD\lcd.c:654` 注释 | “ADDSET 为 16 个 HCLK = 96ns” | 写入的是 `0xF`；FSMC 时序的实际 HCLK 计数口径应以 RM0090 为准，注释按 16 周期估算 | 时序估算仅供参考 |
| D13 | `HARDWARE\TOUCH\touch.c:225` 注释 | 校准参数“占用 13 个字节（40~52）” | 实际写 40~54（14 B）+ 54 标记 + 55 方向标记（`touch.c:229-231`） | 地址区间注释过时 |

## 9. 未确认项（静态审计无法判定）

1. **实际 LCD ID 与触摸分支**：`lcddev.id` 由 `LCD_Init()` 运行期读 ID 决定（`lcd.c:695-772`），`TP_Init()` 据此二选一（`touch.c:508`）。UI 按 320×240 布局（`ui.c:48-49`），推断面板为 9341/5310/7789 一类，**大概率走电阻屏分支（PB0/PB1/PB2/PC13/PF11 + PB8/PB9 + 24C02）**；但 4.3 寸 0x5510 电容屏分支同样编译在固件内。需上板读 `lcddev.id` 或看 `LCD_Init` 的 `printf(" LCD ID:%x")` 输出确认。
2. **PA4/PA5 实际接线**：代码只能证明引脚配置；J4 的第 5 脚/第 2、4 脚实际接线、信号源负载（50 Ω/High-Z）、共地情况需硬件核对（`..\dual-channel-pin.md:11,258-262`）。
3. **摄像头模块是否插着**：若插着，PA4（DCMI_HSYNC）会被驱动，CH1 无效（`..\dual-channel-pin.md:7`）。
4. **HSE 实际晶振与精度**：代码按 8 MHz（`stm32f4xx.h:123`）；板级晶振频率/ppm 未知，影响 460800 波特率误差。
5. **24C02 是否存在及校准数据**：地址 40~55 的内容与方向标记需读 EEPROM 确认；`AT24CXX_Check()` 未被调用，IIC 无应答时不会显式报错。
6. **低采样率下的真实 ARR**：R1 为静态推导（TIM3 为 16 位），建议在 fs=1000 时打印 `TIM3->ARR` 或实测频率确认。
7. **备份目录是否曾被编译/烧录**：目录不在工程内，但无法从源码判断历史固件版本。
8. **DAC 是否曾被调试器/旧固件使能**：代码是防御式清零（`adc.c:239-248`），无法从源码确认上电前状态。

## 10. 审计方法与复核说明

- 只读操作：仅使用文件读取与文本检索；未写任何源码/工程文件。
- 行号来源：对当前磁盘文件按行重新读取（GBK 用代码页 936、UTF-8 用无 BOM 严格解码），并与 `grep` 行号交叉核对；`USER\*.c` 等为纯 ASCII，两种解码行号一致。
- 检索覆盖的库函数/关键字：`RCC_AHB1PeriphClockCmd`、`RCC_APB1PeriphClockCmd`、`RCC_APB2PeriphClockCmd`、`RCC_AHB3PeriphClockCmd`、`GPIO_Init`、`GPIO_PinAFConfig`、`ADC_CommonInit`、`ADC_Init`、`ADC_RegularChannelConfig`、`ADC_DMACmd`、`ADC_Cmd`、`ADC_SoftwareStartConvCmd`、`DMA_Init/Cmd/ITConfig`、`TIM_TimeBaseInit/SelectOutputTrigger/Cmd`、`USART_Init/Cmd/ITConfig/DMACmd`、`FSMC_NORSRAMInit/Cmd`、`SysTick_*`、`NVIC_*`、`ADC_Channel_0/6`、`PA0/PA6`、`DCMI`、`DAC`、`LED_Init`、`KEY_Init`、`uart_init`、`TP_Init`、`IIC_Init`、`AT24CXX_Init`。
- 未审计范围：`FWLIB/` 标准外设库实现、`CORE/` CMSIS、`STM32F4xx_DSP_Support/`（仅确认工程引用关系）。
- 覆盖外设/子系统计数：**RCC 时钟树 + GPIOA~G（7 端口）+ ADC1 + ADC2 + DMA2 Stream0 + TIM3 + USART2 + DMA1 Stream6 + FSMC + LCD + 触摸控制器（GT9147/OTT2001A/电阻屏）+ 24C02 软件 IIC + SysTick**，共 **19 项实际启用的外设/子系统**；另有 **USART1、DAC、DCMI、LED、KEY、硬件 I2C** 6 项“存在但未使用/已关闭”。

---

*报告结束。所有行号均基于本次审计时磁盘上的当前文件；若后续修改源码，请重新核对。*
