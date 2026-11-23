# STM32F407 芯片内部机制解析：双通道 ADC 示波器
## TIM3 → ADC1/ADC2 双重规则同时 → DMA2 → SRAM → CPU → FSMC/USART

> 本报告只读源码、只读/下载芯片手册，未修改任何 `.c/.h/工程` 文件；本文件是本次任务的唯一写入产物。
> 与已有报告的分工：时钟树/GPIO/初始化“清单式”溯源见 `stm32-peripheral-traceability.md`；本报告聚焦**芯片内部怎么工作、关键指标怎么算、手册依据在哪、边界与风险在哪**，面向答辩讲解。

---

## 0. 元信息与证据约定

| 项 | 内容 |
|---|---|
| 分析对象 | `2026创新实验室招新选拔软件第三题\2026创新实验室招新选拔软件第三题\ADC\ADC`（Keil 工程 `USER\Base.uvprojx`） |
| 芯片/器件 | **STM32F407ZGTx**（`USER\Base.uvprojx:17`），Cortex-M4F，LQFP144 封装（ZGTx） |
| 工程宏 | `STM32F40_41xxx, USE_STDPERIPH_DRIVER, __FPU_PRESENT=1, ARM_MATH_CM4`（`USER\Base.uvprojx:337`） |
| 标准外设库 | STM32F4xx StdPeriph V1.4.0（`USER\stm32f4xx_conf.h:5`） |
| 只读核对范围 | `USER\main.c`、`USER\adc.c/.h`、`USER\cmd.c/.h`、`USER\measure.c/.h`、`USER\ui.c/.h`、`SYSTEM\usart\usart.c/.h`、`SYSTEM\delay\delay.c/.h`、`USER\system_stm32f4xx.c`、`USER\stm32f4xx.h`、`USER\stm32f4xx_it.c`、`HARDWARE\LCD\lcd.c/.h`、`HARDWARE\TOUCH\*.c`、`HARDWARE\IIC\myiic.c`、`HARDWARE\24CXX\24cxx.c/.h`、`CORE\startup_stm32f40_41xxx.s`、`OBJ\Base.map` |
| 行号基准 | 除特别注明外，`文件:行` 均相对工程根目录 `...\ADC\ADC` |
| 编码说明 | 旧 ALIENTEK 驱动（lcd.c/touch.c/myiic.c/24cxx.c/usart.c/delay.c 等）为 **GBK**，其余 `USER\*.c` 为 UTF-8/ASCII；本报告行号以逐行解码后核对 |
| 报告写入 | `2026创新实验室招新选拔软件第三题\stm32-internal-mechanism.md`（仅此文件） |

**证据约定**

| 标记 | 含义 |
|---|---|
| 【代码】 | 已逐行只读核对，给出 `file:line` |
| 【手册】 | 已在 PDF 正文中核对到章节/页码（非凭记忆） |
| 【推导】 | 由手册公式 + 代码参数计算得出（给出算式） |
| 【估算】 | 量级估算，未上机测量（给出假设） |
| 【待核对】 | 未能从手册/代码唯一确认，答辩时应说明或实测 |

**手册文件（本地 + 官方 URL）**

| 代号 | 文档 | 本地文件 | 官方 URL / 版本 |
|---|---|---|---|
| 【RM】 | RM0090 参考手册（STM32F405/407/415/417） | `.tmp_rm0090.pdf`（1741 页，与 ST 官网下载文件 **SHA256 完全一致**：`F3FF2322…B27CCA`） | https://www.st.com/resource/en/reference_manual/dm00031020.pdf ，**Rev 22, May 2026** |
| 【CN】 | RM0090 中文参考手册 第 4 版 | `冠军407开发板配套资料\9-开发攻略\STM32F4xx中文参考手册.pdf`（1284 页） | 文档 ID 018909 第 4 版，2013-02（**章节号/页码与英文 Rev 22 不同**） |
| 【DS】 | DS8626 数据手册（STM32F405xx/407xx） | —（本报告数值取自官网 Rev 12） | https://www.st.com/resource/en/datasheet/stm32f407vg.pdf ，**Rev 12, March 2026**（206 页） |
| 【DS2】 | DS8626 早期版（开发板配套） | `冠军407开发板配套资料\6--芯片资料\开发板芯片数据手册\开发板芯片数据手册\STM32F407ZGT6.pdf`（167 页） | Doc ID 022152 **Rev 2**, January 2012 |
| 【PM】 | PM0214 Cortex-M4 编程手册 | —（英文） | https://www.st.com/resource/en/programming_manual/dm00046982.pdf ，**Rev 10, March 2020**（262 页） |
| 【PMC】 | PM0214 中文版（STM32F3/F4） | `冠军407开发板配套资料\9-开发攻略\STM32F3与F4系列Cortex M4内核编程手册.pdf`（245 页） | DocID022708 Rev 4, May 2014 |

> ⚠️ 引用页码的两个坑（本报告已规避）：
> 1. **英文 Rev 22 与中文第 4 版章节号不同**：英文 ADC 是**第 13 章**，中文是**第 11 章**；英文 FSMC 第 36 章，中文第 32 章。
> 2. **本地 DS8626 是 Rev 2（2012）**，与官网 Rev 12 页码/引脚命名不同（Rev 2 叫 `DAC1_OUT/DAC2_OUT`，Rev 12 叫 `DAC_OUT1/DAC_OUT2`）。本报告把两版分开标注。

---

## 1. 一句话结论

这套固件把示波器的“**等间隔采样**”彻底交给硬件：**TIM3 的更新事件（TRGO）以 84 MHz/420 = 200 kHz 的硬节拍同时触发 ADC1（PA4/IN4）与 ADC2（PA5/IN5）做双重规则同时转换，两个 12 位结果由硬件打包进 `ADC->CDR`，再由 DMA2 Stream0 以 32 位循环搬运到 SRAM 的双半区（2×2048 对）**；CPU 只在半传输/传输完成中断里翻页，然后在主循环里做中值滤波、软件触发对齐、测量（过零+FFT/Goertzel）、FSMC 刷屏和 USART2+DMA 上传。**采样时刻和采样率由芯片硬件保证（抖动仅取决于 TIM3 与 ADC 时钟），CPU 负载只影响“显示/测量刷新率”，不影响采样本身**——这是本设计在芯片层面最重要的正确性来源。

---

## 2. 应用场景与设计目标

### 2.1 题目要求 → 芯片能力映射

| 需求 | 本代码的做法 | 依赖的芯片资源 |
|---|---|---|
| 双通道、**同一时刻**采样 | ADC1+ADC2 **双重规则同时模式**（Dual Regular Simultaneous） | 双 ADC 公共控制 `ADC_CCR.MULTI`、`ADC->CDR` |
| 采样率可调（200 kSa/s、500 kSa/s…） | TIM3 定时触发，改 `ARR` 改采样率 | TIM3（APB1 定时器，84 MHz）→ TRGO |
| 不丢点、不占 CPU | DMA2 Stream0 循环模式 + HT/TC 中断 | DMA2、总线矩阵、SRAM |
| 触发/单次/自动 | 软件在帧内找触发点并旋转帧 | CPU + SRAM 帧缓存 |
| 实时显示 | FSMC 并口 LCD，CPU 写显存 | FSMC Bank1 NE4、AHB3 |
| 上位机通信 | USART2 460800 + DMA1 Stream6 发送、RX 环形缓冲 | USART2（APB1）、DMA1 |
| 频率/幅值/相位/THD | 过零插值 + 2048 点 FFT + Goertzel + 相关相位 | Cortex-M4F FPU + CMSIS-DSP |

### 2.2 为什么必须“定时器触发 + 双 ADC + DMA”，而不是软件触发/单 ADC 扫描

- **单 ADC 扫描两条通道 = 分时采样**：同一 ADC 先采 CH1 再采 CH2，两路相差一个转换时间（本配置下 1.29 µs），相位测量会引入与频率相关的系统误差。要“同一时刻”，必须两个 ADC 同时采样保持。
- **软件触发（SWSTART）抖动大**：软件写寄存器受中断、LCD 总线等待影响，采样间隔不可恒定；用 TIM3 硬件 TRGO，触发沿由定时器计数器和 ADC 时钟决定，**抖动在 ns~几十 ns 量级**（取决于时钟同步），远优于软件。
- **CPU 搬运不可行**：200 kSa/s × 2 通道 × 2 B = 800 kB/s，若每次 EOC 进中断搬运，CPU 每秒 20 万次中断，必然拖垮显示与测量；DMA 把“每次转换”变成“每 2048 对一次中断”（约 10 ms 一次），CPU 开销下降 3 个数量级。
- **循环模式 + 双半区**：DMA 循环让采集永不停止；HT/TC 各给出 2048 对样本（一个“帧”），CPU 处理一帧时 DMA 正在写另一帧，形成**无锁双缓冲**（严格说是“半缓冲交替”，见 §5.8）。

### 2.3 代码分层（答辩时先讲这张图）

```
main.c
 ├─ NVIC_PriorityGroupConfig(Group2)          // 中断优先级分组
 ├─ delay_init(168)                           // SysTick = HCLK/8 = 21MHz
 ├─ uart2_init(460800)                        // USART2 + DMA1 Stream6 + RX 中断
 ├─ ADC1_Init()                               // 采集核：GPIO/ADC/TIM3/DMA2/NVIC
 ├─ CMD_Init()                                // 状态机：RUN/STOP/SINGLE/NORM/AUTO、时基、档位
 └─ UI_Init()                                 // LCD(FSMC)+触摸(24C02 校准)
while(1) { CMD_Task(); UI_Task(); }           // 裸机前后台，无 RTOS（sys.h:23 SYSTEM_SUPPORT_OS=0）
```

- **采集核**（`USER\adc.c`）：只做“硬件配置 + 帧拷贝 + 中值滤波 + 触发点搜索”。
- **命令/状态层**（`USER\cmd.c`）：命令解析、自动量程、触发模式、帧旋转、串口流。
- **算法层**（`USER\measure.c`）：Vpp/DC/RMS/频率/相位/THD/波形类型。
- **显示层**（`USER\ui.c`）：局部刷新优化、触摸交互。
- **通道层**（`SYSTEM\usart\usart.c`）：USART2 收发、DMA 发送、环形接收。

---

## 3. 术语表（用代码里的真实配置解释）

| 术语 | 芯片里的准确含义 | 本代码中的落点 |
|---|---|---|
| **TRGO**（Trigger Output） | 定时器主模式输出，可把“更新事件/比较事件/使能”等送到片上其他外设（ADC、DAC、其他定时器）作为硬件触发源；由 `TIMx_CR2.MMS[2:0]` 选择 | `TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update)`（`adc.c:331`）→ MMS=010，TRGO=更新事件【RM §18.4.3 p.626；SPL 实际写 `TIMx->CR2`（`FWLIB\src\stm32f4xx_tim.c:2941-2944`）】 |
| **规则通道**（Regular） | ADC 的常规转换组，结果进 `ADC_DR`（多 ADC 时进 `ADC_CDR`），由 `ADC_CR2` 的 `EXTSEL/EXTEN` 选外部触发 | ADC1 规则组 1 个通道：`ADC_Channel_4`，采样时间可变（`adc.c:221`）；ADC2：`ADC_Channel_5`（`adc.c:222`） |
| **双重规则同时模式** | 两个 ADC 的规则组由**同一个触发**同时启动采样，结果分别进各自 `ADC_DR`，并可按 DMA 模式打包 | `ADC_DualMode_RegSimult`（`adc.c:260`，库值 0x06 = MULTI=00110）；ADC1 有触发、ADC2 **触发沿 = None**（`adc.c:277`）【RM §13.9.2 p.407】 |
| **ADCCLK** | ADC 模拟部分时钟，由 PCLK2 经 `ADC_CCR.ADCPRE` 2/4/6/8 分频，所有 ADC 共用；有上限（F407 为 36 MHz @VDDA≥2.4 V） | `ADC_Prescaler_Div4`（`adc.c:261`）→ 84/4 = **21 MHz**【RM §13.3.2 p.392；§13.13.16 p.430；DS Table 67 p.135】 |
| **APB1 定时器时钟** | APB1 预分频 ≠1 时，挂 APB1 的定时器时钟 = 2×PCLK1（F4 的“定时器倍频”规则） | PCLK1=42 MHz → TIM3CLK=**84 MHz**（代码在 `adc.c:342` 直接写死 84 000 000）【RM §7.2 p.218】 |
| **采样时间 / 转换时间** | 采样时间 = `ADC_SMPR` 选的 ADCCLK 周期数；总转换时间 = 采样 + 12 个 ADCCLK（12 位逐次逼近） | 15/84/480 cycles 三档（`adc.c:214-219`）；`Tconv=(N+12)/21 MHz`【RM §13.5 p.399；DS Table 67 p.136】 |
| **CDR** | 多 ADC 公共规则数据寄存器：**ADC2 在 [31:16]，ADC1 在 [15:0]**；地址 = ADC1 基址 + 0x300 + 0x08 | DMA 外设地址 `(u32)&ADC->CDR`（`adc.c:287`）；解包 `w & 0xFFFF` = CH1、`w >> 16` = CH2（`adc.c:69-70`）【RM §13.13.17 p.432；DMA 模式 2 描述 §13.9 p.405】 |
| **DMA 循环模式（Circular）** | 传输计数 `NDTR` 减到 0 后自动重装初值，请求继续服务，适合连续数据流 | `DMA_Mode_Circular`（`adc.c:295`），`BufferSize=4096` 字（`adc.c:290`）【RM §10.3.8 p.315】 |
| **HT / TC** | 半传输（NDTR 到一半）与传输完成（NDTR 到 0）中断标志；循环模式下二者周期性出现 | `DMA_IT_HT`/`DMA_IT_TC`（`adc.c:303`），ISR 里置 `s_frame_offset=0/2048`（`adc.c:18-33`）【RM §10 p.304 起】 |
| **DDS** | 多 ADC 模式下“末次传输后是否继续发 DMA 请求”：0 = 停发，1 = 继续发 | `ADC_MultiModeDMARequestAfterLastTransferCmd(ENABLE)`（`adc.c:318`、`adc.c:378`）→ `ADC_CCR.DDS=1`【RM §13.13.16 p.430；§13.8.1 p.402】 |
| **OVR（Overrun）** | 数据未被及时读走而丢失时硬件置位；`DMA=1` 或 `EOCS=1` 时才使能检测；DMA 模式下 OVR=1 后 DMA 请求被阻塞 | 主循环每次 `ADC1_CopyFrame()` 轮询并清 ADC1/ADC2 的 OVR（`adc.c:51-58`）【RM §13.13.1 p.417；§13.8.1 p.402】 |
| **FSMC bank** | 外部静态存储器控制器，Bank1 分 4 个 NOR/PSRAM 子区（NE1~NE4），由 `HADDR[27:26]` 选择；16 位总线时 `FSMC_A[x]=HADDR[x+1]` | `FSMC_Bank1_NORSRAM4`（`lcd.c:672`）→ 基址 **0x6C000000**；`LCD_BASE=0x6C000000\|0x7E`，A6 作命令/数据线（`lcd.h:62`）【RM §36.4/§36.4.1 p.1535-1536，Table 216/217】 |
| **NVIC** | Cortex-M4 嵌套向量中断控制器；F407 实现 4 位优先级 = 16 级；分组把优先级分为抢占/子优先级 | `NVIC_PriorityGroup_2`（`main.c:11`）→ 2 位抢占（4 级）+2 位子优先级；DMA2S0 抢占 1（`adc.c:307`）、DMA1S6 抢占 2（`usart.c:222`）、USART2 抢占 3（`usart.c:189`）【RM §12.1 p.373；PM §2.3.6 p.41、§4.3.7 p.215】 |
| **总线矩阵** | 32 位多层 AHB 矩阵，8 个主设备（CPU I/D、DMA1/2 存储与外围口、以太网、USB HS 等）连到 SRAM/FSMC/APB 桥 | ADC 在 APB2、TIM3/USART2 在 APB1、DMA2 在 AHB1、FSMC 在 AHB3、帧缓存在 SRAM1【RM §2.1 p.59；§2.1.4 p.63；§2.1.11 p.64】 |
| **ART / 等待周期** | 168 MHz 下 Flash 需 5 个等待周期；ART 加速器（预取+I/D cache）降低取指惩罚 | `FLASH->ACR = …LATENCY_5WS`（`system_stm32f4xx.c:644`）【RM §3.5.1 p.80-81，Table 11 p.81】 |

---

## 4. 芯片内部数据流图

### 4.1 Mermaid 数据流（答辩可直接投屏）

```mermaid
flowchart LR
    HSE[HSE 8MHz<br/>stm32f4xx.h:123] --> PLL[PLL<br/>M8 N336 P2<br/>VCO 336MHz]
    PLL --> SYSCLK[SYSCLK/HCLK 168MHz]
    SYSCLK --> APB2[APB2 84MHz]
    SYSCLK --> APB1[APB1 42MHz]
    APB1 -->|×2 定时器倍频| TIM3CLK[TIM3CLK 84MHz]
    APB2 -->|ADCPRE=/4| ADCCLK[ADCCLK 21MHz]
    TIM3CLK --> TIM3[TIM3<br/>PSC=0 ARR=419<br/>更新事件 200kHz]
    TIM3 -->|TRGO 上升沿| ADC1[ADC1 主<br/>IN4/PA4]
    TIM3 -->|同一触发| ADC2[ADC2 从<br/>IN5/PA5]
    ADCCLK --> ADC1
    ADCCLK --> ADC2
    ADC1 --> CDR[ADC_CDR<br/>[15:0]=ADC1<br/>[31:16]=ADC2]
    ADC2 --> CDR
    CDR -->|DMA2 Stream0<br/>Ch0, 32bit, Circular| SRAM[SRAM1 帧缓冲<br/>4096 words=16KB]
    SRAM -->|HT/TC 中断 2048 对/次| CPU[CPU 主循环<br/>CopyFrame/滤波/触发/测量]
    CPU -->|FSMC NE4 16bit 模式A| LCD[LCD 320x240<br/>显存]
    CPU -->|USART2 TX<br/>DMA1 Stream6| UART[USB-TTL 460800<br/>VOFA+/上位机]
    TOUCH[触摸/24C02<br/>I2C 轮询] --> CPU
```

### 4.2 时钟域与“谁在什么时候动”

| 域 | 频率 | 谁在这里跑 | 关键结论 |
|---|---|---|---|
| HSE | 8 MHz | 外部晶振 | PLL 参考 |
| PLL VCO | 336 MHz | PLL | 8/8×336 |
| SYSCLK/HCLK | 168 MHz | CPU、DMA、SRAM、FSMC、GPIO | 1 周期 5.95 ns |
| APB2 | 84 MHz | ADC1/2 数字接口、USART1（未用）、SPI1（未用） | ADC 寄存器访问时钟 |
| APB1 | 42 MHz | TIM3、USART2、I2C（未用）、DAC（已关） | TIM3 计数时钟 84 MHz（倍频） |
| ADCCLK | 21 MHz | ADC 模拟采样/转换 | 1 周期 47.6 ns |
| SysTick | 21 MHz | `delay_us/ms` 轮询 | 不产生中断（`stm32f4xx_it.c:142-145` 为空） |
| FSMC 外部总线 | HCLK 168 MHz | LCD 并口写 | 写一个像素 ≈17 HCLK |

### 4.3 一次“触发→像素/串口”的完整时序链（200 kSa/s）

```
t0        TIM3 CNT 溢出（更新事件） → TRGO 上升沿
t0+~2cyc  ADC1/ADC2 同时开始采样（tlatr≈2 ADCCLK，约 0.095µs）【DS Table 67 p.136】
t0+15cyc  采样结束（0.714µs）
t0+27cyc  两个 ADC 转换完成（1.286µs），结果进 ADC_DR
          ↓ 硬件打包进 ADC_CDR（一次 32 位读 = CH1|CH2）
          ↓ ADC1 发 DMA 请求（DMA 模式 2：成对传输）
t0+~1.5µs DMA2 Stream0 把一个 32 位字写入 g_adc_dma_buf[n]
          每 2048 个字 → HT 中断；每 4096 个字 → TC 中断（循环重装）
t0+10.24ms  一帧 2048 对采满 → ISR 置 s_frame_ready/offset
          主循环 ADC1_CopyFrame()：清 OVR、拷 2048 字、解包、3 点中值
          CMD_Task：在帧内找触发点 → 旋转两路帧 → 保存显示帧
          UI_Task：测量（过零/FFT）、FSMC 画波形、参数卡
          USART2+DMA：可选的 MEAS?/WAVE?/FireWater 文本流
```

---

## 5. 时钟树与总线（含全部计算）

### 5.1 时钟树推导表

| 节点 | 频率 | 计算 | 代码/手册依据 |
|---|---|---|---|
| HSE | 8 MHz | 外部晶振 | `USER\stm32f4xx.h:123`【代码】 |
| PLL_M | 8 | VCO 输入 = 8/8 = 1 MHz | `system_stm32f4xx.c:316`【代码】 |
| PLL_N | 336 | VCO = 1×336 = 336 MHz | `system_stm32f4xx.c:329`【代码】 |
| PLL_P | 2 | SYSCLK = 336/2 = **168 MHz** | `system_stm32f4xx.c:331`、`:617-618`【代码】 |
| PLL_Q | 7 | 48 MHz（USB/SDIO/RNG，本工程未用） | `system_stm32f4xx.c:326`【代码】 |
| HCLK | 168 MHz | HPRE=DIV1 | `system_stm32f4xx.c:598`【代码】；【RM §7.2 p.218】 |
| PCLK2 | 84 MHz | PPRE2=DIV2 | `system_stm32f4xx.c:602`【代码】 |
| PCLK1 | 42 MHz | PPRE1=DIV4 | `system_stm32f4xx.c:605`【代码】 |
| TIM3CLK | **84 MHz** | PPRE1≠1 → 定时器时钟 ×2 | 【RM §7.2 p.218】“Otherwise, they are set to twice (×2)…” |
| ADCCLK | **21 MHz** | PCLK2/4 | `adc.c:261`【代码】；【RM §13.3.2 p.392、§13.13.16 p.430】 |
| SysTick | 21 MHz | HCLK/8 | `delay.c:120-121`【代码】；【RM §7.2 p.218】 |
| FSMC | 168 MHz | AHB3 无分频，=HCLK | `lcd.c:592`【代码】；【RM §36 p.1532】 |
| Flash | 5 WS + ART | 168 MHz 必须 5 等待周期 | `system_stm32f4xx.c:644`【代码】；【RM §3.5.1 p.80-81】 |

**PLL 公式（可在答辩板书）**

```
fVCO = (HSE / PLL_M) × PLL_N = (8 MHz / 8) × 336 = 336 MHz
SYSCLK = fVCO / PLL_P = 336 / 2 = 168 MHz
HCLK = SYSCLK / AHB_div = 168 / 1 = 168 MHz
PCLK1 = HCLK / 4 = 42 MHz     PCLK2 = HCLK / 2 = 84 MHz
TIMxCLK(APB1) = 2 × PCLK1 = 84 MHz（APB 预分频≠1）
ADCCLK = PCLK2 / 4 = 21 MHz
```

### 5.2 三个“为什么”

1. **为什么 ADCCLK 是 21 MHz 而不是 84 MHz？**
   ADC 模拟部分不能跑 84 MHz；F407 规定 ADCCLK 最大 36 MHz（VDDA 2.4–3.6 V）【DS Table 67 p.135】。取 /4 得 21 MHz，既低于上限、又让 500 kSa/s（需要 1/2 µs 一次转换）有足够余量（见 §7）。

2. **为什么 TIM3 是 84 MHz 而不是 42 MHz？**
   这是 F4 的“定时器倍频”规则：只要 APB 预分频不是 1，定时器时钟自动 ×2【RM §7.2 p.218】。所以 `84 000 000` 这个常数（`adc.c:342`）是对的，但它**依赖 PPRE1=DIV4 这一前提**——若有人把 PPRE1 改成 DIV1，TIM3CLK 变 42 MHz，所有采样率减半、时基全错（风险 R5）。

3. **为什么 ADC 时钟要单独分频，而定时器不用？**
   TIM3 属于“数字定时器”，时钟可以到 84 MHz；ADC 的模拟前端（采样保持、比较器）有独立上限。二者由不同分频器管理：TIM3 用 APB1 定时器倍频，ADC 用 `ADC_CCR.ADCPRE`【RM §13.13.16 p.430】。

### 5.3 总线矩阵：数据从哪条路走

| 事务 | 主设备 | 从设备/总线 | 带宽/竞争 |
|---|---|---|---|
| ADC 结果 → SRAM（DMA） | DMA2 存储口（AHB1） | SRAM1 | 800 kB/s@200k（0.12% 理论带宽）【推导】 |
| CPU 读帧/算法 | Cortex-M4 I/D 总线 | SRAM1、Flash（ART） | 与 DMA 共享 SRAM 端口，但量级差距大 |
| LCD 写显存 | Cortex-M4 D 总线 → AHB3 | FSMC → 外部并口 | **CPU 同步等待**，是真正的“时间竞争” |
| 串口发送 | DMA1 存储口/外设口 | USART2（APB1） | 46 kB/s |
| 触摸/EEPROM | CPU 位带 GPIO 翻转 | GPIOB/F | 软件延时主导 |

**关键结论**：DMA 与 CPU 对 SRAM 的竞争在数量级上可忽略（DMA 只占 0.12% 理论带宽）；真正拖慢系统的是 **FSMC 写显存时 CPU 被总线等待阻塞**（每次 16 位写约 17 个 HCLK，见 §7.4），以及 **浮点/FFT 算法占用的 CPU 周期**。这解释了为什么 UI 里有大量“局部刷新/签名比较”优化（`ui.c:1403-1435`、`ui.c:2214-2248`）。

---

## 6. ADC / TIM / DMA 内部机制

### 6.1 TIM3 如何产生 200 kHz 的 TRGO

**配置**（`adc.c:325-332`）：

```
PSC = 0, ARR = 419, 向上计数, CKD=DIV1
MMS = 010（Update）→ TRGO 在每次更新事件（计数器溢出）输出一个脉冲
```

**公式**：

```
f_TRGO = f_TIM3CLK / ((PSC+1) × (ARR+1))
       = 84 000 000 / (1 × 420)
       = 200 000 Hz
```

- 改采样率：`ARR = round(84 000 000 / fs) − 1`（`adc.c:342`）。
  - 200 kHz → ARR=419（实测公式：84 000 000/200 000=420 → 419）【代码 `adc.c:327` 与公式一致】
  - 500 kHz → ARR=167（84 000 000/500 000=168 → 167）
  - 40 kHz → ARR=2099；100 kHz → ARR=839
- **更新事件来源**：向上计数时 CNT 从 ARR 回到 0 产生 UEV；`TIM_SelectOutputTrigger(..., TIM_TRGOSource_Update)` 把 UEV 映射到 TRGO【RM §18.4.3 p.626；SPL 写 CR2】
- **为什么不用 PWM/比较通道**：更新事件每个周期都有、无额外引脚、与 ADC 触发极性天然匹配（上升沿）【RM §13.13.3 p.420，EXTSEL=1000 即 T3_TRGO】。

**边界**：TIM3 是 **16 位**定时器（TIM2/TIM5 才是 32 位）【RM §18.1 p.586】。ARR 最大 65535 → 最低触发频率 = 84 MHz/65536 = **1281.74 Hz**。代码允许 `fs` 低到 1000 Hz（`adc.c:339`），此时 `arr=83999` 超出 16 位，写入 `TIM3->ARR` 会截断（推导：0x1481F→0x481F=18463 → 实际约 4549 Hz），时基会错 4.5 倍（风险 R1，必须修）。

### 6.2 触发如何进入 ADC

**ADC1（主）**：

```c
ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising;   // adc.c:269
ADC_InitStructure.ADC_ExternalTrigConv     = ADC_ExternalTrigConv_T3_TRGO;      // adc.c:270
```

- 库值 `ADC_ExternalTrigConv_T3_TRGO = 0x08000000` → `ADC_CR2.EXTSEL[3:0] = 1000`，即 **Timer 3 TRGO event**【RM §13.13.3 p.420；“1000: Timer 3 TRGO event”】。
- `EXTEN=01`（上升沿）→ TRGO 的上升沿启动规则组转换。

**ADC2（从）**：

```c
ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;     // adc.c:277
```

- 从机**禁止自己的外部触发**，只等 ADC1 的“同时触发”信号。这正是手册要求：
  > “In multi ADC mode, when configuring conversion trigger by an external event, the application must set trigger by the master only and disable trigger by slaves to prevent spurious triggers…”【RM §13.9 p.403】
- 规则同时模式的触发路径：
  > “The external trigger source comes from the regular group multiplexer of ADC1 … A simultaneous trigger is provided to ADC2 and ADC3.”【RM §13.9.2 p.407】

### 6.3 采样与转换时间

**代码分档**（`adc.c:210-223`）：

| 采样率范围 | 采样时间 | 采样周期数 | 总转换周期 | 总转换时间 @21 MHz | 单 ADC 理论最高速率 |
|---|---|---|---|---|---|
| ≤ 40 kHz | `ADC_SampleTime_480Cycles` | 480 | 480+12=492 | **23.43 µs** | 42.68 kSa/s |
| 40–100 kHz | `ADC_SampleTime_84Cycles` | 84 | 84+12=96 | **4.571 µs** | 218.75 kSa/s |
| > 100 kHz | `ADC_SampleTime_15Cycles` | 15 | 15+12=27 | **1.286 µs** | 777.78 kSa/s |

**手册公式**：

```
Tconv = 采样时间 + 12 个 ADCCLK        （12 位分辨率，逐次逼近）
```
【RM §13.5 p.399：“Tconv = Sampling time + 12 cycles”；示例：ADCCLK=30 MHz、3 cycles → 0.5 µs】
【DS Table 67 p.136：tCONV = 9~492 个 1/fADC（采样 + n 位逐次逼近），12 位 @30 MHz 为 0.50–16.40 µs】

**推导**：21 MHz 下 1 个 ADCCLK = 47.6 ns；15+12=27 → 1.286 µs。200 kSa/s 的触发周期是 5 µs，余量 3.89×；500 kSa/s 是 2 µs，余量 1.56×。

**双 ADC 并行性**：两个 ADC 各自有独立采样保持/转换器，**同一触发下并行转换**，因此“每对通道”的吞吐率由触发率决定，而不是两倍串行时间。ADC1/ADC2 的采样时间必须一致（代码对两者用同一个 `sample_time`，`adc.c:221-222`），否则同一时刻启动、不同时刻结束，虽然仍算同时采样，但会改变 EOC/DMA 请求的合并时刻（推导）。

**关于 `ADC_TwoSamplingDelay_5Cycles`**（`adc.c:263`）：该字段是 `ADC_CCR.DELAY`，手册在**交错模式**里定义“两次转换之间的最小延迟”【RM §13.9.3 p.409】；规则同时模式由同一触发同步启动，DELAY 不改变“同时采样”的性质。保留它无害，但不要把它当成“双 ADC 对齐参数”讲解（待核对项 N2）。

### 6.4 双重规则同时模式：芯片内部发生了什么

1. TIM3 TRGO 上升沿送到 ADC1 的规则组触发输入（EXTSEL=1000）。
2. ADC1 通过内部“同时触发”信号启动 ADC2（ADC2 的 EXTEN=00，不响应外部触发）。
3. ADC1 采样 PA4（IN4），ADC2 采样 PA5（IN5），**采样保持同一时刻开始**。
4. 两个 12 位 SAR 并行转换；转换完成后各自结果进 `ADC_DR`。
5. 多 ADC 公共逻辑把两个结果打包进 `ADC_CDR`：**[15:0]=ADC1，[31:16]=ADC2**【RM §13.13.17 p.432】。
6. 由于 `ADC_CCR.DMA=10`（DMA 模式 2）【RM §13.13.16 p.430】，每凑齐一对数据产生一次 DMA 请求，DMA 读 `ADC_CDR` 得到一个 32 位字：
   > “In Dual ADC mode, both ADC2 and ADC1 data are transferred on the first request (ADC2 data take the upper half-word and ADC1 data take the lower half-word)”【RM §13.9 p.405】

**代码映射**：`ADC_DMAAccessMode_2`（`adc.c:262`）、`DMA_PeripheralDataSize_Word`/`DMA_MemoryDataSize_Word`（`adc.c:293-294`）、解包 `低 16 位=CH1 / 高 16 位=CH2`（`adc.c:69-70`）——与手册逐字对应。

**通道约束**：规则同时模式**不允许两个 ADC 采同一个通道**（“Do not convert the same channel on the two/three ADCs”）【RM §13.9.2 p.407】。当前 CH1=ADC1_IN4、CH2=ADC2_IN5，满足约束；若改成同一引脚，手册明确禁止（风险 R16）。

### 6.5 CDR → DMA2 Stream0 → SRAM

**DMA 配置**（`adc.c:284-310`）：

| 项 | 值 | 手册/代码 |
|---|---|---|
| 流/通道 | DMA2 Stream0 / Channel0（ADC1） | `adc.c:286`；【RM Table 44 p.310】DMA2 请求映射 |
| 外设地址 | `&ADC->CDR` | `adc.c:287`；【RM §13.13.17 p.432】地址 = ADC1 基址+0x300+0x08 |
| 存储器地址 | `g_adc_dma_buf`（4096 字） | `adc.c:11`、`:288` |
| 方向 | 外设→存储器 | `adc.c:289` |
| 数据宽度 | 外设 Word / 存储器 Word | `adc.c:293-294` |
| 模式 | **Circular** | `adc.c:295`；【RM §10.3.8 p.315】 |
| 优先级 | High | `adc.c:296` |
| FIFO | 关闭、单次 burst | `adc.c:297-300` |
| 中断 | HT+TC | `adc.c:303`；NVIC 抢占 1/子 0（`adc.c:307-308`） |

**为什么是 4096 字**：`ADC_DMA_BUF_LEN=4096`（`adc.h:10`）= 2 帧 × 2048 对。HT 中断发生在第 2048 字（第 1 帧满），TC 发生在第 4096 字（第 2 帧满），循环重装后回到 0【RM §10.3.8 p.315】。ISR 只做两件事：清标志、记录当前帧在缓冲区里的偏移（0 或 2048），并置 `s_frame_ready`（`adc.c:18-33`）。

**DMA 请求率**：每个触发一次请求（一对数据）→ 200 k 请求/s、每次 4 B = **800 kB/s**；500 kSa/s 时 **2 MB/s**。相对 SRAM 理论带宽（168 MHz × 4 B ≈ 672 MB/s）占 0.12%/0.3%，可忽略【推导】。

**为什么不用“两个 DMA 流各搬一路”**：也可以，但会占两个流、两次请求、两个中断；DMA 模式 2 用硬件打包把带宽和中断数减半，这是 ST 为双 ADC 设计的标准用法【RM §13.9 p.405】。

### 6.6 DDS：为什么必须置 1

`ADC_CCR.DDS`（多 ADC 模式）：

- **DDS=0**：DMA 传输完 `NDTR` 个数据后**不再发新请求**，且 DMA 位不会被硬件清零；“必须软件先清 0 再置 1 才能开始新传输”【RM §13.13.16 p.430；§13.8.1 p.402】。
- **DDS=1**：只要 ADC 还在转换且 DMA=01/10/11，请求就继续发，配合 Circular 形成永不停止的采集【同上】。

代码在初始化和每次改采样率后都调用 `ADC_MultiModeDMARequestAfterLastTransferCmd(ENABLE)`（`adc.c:318`、`adc.c:378`），注释也明确写了“否则第一轮循环缓冲结束后 ADC 不再发 DMA 请求，屏幕永远停在最后一帧”（`adc.c:315-317`、`:375-377`）。这是本工程一个**非常关键的芯片机制点**，答辩很可能被追问。

### 6.7 OVR：什么时候发生、代码怎么处理

**发生条件**：ADC 转换完成但数据没被及时读走（`ADC_DR` 被覆盖），`ADC_SR.OVR` 置 1；只有 `DMA=1` 或 `EOCS=1` 时才使能检测【RM §13.13.1 p.417】。

**DMA 模式下的行为**（手册原文要点）：
> “When OVR = 1 in DMA mode, the DMA requests are blocked after the last valid data have been transferred…”【RM §13.8.1 p.402】
> 恢复步骤：① 重新初始化 DMA（调整目标地址与 NDTR）② 清 `ADC_SR.OVR` ③ …【同上】

**代码**：`ADC1_CopyFrame()` 在每次主循环开头轮询并清 ADC1/ADC2 的 OVR（`adc.c:51-58`），但**没有重新初始化 DMA**。这与手册的推荐恢复流程不完全一致：在某些 OVR 场景下，仅清标志可能不足以让请求恢复。工程里还有一层保险：`CMD_Task()` 发现“长时间没有新帧”就调用 `ADC1_Init()` 整条链路重建（`cmd.c:1080-1097`）。因此：
- 正常采集（DMA 优先级 High、总线不拥塞）几乎不会 OVR；
- 若因调试/异常触发 OVR，最坏情况是“停采 → 看门狗重初始化 → 恢复”，期间会丢帧并可能重置采样率（风险 R2、R12）。

### 6.8 帧形成与“双缓冲”

```c
volatile u32 g_adc_dma_buf[4096];   // DMA 目的地（adc.c:11）
static u16 s_frame[2][2048];        // 中值滤波后的显示帧（adc.c:14）
static u16 s_raw_frame[2][2048];    // 解包后的原始帧（adc.c:15）
static volatile u8  s_frame_ready;  // HT/TC 置位（adc.c:12）
static volatile u16 s_frame_offset; // 0 或 2048（adc.c:13）
```

**拷贝流程**（`adc.c:43-90`）：

1. 清 OVR（ADC1/ADC2）。
2. `s_frame_ready==0` 直接返回；否则清标志、取 offset。
3. 把 `g_adc_dma_buf[offset+i]` 解包成 CH1/CH2 原始帧（`i=0..2047`）。
4. 对两路各做 **3 点中值滤波**（首尾点保持），去掉单点尖峰而不牺牲带宽（`adc.c:73-87`）。

**为什么安全**：拷贝 2048 字 + 中值约几万条指令（µs~百µs 量级），而 DMA 写满另一半需要 10.24 ms（200 k）——拷贝期间 DMA 不会覆盖正在读的那一半。严格地说，这是“半缓冲交替”而不是“双缓冲指针切换”，但在当前速率下等效安全（【估算】，未实测最坏情况）。

### 6.9 软件触发点搜索与帧旋转

- **触发不是硬件模拟比较器**：ADC 没有配置模拟看门狗/注入触发来“对齐触发”，而是把整帧采回来后，在 `adc_find_trigger()` 里找电平穿越（`adc.c:102-182`）。
- **自动触发**：先算 min/max，span<80 LSB 视为无信号；以中值 `(min+max)/2` 为门限、`span/8` 为迟滞，先“武装”（低于/高于门限）再等穿越（`adc.c:135-179`）。
- **手动触发**：`TRIG <电压>` 把 0–3.3 V 映射到 0–4095（`cmd.c:441`），直接找 `level` 穿越。
- **旋转**：`CMD_Task()` 以触发点为 0，把两路帧循环移位到 `s_disp_frame1/2`（`cmd.c:1160-1187`），因此屏幕上触发点永远在左边缘——**没有预触发（负时间）显示**，这是与台式示波器的功能差异（风险 R11）。
- **NORM/SINGLE/AUTO**：都是软件策略（`cmd.c:1127-1158`）：NORM 找不到触发就冻结；SINGLE 捕获一帧后 `g_run=0`；AUTO 丢一帧后自由运行。

---

## 7. CPU / 显示 / 串口数据通路

### 7.1 主循环节拍

| 环节 | 位置 | 周期/开销 |
|---|---|---|
| `CMD_Task()` | `cmd.c:1014-1250` | 每轮一次；有帧时做触发搜索/旋转/测量类型 |
| `UI_Task()` | `ui.c:2206-2265` | 触摸扫描 + 条件刷新，**末尾 `delay_ms(20)`**（`ui.c:2264`） |
| 最小循环周期 | — | ≈20 ms + 绘制时间 → UI 上限 ~50 Hz |
| 采集帧率 | 200 kSa/s | 2048/200k = **10.24 ms**（≈97.7 帧/s） |
| 500 kSa/s | — | 4.096 ms（≈244 帧/s） |

**结论**：采集帧率高于 UI 循环率，`UI_Task` 每次只取“最新完成的一帧”（`CMD_DisplaySeq()` 变化才重绘），中间的帧被自然丢弃——对示波器显示是合理取舍；但**没有丢帧计数**，答辩时不要声称“帧帧都上屏”。

### 7.2 测量链（CPU 侧）

`MEAS_Compute()`（`measure.c:406-615`）对 2048 点做：

1. min/max/DC/RMS（单遍循环）。
2. 过零检测 + 线性插值（`measure.c:35-73`）→ 频率（`measure.c:497-505`），频率限 20–60 000 Hz（`measure.c:501`）。
3. Hann 窗 2048 点 FFT（`arm_cfft_f32`，`measure.c:224-289`）→ 峰值 bin + 抛物线插值；**每 4 次调用才重算一次**（`measure.c:391-403` 的缓存策略）。
4. 单频 Goertzel 取基波/3/5 次谐波算 THD（`measure.c:191-219`、`529-536`）。
5. 斜率变异系数（三角/正弦区分）、crest factor、削顶计数 → 波形类型 + 置信度（`measure.c:547-614`）。
6. 双通道相位：Hann 窗单频相关（I/Q），不依赖触发点（`measure.c:620-674`）。

**Nyquist 与“测得到”的区别**：fs=200 kSa/s 时 Nyquist=100 kHz，但代码把频率上限卡在 60 kHz；且模拟前端没有抗混叠滤波，超过 Nyquist 的信号会折叠（风险 R7）。测量精度还受 12 位量化（3.3 V/4096 ≈ 0.806 mV/LSB）与 VDDA 实际值影响（风险 R14）。

### 7.3 LCD + FSMC 通路

**硬件配置**（`lcd.c:582-690`）：

| 项 | 值 | 依据 |
|---|---|---|
| 片选 | FSMC Bank1 **NE4**（NOR/PSRAM4） | `lcd.c:672`；【RM Table 216 p.1536】HADDR[27:26]=11 |
| 基址 | `0x6C000000`，`LCD_BASE=0x6C000000\|0x7E` | `lcd.h:62`；【RM §36.4.1 p.1535-1536】 |
| 数据宽度 | 16 位 | `lcd.c:675`；【RM Table 217 p.1536】16 位时 `HADDR[25:1]>>1` |
| 存储器类型 | SRAM（异步） | `lcd.c:674` |
| 时序模式 | **Mode A**（OE 翻转） | `lcd.c:660/669`；【RM §36.5.x p.1544】 |
| 读时序 | ADDSET=0xF(15)，DATAST=60 | `lcd.c:654-656`；【RM Table 229 p.1546】“ADDSET HCLK cycles / DATAST HCLK cycles” |
| 写时序 | ADDSET=9，DATAST=8 | `lcd.c:663-665` |
| 命令/数据 | A6=0 写寄存器，A6=1 写数据 | `lcd.h:60-63`；`lcd.c:43-54` |

**时序计算**（【推导】）：

```
写一个 16 位像素 = (ADDSET + DATAST) / fHCLK
               = (9 + 8) / 168 MHz
               = 17 × 5.952 ns ≈ 101.2 ns
→ 理论像素率 ≈ 9.88 Mpx/s
→ 全屏 320×240=76 800 像素 ≈ 7.77 ms
读一个像素 = (15 + 60)/168 MHz ≈ 446 ns（仅初始化读 ID 用）
```

> 注：若某些时序图按 `DATAST+1` 计算，则写周期为 113 ns、全屏 8.7 ms（待核对 N5）。无论哪种，量级都是“**每写一屏 8–9 ms**”。

**绘制算法与 CPU 代价**：

- 视窗 `PLOT_W = 278−42+1 = 237` 列、`PLOT_H = 111` 行（`ui.c:57-62`）。
- 采样点多于列数时：每列算 min/max，画一条竖线 + 连接段（`ui.c:976-1056`）。
- 采样点少于列数时：列内线性插值，逐点 `LCD_DrawLine`（`ui.c:928-975`）。
- 动态刷新前先“擦旧波形/触发线/零线”，再补网格（`ui.c:862-870`）。
- 局部刷新用“签名”判断（`ui.c:1403-1435`、`2214-2248`），避免整屏填充导致闪烁。

**估算**：一列若幅度 100 像素，237 列 ≈ 2.4 万次像素写 ≈ 2.4 ms；擦除再加一次 ≈ 4.8 ms；加文字/参数卡可能 5–8 ms/帧。全屏重绘（面板填充 308×144≈4.4 万像素）≈ 4.5 ms + 文本（【估算】）。

### 7.4 USART2 + DMA1 Stream6

**配置**（`usart.c:158-228`）：

| 项 | 值 | 依据 |
|---|---|---|
| 引脚 | PA2=TX、PA3=RX，AF7 | `usart.c:168-176` |
| 波特率 | 460800，8N1，无流控 | `usart.c:178-184`、`main.c:14` |
| TX DMA | DMA1 Stream6 / Channel4，8 位，Normal | `usart.c:194-214`；【RM Table 43 p.309】DMA1 请求映射 |
| TX 完成 | DMA1 Stream6 TC 中断，清 busy | `usart.c:256-264` |
| RX | USART2 RXNE 中断 + 512 B 环形缓冲 | `usart.c:266-309` |
| 方向 | 存储器→外设 | `usart.c:202` |

**波特率误差**（【推导】）：

```
USARTDIV = PCLK1 / (16 × baud) = 42 000 000 / (16 × 460 800) = 5.6966
BRR: 尾数 5，小数 round(0.6966×16)=11 → 实际 5+11/16 = 5.6875
实际波特率 = 42 000 000 / (16 × 5.6875) = 461 538.46 → 误差 +0.16%
```
在 UART 容忍范围内（通常 <2–3%）。USART 波特率公式与 USARTDIV 编码见【RM §30.3.4 p.973】；TX DMA 使能为 `USART_CR3.DMAT`【RM p.1010】。

**吞吐**（8N1 = 10 bit/byte）：

```
46 080 B/s
MEAS? 回复 ~200 B → ~4.3 ms
WAVE,512 约 512×5 B ≈ 2.5 KB → ~54 ms
FireWater 每帧最多 256 行 × ~10 B ≈ 2.5 KB → 理论 ~18 Hz 上限
```
发送缓冲 `UART2_TX_BUF_SIZE=12288`（`usart.h:11`），`uart2_send_dma()` 在 busy 时直接拒绝（`usart.c:240-243`），上层用 `g_pending/g_wave_pending` 排队（`cmd.c:128-145`、`1062-1075`）。

### 7.5 触摸与 EEPROM（CPU 轮询路径）

- **UI 每个循环都调用 `tp_dev.scan(0)`**（`ui.c:2127`），触摸是**纯轮询**，不占中断。
- LCD 是 0x5510（NT35510）时走**电容屏** GT9147/OTT2001A，软件 IIC 用 **PB0(SCL)/PF11(SDA)**（`ctiic.h`、`touch.c:508-520`）；否则走**电阻屏** SPI（PB1/PB2/PB0/PC13/PF11，`touch.c:525-548`）。
- 电阻屏校准值存 **AT24C02**（2 Kbit，256 B）地址 40 起，标记 `0x0A` 在 54、屏幕方向在 55（`touch.c:226-231`、`ui.c:2176-2185` 读地址 55）。
- **CPU 代价**：软件 IIC 每位有 `delay_us(2)` 级延时、起始/停止有 `delay_us(30)`（`ctiic.c`），一次 GT9147 读状态+坐标约几十字节 → **百 µs~1 ms/次**；电阻屏 SPI 每次坐标转换 `delay_us(6)+` 位延时（`touch.c:44-82`）→ 百 µs 量级（【估算】）。这部分与 LCD 绘制一起构成 UI 的固定开销。

### 7.6 NVIC 与中断行为

| 中断 | 抢占/子优先级 | 工作内容 | 说明 |
|---|---|---|---|
| DMA2_Stream0（ADC） | 1 / 0（最高） | 清 HT/TC 标志、置帧就绪/偏移 | 必须最高：晚处理会 OVR（`adc.c:306-310`） |
| DMA1_Stream6（USART TX） | 2 / 0 | 清 TC、关流、清 busy | 次高（`usart.c:221-225`） |
| USART2（RXNE） | 3 / 0 | 取字节进 512 B 环 | 最低（`usart.c:188-192`） |
| SysTick | 默认最低 | 空函数（`stm32f4xx_it.c:142-145`） | 仅 `delay_us` 轮询 COUNTFLAG |

分组 `NVIC_PriorityGroup_2` → 2 位抢占（0–3）+ 2 位子优先级（0–3）【PM §2.3.6 p.41；RM §12.1 p.373 给出 4 位优先级=16 级】。ADC DMA 能打断 USART 中断，反之不能——符合“采集优先于通信”的设计。

**注意**：ADC 的 OVR **不是中断**，而是在主循环 `ADC1_CopyFrame()` 里轮询清零（`adc.c:51-58`）。若 UI 长时间阻塞（全屏重绘、触摸校准），OVR 的处理会被推迟；但 DMA 硬件仍持续搬运，真正会触发 OVR 的是“ADC 转换完成时上一次数据未被 DMA 读走”，正常配置下极少发生。

---

## 8. 关键指标表（公式 + 当前值 + 边界）

### 8.1 时钟与采样

| 指标 | 公式 | 当前值/示例 | 边界与依据 |
|---|---|---|---|
| SYSCLK | (HSE/M)×N/P | 168 MHz | HSE 8 MHz，M8/N336/P2【代码 `system_stm32f4xx.c:316,329,331`】 |
| HCLK | SYSCLK/AHB_div | 168 MHz | AHB div=1【`system_stm32f4xx.c:598`】 |
| PCLK1 / PCLK2 | HCLK/4、HCLK/2 | 42 / 84 MHz | 【`system_stm32f4xx.c:602,605`】 |
| TIM3CLK | 2×PCLK1（PPRE1≠1） | 84 MHz | 【RM §7.2 p.218】 |
| ADCCLK | PCLK2/ADCPRE | 21 MHz | ADCPRE=/4【`adc.c:261`】；上限 36 MHz【DS Table 67 p.135】 |
| 触发频率 | 84 MHz/((PSC+1)(ARR+1)) | 200 kHz（ARR=419） | 500 kHz→ARR=167；下限 1281.74 Hz（16 位） |
| 单 ADC 转换时间 | (N+12)/21 MHz | 1.286 µs（15 cyc） | 4.571 µs（84）；23.43 µs（480）【RM §13.5 p.399】 |
| 单 ADC 上限 | 21 MHz/(N+12) | 777.8 kSa/s（15 cyc） | 218.75 k（84）；42.68 k（480） |
| 双通道有效采样率 | = 触发频率（并行） | 200 k / 500 kSa/s | 每触发一对样本 |
| 外部触发频率上限（DS 参考） | — | 1764 kHz @fADC=30 MHz、12 位；本设计 21 MHz/15cyc 理论 777.8 kSa/s | 【DS Table 67 p.135】 |
| 规则触发转换延迟 tlatr | 0.067 µs @30 MHz（2 ADCCLK） | 21 MHz 约 0.095 µs | 【DS Table 67 p.136】 |
| Nyquist | fs/2 | 100 kHz@200k；250 kHz@500k | 代码测频上限 60 kHz（`measure.c:501`） |
| 帧时间 | 2048/fs | 10.24 ms@200k；4.096 ms@500k | 2048 点/帧（`adc.h:11`） |
| 帧率 | fs/2048 | 97.7 Hz@200k；244 Hz@500k | UI 只取最新帧 |

### 8.2 数据搬运与显示/串口

| 指标 | 公式 | 当前值 | 备注 |
|---|---|---|---|
| ADC→SRAM 带宽 | fs × 4 B | 800 kB/s@200k；2 MB/s@500k | 占 SRAM 理论带宽 0.12%/0.3%【推导】 |
| DMA 缓冲 | 4096 words × 4 B | 16 KB | 2 帧 × 2048 对【`adc.c:11`】 |
| HT/TC 间隔 | 2048/fs | 10.24 ms / 20.48 ms@200k | 循环模式自动重装【RM §10.3.8 p.315】 |
| USART 吞吐 | baud/10（8N1） | 46 080 B/s | 460 800 baud；误差 +0.16%【推导】 |
| USART 单次回复 | 字节数×10/baud | MEAS? ~4.3 ms；WAVE,512 ~54 ms | 缓冲 12 KB【`usart.h:11`】 |
| LCD 写像素 | (ADDSET+DATAST)/HCLK | ≈101 ns（17 HCLK） | 9.88 Mpx/s；全屏 ≈7.8 ms【推导】 |
| LCD 读像素 | (15+60)/HCLK | ≈446 ns | 仅读 ID/状态用 |
| LCD 全屏数据量 | 320×240×2 B | 153 600 B | 16 位色 |
| UI 循环 | delay 20 ms + 绘制 | ≤50 Hz | `ui.c:2264` |
| Flash/RAM | map 统计 | ROM 111.70 KB；RW 74.23 KB（RW 324 B + ZI 75 684 B） | `OBJ\Base.map`【代码】 |
| 栈/堆 | startup | Stack 1 KB、Heap 512 B | `startup_stm32f40_41xxx.s:41,52`【代码】 |
| 12 位量化 | VDDA/4096 | ≈0.806 mV/LSB | VDDA 视为 3.3 V（风险 R14） |

### 8.3 静态 RAM 分布（为什么 74 KB 是合理的）

| 缓冲 | 大小 | 位置 |
|---|---|---|
| `g_adc_dma_buf[4096]`（u32） | 16 384 B | `adc.c:11` |
| `s_frame[2][2048]`（u16） | 8 192 B | `adc.c:14` |
| `s_raw_frame[2][2048]`（u16） | 8 192 B | `adc.c:15` |
| `s_disp_frame1/2[2048]`（u16） | 8 192 B | `cmd.c:107-108` |
| `s_fft_buf[4096]`（float32） | 16 384 B | `measure.c:15` |
| `g_uart2_tx_buf[12288]`（char） | 12 288 B | `usart.c:230` |
| UI 轨迹数组 4×2×237×u16 | 3 792 B | `ui.c:143-146` |
| 栈 + 堆 | 1 536 B | startup |
| 其余全局/静态 | 约 1–2 KB | 各文件 |

合计 ≈ 73–74 KB，与 map 的 ZI 75 684 B 一致。**注意**：`s_frame/s_raw_frame/s_disp_frame` 三份 8 KB 是同一个 2048 点帧的不同阶段副本，存在优化空间（但当前 128 KB SRAM1 够用）。

### 8.4 CPU 负载（估算，非实测）

- 每次 `MEAS_Compute`：2048 点单遍统计 + 过零 + FFT（每 4 次一算）+ 3 次 Goertzel + 斜率拟合 + 类型判定。FFT 摊薄后估计 **0.2–0.6 ms/次**（168 MHz、O1 量级，【估算】）。
- 每帧被调用次数：`ui_draw_wave_area` 2 次（`ui_channel_signal`）+ 相位块 2 次 + `ui_draw_params` 2 次 + `CMD_Task` 类型判定 1 次 + 自动量程每 5 tick 2 次 → **约 6–10 次/帧**。
- 结论：**测量+绘制合计约 5–15 ms/20 ms 循环 → CPU 占用约 25–75%**（O0 优化下更高），余量用于命令解析与串口流。**但这不影响采样**：采样由 TIM3+ADC+DMA 硬件完成，CPU 卡顿只体现为显示帧率下降/测量刷新变慢（【估算】）。

---

## 9. 代码 → 芯片资源映射表

> 说明：本表只列“机制相关”的关键点；完整初始化清单见 `stm32-peripheral-traceability.md`。

### 9.1 时钟 / 启动 / 延时

| 功能 | file:line | 外设/寄存器 | 库函数/写法 | 总线/时钟 | 手册 |
|---|---|---|---|---|---|
| HSE 8 MHz 定义 | `USER\stm32f4xx.h:123` | `HSE_VALUE` | 宏 | — | 【RM §7.2.1 p.219】 |
| 开 HSE | `system_stm32f4xx.c:573` | `RCC_CR.HSEON` | 直接写寄存器 | — | 【RM §7.3.1 p.225-226】 |
| 电压 Scale1 | `system_stm32f4xx.c:594-595` | `RCC_APB1ENR.PWREN`、`PWR_CR.VOS` | 直接写 | APB1 | 【RM §5.4.1 PWR_CR p.141（Bit14 VOS）】 |
| AHB/APB 分频 | `system_stm32f4xx.c:598-605` | `RCC_CFGR.HPRE/PPRE1/PPRE2` | 直接写 | — | 【RM §7.3.3 p.229】 |
| PLL 参数 | `system_stm32f4xx.c:316,326,329,331` | `PLL_M/N/P/Q` | 宏 | — | 【RM §7.2.3 p.220】 |
| 写 PLLCFGR | `system_stm32f4xx.c:617-618` | `RCC_PLLCFGR` | 直接写 | — | 【RM §7.3.2 p.227】 |
| 切换 SYSCLK 到 PLL | `system_stm32f4xx.c:653-657` | `RCC_CFGR.SW/SWS` | 直接写 | — | 【RM §7.3.3 p.229】 |
| Flash 5WS+ART | `system_stm32f4xx.c:644` | `FLASH_ACR` | 直接写 | — | 【RM §3.5.1 p.80-81】 |
| SysTick HCLK/8 | `delay.c:120-121` | `SysTick->CTRL/LOAD` | `SysTick_CLKSourceConfig` | 21 MHz | 【PM §4.5 p.246】 |
| 微秒延时 | `delay.c:179-191` | SysTick 轮询 | 直接写 | — | 【RM §7.2 p.218】 |

### 9.2 采集链（核心）

| 功能 | file:line | 外设/寄存器 | 库函数/写法 | 总线/时钟 | 手册 |
|---|---|---|---|---|---|
| 开 GPIOA/DMA2 时钟 | `adc.c:235` | `RCC_AHB1ENR` | `RCC_AHB1PeriphClockCmd` | AHB1 | 【RM §7.3.10 p.243】 |
| 开 ADC1/ADC2 时钟 | `adc.c:236` | `RCC_APB2ENR` | `RCC_APB2PeriphClockCmd` | APB2 84 MHz | 【RM §7.3.14 p.249】 |
| 开 TIM3/DAC 时钟 | `adc.c:237` | `RCC_APB1ENR` | `RCC_APB1PeriphClockCmd` | APB1 42 MHz | 【RM §7.3.13 p.246】 |
| 关 DAC 输出缓冲 | `adc.c:243-248` | `DAC_CR.BOFF1/BOFF2`、`DAC_DHR12R1/2` | 直接写后关时钟 | APB1 | 【RM §14.3.1 p.437；DAC_CR BOFF1 p.449】 |
| PA4/PA5 模拟输入 | `adc.c:253-258` | `GPIOA_MODER` 模拟 | `GPIO_Init` | AHB1 | 【DS2 Table 6 p.46】 |
| 双 ADC 公共配置 | `adc.c:260-264` | `ADC_CCR`（MULTI/ADCPRE/DMA/DELAY） | `ADC_CommonInit` | APB2 | 【RM §13.13.16 p.429-430】 |
| ADC1 规则组（IN4） | `adc.c:266-273` | `ADC_CR1/CR2/SMPR1` | `ADC_Init` | APB2 | 【RM §13.13.2-3 p.418-421】 |
| ADC2 规则组（IN5，无触发） | `adc.c:277-278` | `ADC2_CR2.EXTEN=00` | `ADC_Init` | APB2 | 【RM §13.9 p.403 注】 |
| 采样时间分档 | `adc.c:210-223` | `ADC_SMPR1/2` | `ADC_RegularChannelConfig` | APB2 | 【RM §13.5 p.399】 |
| DMA2 Stream0 Ch0 | `adc.c:284-301` | `DMA_S0CR/NDTR/PAR/M0AR` | `DMA_Init` | AHB1 | 【RM Table 44 p.310；§10.3 p.311-315】 |
| DMA HT/TC 中断 | `adc.c:303-310` | `DMA_S0CR`、`NVIC_ISER` | `DMA_ITConfig`/`NVIC_Init` | — | 【RM §10 p.304；§12.1 p.373】 |
| 清 OVR | `adc.c:312-313,360-361` | `ADC_SR.OVR` | `ADC_ClearFlag` | APB2 | 【RM §13.13.1 p.417】 |
| ADC DMA 使能 | `adc.c:314,374` | `ADC_CR2.DMA` | `ADC_DMACmd` | APB2 | 【RM §13.13.3 p.421】 |
| **DDS=1** | `adc.c:318,378` | `ADC_CCR.DDS` | `ADC_MultiModeDMARequestAfterLastTransferCmd` | APB2 | 【RM §13.13.16 p.430】 |
| 开 ADC2/ADC1 | `adc.c:319-320,379-380` | `ADC_CR2.ADON` | `ADC_Cmd` | APB2 | 【RM §13.3.1 p.392】 |
| TIM3 时基 | `adc.c:325-332` | `TIM3_PSC/ARR/CR1` | `TIM_TimeBaseInit` | APB1×2=84 MHz | 【RM §18 p.586】 |
| TRGO=更新事件 | `adc.c:331` | `TIM3_CR2.MMS=010` | `TIM_SelectOutputTrigger` | — | 【RM §18.4.3 p.626（MMS 归属见待核对 N1）】 |
| 改采样率 | `adc.c:335-388` | `TIM3->PSC/ARR/EGR/SR`、`ADC_CR2`、`DMA` | 直接写 + 库 | — | 【RM §18.4.2 p.625】 |
| 帧拷贝/滤波 | `adc.c:43-90` | SRAM（CPU） | — | AHB/SRAM | — |

### 9.3 显示 / 串口 / 触摸

| 功能 | file:line | 外设/寄存器 | 库函数/写法 | 总线/时钟 | 手册 |
|---|---|---|---|---|---|
| FSMC 时钟/GPIO | `lcd.c:591-592,602-651` | `RCC_AHB3ENR.FSMCEN`、`GPIOx_AFRL/AFRH` | `RCC_AHB3PeriphClockCmd`、`GPIO_PinAFConfig` | AHB3 168 MHz | 【RM §36 p.1532】 |
| FSMC 时序/银行 | `lcd.c:654-690` | `FSMC_BTR/BWTR/BCR` | `FSMC_NORSRAMInit/Cmd` | — | 【RM §36.5.6 p.1565；Table 229 p.1546】 |
| 写寄存器/数据 | `lcd.c:43-54` | `LCD->LCD_REG/LCD_RAM`（0x6C00007E/0x80） | 直接写 | FSMC | 【RM §36.4.1 p.1535-1536】 |
| 画点/填充/画线 | `lcd.c:373-422,2426-2517` | FSMC 写显存 | — | — | — |
| 显示初始化 | `ui.c:2163-2166` | LCD_Init/方向/清屏 | — | — | — |
| USART2 GPIO/AF | `usart.c:165-176` | `GPIOA_AFRL=AF7` | `GPIO_PinAFConfig` | AHB1 | 【RM §30 p.960】 |
| USART2 参数 | `usart.c:178-185` | `USART2_BRR/CR1/CR2/CR3` | `USART_Init` | APB1 42 MHz | 【RM §30.3.4 p.973】 |
| USART2 TX DMA | `usart.c:194-227` | `DMA1_S6CR/PAR/M0AR`、`USART_CR3.DMAT` | `DMA_Init`、`USART_DMACmd` | AHB1/APB1 | 【RM Table 43 p.309；p.1010】 |
| RX 环 + 中断 | `usart.c:266-309` | `USART2_CR1.RXNEIE`、NVIC | `USART_ITConfig` | — | 【RM §30 p.960】 |
| 触摸初始化 | `touch.c:504-566` | GPIO/SPI 位翻转或 CT_IIC | — | — | — |
| EEPROM 校准 | `touch.c:226-231`、`ui.c:2176-2185` | 24C02（I2C，PB8/PB9） | `AT24CXX_*` | GPIO 位翻转 | — |
| 中断分组 | `main.c:11` | `SCB->AIRCR.PRIGROUP` | `NVIC_PriorityGroupConfig` | — | 【PM §2.3.6 p.41】 |

---

## 10. 风险与边界（按优先级）

> 每条给“现象 → 机理 → 依据 → 代码 → 建议”。答辩被问“这套代码哪里可能出问题”时按表回答。

### R1（高）TIM3 是 16 位，采样率低于 ~1282 Hz 时时基会错

- **现象**：自动量程在低频（<0.5 Hz 判断）时会把 fs 逐次减半（`cmd.c:737-744`），可低到 1000 Hz（`adc.c:339`）；此时屏幕时间轴与真实采样率不符。
- **机理**：TIM3 是 16 位定时器（【RM §18.1 p.586】：“16-bit (TIM3 and TIM4) or 32-bit (TIM2 and TIM5)”）。`arr = 84 000 000/fs − 1`（`adc.c:342`）在 fs=1000 时为 83 999，写入 16 位 `ARR` 只会保留低 16 位（0x1481F→0x481F=18 463）→ 实际 fs≈4 549 Hz；fs=1281 时更极端（ARR=65 573→37→约 2.21 MHz）。
- **建议**：把 `fs` 下限抬到 ≥1282 Hz（或 ≥2048 Hz，与手动时基下限一致），或在低采样率时增大 `TIM3->PSC`（例如 PSC=7 → 12 MHz 计数，ARR 范围 ×8）。**这是必须修复的硬边界。**

### R2（中高）OVR 只清标志、不重配 DMA，与手册恢复流程不一致

- **现象**：极端情况下（调试打断、总线异常、误触发）ADC 停采，屏幕冻结，直到 `CMD_Task` 的“无帧看门狗”调用 `ADC1_Init()`（`cmd.c:1080-1097`）。
- **机理**：DMA 模式下 OVR=1 后 DMA 请求被阻塞；手册要求“重新初始化 DMA（地址+NDTR）→ 清 OVR → …”（【RM §13.8.1 p.402】）。代码只做了“清 OVR”（`adc.c:51-58`）。
- **建议**：OVR 时同时 `DMA_Cmd(DISABLE)`、`DMA_SetCurrDataCounter(4096)`、清全部 DMA 标志、再 `DMA_Cmd(ENABLE)`；或把 OVR 检测升级为“整链路重建”并记录事件计数。

### R3（中）40 kHz 档余量只有 1.07×，接近转换极限

- **机理**：fs=40 kHz 时用 480 cycles（23.43 µs），而触发周期 25 µs，只剩 1.57 µs；再叠加触发延迟/时钟同步，可能丢触发（ADC 正在转换时新触发被忽略）。
- **依据**：转换时间公式【RM §13.5 p.399】；DS 给出触发延迟 `tlatr` 典型 0.067 µs@30 MHz（【DS Table 67 p.136】，21 MHz 约 0.096 µs）。
- **建议**：把 480 cycles 档的上限从 40 kHz 降到 35 kHz，或对 40 kHz 用 84 cycles（4.57 µs，余量 5.5×）；480 cycles 留给 ≤20 kHz。

### R4（中）500 kHz 档余量 1.56×，且“触发被忽略”不会报错

- **机理**：2 µs 周期 vs 1.286 µs 转换；若 ADC 因任何原因晚启动，下一次触发落在转换中会被忽略，实际采样率降到 ~778 kSa/s 以下但**没有标志位**，时基/FFT 频率会偏。
- **建议**：500 kSa/s 只作为“上限演示”，并在答辩中说明“受 21 MHz ADCCLK 限制，15 cycles 时理论极限 777.8 kSa/s”；若要稳定 500 k，可把 ADCCLK 提到 28 MHz（PCLK2/3 不支持，只有 /2/4/6/8 → /2=42 MHz 超 36 MHz 上限，故 21 MHz 已是最佳折中）。

### R5（中）APB1 定时器 ×2 是硬编码前提

- **机理**：`adc.c:342` 写死 `84 000 000`，前提是 PPRE1=DIV4（PCLK1=42）。若改时钟树（例如降频到 144 MHz），采样率与时基全错。
- **建议**：用 `RCC_GetClocksFreq()` 动态取 `PCLK1` 并按“APB1 预分频≠1 则 ×2”计算 TIM3CLK；或加编译期断言。

### R6（高，硬件相关）PA4/PA5 的复用冲突

- **PA4**：`ADC12_IN4`、`DAC_OUT1`、**DCMI_HSYNC**、`USART2_CK`、`SPI1_NSS/SPI3_NSS`、`OTG_HS_SOF`、`I2S3_WS`、`EVENTOUT`（【DS2 Table 6 p.46】；【DS Table 7 p.52】）。
- **PA5**：`ADC12_IN5`、`DAC_OUT2`、`SPI1_SCK`、`OTG_HS_ULPI_CK`、`TIM2_CH1_ETR`、`TIM8_CH1N`、`EVENTOUT`（同上）。
- **机理**：只要 DAC 通道使能，PA4/PA5 会自动接到 DAC 输出（【RM p.436】：“Once the DAC channelx is enabled, the corresponding GPIO pin (PA4 or PA5) is automatically connected to the analog converter output”）。代码先写 `DAC->CR = BOFF1|BOFF2` 关缓冲、清 DHR，再**关 DAC 时钟**（`adc.c:243-248`），使 PA4/PA5 保持高阻模拟输入——顺序正确。
- **残余风险**：① 板上 PA4 网络是 `STM_DAC`（带 RC/测试点，见项目文档 `dual-channel-pin.md:28`），不是干净模拟输入；② 插摄像头时 PA4 被 DCMI_HSYNC 驱动；③ 若其他代码使能 SPI1/USART2_CK，会争用 PA4/PA5。**答辩时要主动说明“CH1 用 PA4 是板级接线妥协，量产应改 PA0/PA6 等专用模拟脚”。**

### R7（中）没有抗混叠滤波，输入范围与 3.3 V 假设

- **机理**：信号直接进引脚，没有 RC/运放抗混叠；超过 Nyquist 的分量会折叠到低频，FFT/频率读数会假。输入必须 0–VDDA（DS 的 `VAIN` 转换范围，【DS2 Table 65 p.118】），负电压/过压会通过 ESD 二极管灌电流。
- **建议**：答辩时明确“前端需加 0–3.3 V 调理 + 抗混叠低通”；软件上可加“频谱能量在 Nyquist 附近异常”告警（当前没有）。

### R8（低-中）FSMC 与 SRAM 的“总线竞争”要分清对象

- **事实**：DMA 与 CPU 共享 SRAM 端口，但 DMA 只占 0.12% 理论带宽（【推导】）；FSMC 访问走 AHB3，不占 SRAM 带宽。
- **真正的竞争**：FSMC 是**同步阻塞**的（CPU 写 `LCD->LCD_RAM` 要等外部总线时序），每像素 ~17 HCLK，全屏 ~7.8 ms；这期间 CPU 不能做测量/串口解析。因此“总线竞争”的准确说法是“**CPU 时间被 FSMC 占用**”，不是“SRAM 带宽被 DMA 抢走”。

### R9（中）1 KB 栈 + 浮点/sprintf

- **依据**：`Stack_Size=0x400`（1 KB）、`Heap_Size=0x200`（512 B）（`startup_stm32f40_41xxx.s:41,52`）。
- **机理**：`cmd_measure()` 里 `char resp[300]`、`MEAS_Result`、`sprintf("%.3f")` 的浮点格式化会消耗较多栈；`measure.c` 的 `meas_cross_t`（约 136 B）等局部变量叠加。当前未溢出（map 无栈使用告警），但余量不大。
- **建议**：把大响应缓冲改为静态；栈加到 2 KB；避免在中断里用 printf（当前没有）。

### R10（中）UI 是阻塞式前后台，触摸/绘制会拖慢测量刷新

- **依据**：`UI_Task()` 末尾 `delay_ms(20)`（`ui.c:2264`）；触摸是轮询（`ui.c:2127`）；软件 IIC/SPI 触摸有 µs 级延时（`ctiic.c`、`touch.c:44-82`）。
- **机理**：`delay_ms` 期间 CPU 空转（SysTick 轮询），采集不受影响但显示/测量最多 50 Hz；触摸扫描 + 全屏重绘可让单次循环超过 20 ms。
- **建议**：把 `delay_ms(20)` 改成“按帧就绪/定时器节拍”调度；或把测量放到 DMA TC 中断里做轻量统计。

### R11（中）软件触发：无预触发、无硬件沿、无 holdoff

- **机理**：触发点是“采完一帧后在 SRAM 里找”，因此屏幕最左=触发点，**看不到触发前的波形**；触发分辨率=1 个采样点（200 kSa/s 时 5 µs，500 kSa/s 时 2 µs）；没有硬件模拟比较器/迟滞，噪声大时可能误触发（代码用 `span/8` 迟滞缓解，`adc.c:149-153`）。
- **建议**：答辩时把它定位为“数字触发（post-trigger）”，不要宣称“台式示波器级触发”；若要预触发，可用半缓冲：触发搜索在 HT 半区完成后从上一半区取前史。

### R12（中）看门狗恢复会重置采样率

- **机理**：`ADC1_Init()` 第一行把 `s_sample_rate=200000`（`adc.c:227`），而 `cmd.c` 的 `g_tb_us` 不会同步更新；恢复后时间轴与实际采样率不一致，直到下一次自动量程/TB 命令。
- **建议**：看门狗恢复后调用 `ADC1_SetSampleRate(恢复前的 fs)`，或让 `ADC1_Init()` 不重置 `s_sample_rate`。

### R13（低-中）改采样率会短暂停采

- **机理**：`ADC1_SetSampleRate()` 先停 ADC/DMA/TIM（`adc.c:350-353`），改 ARR、清标志、重开（`:374-385`），再 `delay_us(10)` 等 ADON 稳定（DS 的 tSTAB 2–3 µs，【DS Table 67 p.136】）。期间会丢 1–2 帧；`s_frame_ready=0`（`:366`）保证不读脏帧。
- **建议**：自动量程已用“10% 变化才切”的去抖（`cmd.c:926-930`），可再加“切换后丢弃第一帧”。

### R14（低）3.3 V 满量程假设与 /4096 约定

- **机理**：`measure.c` 用 `3.3/4096`（`measure.c:457-461`）；`cmd.c` 用 `*3300/4096`（`cmd.c:1243-1244`）。12 位满码是 4095，严格应按 `VDDA/4095`；差异 0.024%，可忽略，但 VDDA 若不是精确 3.3 V（LDO 误差、负载），Vpp/DC 会有 1% 级系统误差。
- **建议**：答辩时说明“未做两点校准，幅度精度受 VDDA 与量化限制”；有条件用万用表测 VDDA 后修正系数。

### R15（低）`DELAY=5` 在规则同时模式不是“对齐参数”

- **依据**：`ADC_CCR.DELAY` 在手册中用于交错模式的两次转换间隔（【RM §13.9.3 p.409】）；规则同时模式由同一触发同步启动（【RM §13.9.2 p.407】）。
- **建议**：保留即可，但答辩时不要把它说成“双 ADC 采样延迟 5 周期导致相位差”。

### R16（低）双 ADC 不得采同一通道

- **依据**：【RM §13.9.2 p.407】注：“Do not convert the same channel on the two/three ADCs”。
- **现状**：CH4/CH5 不同，满足；若改成同一引脚（如两路都接 PA5），手册明确禁止，必须改单 ADC 扫描或改引脚。

### R17（低）DMA2 Stream0 只开了 HT/TC，没开 TE/DME/FE

- **机理**：若发生传输错误/直接模式错误/FIFO 错误，流会停且无中断；`ADC1_Init()` 的看门狗能兜底（清所有标志 + 重建），但定位困难。
- **建议**：打开 TE 中断并计数，或在调试版里打印 `DMA2->LISR`。

### R18（低）双 ADC 的 OVR 是“每 ADC 一份”，代码处理正确但顺序有瑕疵

- **事实**：`ADC_SR` 的 OVR 是每个 ADC 各自的（【RM §13.13.1 p.417】；`ADC_CSR` 里还有 OVR1/2/3 副本，【RM p.428-429】）。代码分别清 ADC1/ADC2（`adc.c:51-58`），正确；但清 OVR 发生在判断“有没有新帧”之前，若 OVR 与 HT/TC 同时发生，可能读到“半新半旧”的一帧（低概率）。
- **建议**：把 OVR 处理移到 `s_frame_ready` 分支内，并在清 OVR 后丢弃当前帧。

### R19（低）PA4 的 DAC 关闭顺序依赖“先写后关时钟”

- **事实**：`RCC_APB1PeriphClockCmd(DAC, ENABLE)`（`adc.c:237`）→ 写 `DAC->CR/DHR`（`:243-245`）→ 关 DAC 时钟（`:248`）。顺序正确；若反过来（先关时钟再写），写操作会被忽略，DAC 缓冲可能仍开。
- **建议**：保持现状，并在注释里写明顺序原因。

### R20（低）LCD 读时序很慢，但只影响初始化

- 读一个像素 446 ns（75 HCLK），读 ID/状态几次，耗时可忽略；不要把它当成运行期瓶颈。

---

## 11. 手册依据与待核对项

### 11.1 手册依据（逐条可查）

> 格式：【文档 §章节 p.页码】。英文 RM0090 Rev 22 与本地 `.tmp_rm0090.pdf` **SHA256 相同**，页码可直接对照；中文第 4 版页码另列。

| # | 结论 | 手册依据 | 本地/URL |
|---|---|---|---|
| M1 | 时钟树与最大频率：AHB 168、APB2 84、APB1 42 | 【RM §7.2 p.218】 | `.tmp_rm0090.pdf` / dm00031020.pdf |
| M2 | APB 预分频≠1 时定时器时钟 ×2 | 【RM §7.2 p.218】 | 同上 |
| M3 | PLL 配置公式（VCO/N/P） | 【RM §7.2.3 p.220；§7.3.2 p.227】 | 同上 |
| M4 | Flash 168 MHz 需 5 等待周期 + ART | 【RM §3.5.1 p.80-81，Table 11 p.81】 | 同上 |
| M5 | 总线矩阵 8 主设备、DMA 存储/外设口、AHB/APB 桥 | 【RM §2.1 p.59；§2.1.4 p.63；§2.1.11 p.64】 | 同上 |
| M6 | ADC 时钟 = PCLK2/2/4/6/8 | 【RM §13.3.2 p.392；§13.13.16 p.430】 | 同上 |
| M7 | ADCCLK 上限 36 MHz（VDDA 2.4–3.6 V） | 【DS Table 67 p.135】 | stm32f407vg.pdf |
| M8 | Tconv = 采样 + 12 个 ADCCLK | 【RM §13.5 p.399】 | 同上 |
| M9 | 转换时间范围 9–492 周期、tSTAB 2–3 µs、tlatr 0.067 µs@30 MHz | 【DS Table 67 p.135-136】 | 同上 |
| M10 | 多 ADC 模式：主机触发、从机必须禁用触发 | 【RM §13.9 p.403】 | 同上 |
| M11 | 双重规则同时：触发来自 ADC1，同时触发 ADC2/3；不得同通道 | 【RM §13.9.2 p.407】 | 同上 |
| M12 | DMA 模式 2：ADC2 高半字、ADC1 低半字，成对传输 | 【RM §13.9 p.405】 | 同上 |
| M13 | ADC_CDR：ADC2[31:16]、ADC1[15:0]，偏移+0x300+0x08 | 【RM §13.13.17 p.432】 | 同上 |
| M14 | ADC_CCR：ADCPRE、DMA[1:0]、DDS、DELAY | 【RM §13.13.16 p.429-430】 | 同上 |
| M15 | ADC_CR2：EXTSEL=1000 为 T3_TRGO；DDS 位 9 | 【RM §13.13.3 p.420-421】 | 同上 |
| M16 | ADC_SR：OVR 置位条件（DMA=1 或 EOCS=1） | 【RM §13.13.1 p.417】 | 同上 |
| M17 | DMA 模式 OVR 后请求被阻塞；恢复需重配 DMA+清 OVR | 【RM §13.8.1 p.402】 | 同上 |
| M18 | DDS=0 末次传输后停发；DDS=1 继续发 | 【RM §13.13.16 p.430；§13.8.1 p.402】 | 同上 |
| M19 | DMA 循环模式自动重装 NDTR | 【RM §10.3.8 p.315】 | 同上 |
| M20 | DMA1/DMA2 请求映射（USART2_TX=DMA1 S6C4；ADC1=DMA2 S0C0） | 【RM Table 43 p.309；Table 44 p.310】 | 同上 |
| M21 | NVIC 16 级优先级（4 位） | 【RM §12.1 p.373】 | 同上 |
| M22 | 优先级分组/抢占与子优先级/PRIGROUP | 【PM §2.3.5-2.3.6 p.41；§4.3.7 p.215】 | dm00046982.pdf |
| M23 | SysTick 24 位递减、HCLK/8 可选 | 【PM §4.5 p.246】 | 同上 |
| M24 | TIM3/TIM4 为 16 位、TIM2/TIM5 为 32 位 | 【RM §18.1 p.586】 | `.tmp_rm0090.pdf` |
| M25 | TIMx_CR2/SMCR 中 MMS 描述（TRGO=更新事件） | 【RM §18.4.3 p.626（MMS 归属见 N1）】 | 同上 |
| M26 | FSMC Bank1 四个子区、HADDR[27:26] 选择、16 位地址映射 | 【RM §36.4/§36.4.1 p.1535-1536，Table 216/217】 | 同上 |
| M27 | FSMC 模式 A 与 BTR 的 ADDSET/DATAST 语义 | 【RM p.1544；Table 229 p.1546；§36.5.6 p.1565】 | 同上 |
| M28 | USART 波特率/ USARTDIV 编码 | 【RM §30.3.4 p.973】 | 同上 |
| M29 | USART TX DMA 使能位 DMAT（CR3 bit7） | 【RM p.1010】 | 同上 |
| M30 | DAC 使能后 PA4/PA5 自动接 DAC 输出（需先配 AIN） | 【RM p.436；§14.3.1 p.437；DAC_CR BOFF1 p.449】 | 同上 |
| M31 | PA4 = ADC12_IN4/DAC_OUT1/DCMI_HSYNC；PA5 = ADC12_IN5/DAC_OUT2 | 【DS2 Table 6 p.46】；【DS Table 7 p.52】 | 本地 Rev2 / 官网 Rev12 |
| M32 | ADC 输入电压范围 0–VDDA、RAIN 上限公式、精度 @30 MHz | 【DS2 §5.3.20 Table 65-66 p.118-120】；【DS Table 67-68 p.135-137】 | 两版 |

**中文手册（第 4 版）页码对照**（章节号与英文不同，供本地查书）：

| 内容 | 中文第 4 版 |
|---|---|
| 总线矩阵/系统架构 | 第 2 章，§2.1，p.49 |
| DMA 循环模式 | 第 9 章，p.210 |
| NVIC/中断 | 第 10 章，p.233 |
| ADC 时钟 | 第 11 章，§11.3.2，p.250 |
| 总转换时间 | §11.5，p.256 |
| OVR/DMA 数据管理 | §11.8.2，p.259 |
| 多重 ADC / DMA 模式 2 | §11.9，p.259-261 |
| ADC_CCR | §11.13.16，p.284 |
| TIM2-5 CR2/SMCR（MMS） | 第 15 章，§15.4.2-15.4.3，p.425-426 |
| FSMC Bank/时序 | 第 32 章，p.1191、p.1202 |

### 11.2 待核对项（答辩必须诚实说明）

| # | 待核对内容 | 为什么不确定 | 建议核实方式 |
|---|---|---|---|
| N1 | **MMS 到底在 `TIMx_CR2` 还是 `TIMx_SMCR`**：RM0090 §18.4.3（p.626）把 `MMS[2:0]` 列在 **SMCR** 标题下，而 CMSIS 头文件 `TIM_CR2_MMS=0x0070`、SPL 的 `TIM_SelectOutputTrigger()` 实际写 **CR2**（`FWLIB\src\stm32f4xx_tim.c:2941-2944`） | 手册表述与头文件/库实现冲突（中文第 4 版 §15.4.3 p.426 同样如此） | 以 `TIM3->CR2` 实测（读回寄存器）为准；写报告时注明手册表述疑误 |
| N2 | `ADC_CCR.DELAY=5` 在规则同时模式下是否有任何时序影响 | 手册只在交错模式（§13.9.3 p.409）定义 DELAY；规则同时模式未提 | 交换 DELAY 值测两路相位/幅值是否变化 |
| N3 | 只清 OVR、不重配 DMA，能否在 Circular 模式下恢复 | 手册要求重配 DMA（§13.8.1 p.402），但 Circular 的 NDTR 自动重装可能使其非必需 | 人为制造 OVR，观察是否自恢复（或看门狗恢复时间） |
| N4 | 写 `TIM3->ARR=83999` 的实际行为（16 位截断？） | ARR 是 16 位寄存器（RM §18.1 p.586），但手册未写“写超范围值”的行为 | 写后读回 `TIM3->ARR`，并用示波器测 TRGO 频率 |
| N5 | FSMC BTR 的 ADDSET/DATAST 是否“值即周期数”还是“值+1” | Table 229（p.1546）写“ADDSET HCLK cycles / DATAST HCLK cycles”，但部分时序图出现 `DATAST+1`（p.1543） | 用逻辑分析仪测 NWE/NOE 脉宽，或查 ST 官方 FSMC 时序应用笔记 |
| N6 | 本地中文 RM0090 为 2013 年第 4 版，部分内容可能早于 Rev 22（如某些寄存器位定义） | 版本差异 | 关键位以英文 Rev 22 为准；中文版仅作章节对照 |
| N7 | 本地 DS8626 为 Rev 2（2012），与 Rev 12 的引脚命名/页码不同 | 版本差异 | 引脚名以 Rev 12（DAC_OUT1/2）为准，页码以本地 Rev 2 标注 |
| N8 | 板级 PA4（STM_DAC）的 RC/测试点参数，以及是否插摄像头 | 属于开发板原理图，不在芯片手册 | 查开发板原理图/项目文档 `dual-channel-pin.md`，并实测 PA4 直流电平 |
| N9 | 实际 LCD 型号（9341/7789/5510…）与触摸类型（电阻/电容） | 代码按 `lcddev.id` 分支，未在报告中假定具体型号 | 读 `lcddev.id` 或看屏幕排线 |
| N10 | CPU 负载/帧延迟的精确值 | 本报告为估算，未用 DWT->CYCCNT 实测 | 在 `UI_Task()` 首尾读 DWT 周期计数器统计 |
| N11 | ADC 输入源阻抗与 15 cycles 采样是否匹配（RAIN 上限） | DS 只给公式，未给板级源阻抗 | 用高阻源+万用表/示波器对比，或换更长采样时间验证 |
| N12 | `ADC1_Init()` 由看门狗调用时对 UI 帧/时基的副作用 | 单线程无并发，但会重置 `s_sample_rate`（`adc.c:227`） | 人为触发看门狗，观察时基是否跳变 |

---

## 12. 答辩口径（30 秒 / 2 分钟）与最终结论

### 12.1 30 秒版

> “这套代码的核心是**用硬件保证采样时刻**：HSE 8 MHz 经 PLL 到 168 MHz，APB1 定时器 84 MHz 驱动 TIM3，ARR=419 得到 200 kHz 更新事件作为 TRGO；ADC1/ADC2 配成双重规则同时模式，同一个 TRGO 同时采 PA4 和 PA5，结果由硬件打包进 `ADC->CDR`，DMA2 Stream0 循环搬到 SRAM 的双半区（2×2048 对）。CPU 只在半传输/完成中断翻页，然后做中值滤波、软件触发对齐、FFT/过零测量、FSMC 刷屏和 USART2+DMA 上传。所以 CPU 再忙，采样间隔仍由 TIM3 和 21 MHz ADCCLK 决定。”

### 12.2 2 分钟版（按“机制→指标→风险”）

1. **时钟**：8 MHz→PLL(M8/N336/P2)=168 MHz；HCLK=168、PCLK1=42、PCLK2=84；APB1 定时器倍频→TIM3=84 MHz；ADC 预分频/4→ADCCLK=21 MHz（上限 36 MHz）。
2. **触发**：TIM3 PSC=0、ARR=419 → 84 MHz/420 = 200 kHz；MMS=010 → TRGO=更新事件；ADC1 EXTSEL=1000 选 T3_TRGO，ADC2 禁用自身触发，只接收主机的“同时触发”——这正是 RM0090 §13.9/§13.9.2 的要求。
3. **转换**：12 位 SAR，Tconv=(15+12)/21 MHz=1.286 µs（200 k 周期 5 µs，余量 3.9×；500 k 余量 1.56×）；低采样率自动换 84/480 cycles，兼顾高阻源。
4. **搬运**：DMA 模式 2 把 ADC2 放高半字、ADC1 放低半字，DMA2 Stream0 32 位循环搬 4096 字=2 帧；DDS=1 保证末次传输后继续发请求（否则只采一轮就停）；OVR 在 DMA 模式下会阻塞请求，代码轮询清零 + 看门狗兜底。
5. **指标**：200 kSa/s 帧时间 10.24 ms、DMA 800 kB/s；500 kSa/s 4.096 ms、2 MB/s；Nyquist 100/250 kHz（代码测频上限 60 kHz）；USART2 460800 8N1=46 kB/s；FSMC 写像素约 101 ns、全屏约 7.8 ms；RAM 74 KB/ROM 112 KB。
6. **风险**：TIM3 是 16 位，fs<1282 Hz 会截断（必须修）；OVR 恢复流程与手册不完全一致；PA4 是 DAC/DCMI/USART2_CK 复用脚，板上有 DAC 网络；无抗混叠滤波；UI 阻塞式 20 ms 刷新；1 KB 栈偏紧。

### 12.3 最终结论

- **数据通路**：`TIM3(TRGO) → ADC1/ADC2(双重规则同时) → ADC_CDR(32 位打包) → DMA2 Stream0(循环 4096 字) → SRAM1(双半区) → CPU(解包/中值/触发/测量) → FSMC(LCD) / USART2+DMA(上位机)`。
- **瓶颈排序**：① CPU 侧测量+LCD 绘制（阻塞式，估计 25–75% CPU）；② ADC 转换时间对最高采样率的限制（21 MHz 下 15 cycles 上限 777.8 kSa/s）；③ FSMC 写显存带宽（全屏 ~7.8 ms）；④ UART 吞吐（46 kB/s，波形流约 18 Hz 上限）。**采样链路本身不是瓶颈**——它是纯硬件、确定性节拍。
- **最该修的三件事**：R1（TIM3 16 位下限）、R2（OVR 恢复流程）、R6/R12（PA4 板级妥协与看门狗重置采样率的告知/修正）。

---

## 附录 A：关键计算复核（可复现）

```text
# 时钟
fVCO = 8e6/8*336 = 336e6
SYSCLK = 336e6/2 = 168e6
TIM3CLK = 2*(168e6/4) = 84e6
ADCCLK = 84e6/4 = 21e6

# TIM3
f = 84e6/((0+1)*(419+1)) = 200000
ARR(fs) = round(84e6/fs)-1
min fs (16-bit) = 84e6/65536 = 1281.74

# ADC
Tconv(N) = (N+12)/21e6
N=15 -> 1.2857e-6 ; N=84 -> 4.5714e-6 ; N=480 -> 23.4286e-6
fmax(N) = 21e6/(N+12) -> 777.8k / 218.75k / 42.68k

# 帧 / DMA
Tframe = 2048/fs -> 10.24ms@200k, 4.096ms@500k
DMA_BW = fs*4 -> 800kB/s@200k, 2MB/s@500k

# USART
USARTDIV = 42e6/(16*460800) = 5.6966 -> BRR 5+11/16
baud_actual = 42e6/(16*5.6875) = 461538.5 (+0.16%)
B/s = 460800/10 = 46080

# FSMC
Twr = (9+8)/168e6 = 101.2ns -> 9.88Mpx/s -> full 320*240*101.2ns = 7.77ms
Trd = (15+60)/168e6 = 446.4ns
```

## 附录 B：只读核对清单（本报告实际读过并核对行号的文件）

| 文件 | 用途 | 关键行 |
|---|---|---|
| `USER\main.c` | 启动顺序/优先级分组/波特率 | 11,12,14,16-18,20-24 |
| `USER\adc.c` | 采集核全部机制 | 11-33,43-90,102-223,225-333,335-388 |
| `USER\adc.h` | 缓冲/帧长度 | 10-11 |
| `USER\cmd.c` | 时基/自动量程/触发/串口流/看门狗 | 152-168,253-352,601-965,1014-1250 |
| `USER\measure.c` | FFT/过零/相位/THD/类型 | 8,15,35-73,191-289,406-674 |
| `USER\ui.c` | 绘制/签名/触摸/UI 节拍 | 48-62,143-148,782-1130,1403-1435,2159-2265 |
| `USER\usart.c` | USART2+DMA+环形 RX | 52-57,158-228,238-309 |
| `SYSTEM\delay\delay.c` | SysTick 延时 | 115-133,179-224 |
| `USER\system_stm32f4xx.c` | PLL/总线/Flash | 316,326,329,331,371,564-657 |
| `USER\stm32f4xx.h` | HSE 值 | 123 |
| `USER\stm32f4xx_it.c` | SysTick 空中断/异常 | 63-145 |
| `HARDWARE\LCD\lcd.c` | FSMC 初始化/时序/画点/填充 | 43-54,167-216,373-422,582-690,2407-2517 |
| `HARDWARE\LCD\lcd.h` | LCD 基址/A6/背光 | 53,55-63 |
| `HARDWARE\TOUCH\touch.c` | 触摸初始化/校准存储 | 24-37,44-82,225-231,504-566 |
| `HARDWARE\TOUCH\ctiic.c` | 电容屏软件 IIC 引脚 | PB0/PF11 |
| `HARDWARE\IIC\myiic.c` | EEPROM 软件 IIC | 17-32 |
| `HARDWARE\24CXX\24cxx.c/.h` | 24C02 读写 | 24cxx.h:27 |
| `CORE\startup_stm32f40_41xxx.s` | 栈/堆/SystemInit 调用 | 41,52,182 |
| `OBJ\Base.map` | ROM/RAM 实测 | Grand Totals |

---

*报告完。本报告所有页码均来自实际 PDF 文本层核对；凡未能唯一确认者已在 §11.2 标“待核对”，未编造页码。*
