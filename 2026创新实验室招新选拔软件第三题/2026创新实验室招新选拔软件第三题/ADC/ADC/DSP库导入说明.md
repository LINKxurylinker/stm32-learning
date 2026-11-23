# DSP 库导入说明（Base 工程）

- 日期：2026-11-19
- 工程：`USER\Base.uvprojx`（Keil MDK5，ARMCC V5.06 update 5）
- 库版本：`STM32F4xx_DSP_Support` 里的 CMSIS-DSP **1.4.4**，`arm_cortexM4lf_math.lib`（小端 + 单精度 FPU）

## 一、改了哪几处

| 位置 | 改动 |
|---|---|
| Options → C/C++ → Define | 在 `STM32F40_41xxx,USE_STDPERIPH_DRIVER,__FPU_PRESENT=1` 后面加 `ARM_MATH_CM4` |
| Options → C/C++ → Include Paths | 原来的 `.. \STM32F4xx_DSP_Support` 多了一个空格（路径无效），已改成 `..\STM32F4xx_DSP_Support` |
| Project 面板 | 新增 **DSP** 组，里面有 `arm_cortexM4lf_math.lib`（库）和 `arm_math.h`、`arm_common_tables.h`、`arm_const_structs.h`（头文件） |
| Options → Target | Floating Point Hardware 原本就是 **Single Precision**（FPU 已开），不用动 |

> 注意：如果你现在 Keil 里正开着这个工程，先 **Project → Close Project** 再重新打开（或重启 Keil），Project 面板里才能看到新增的 DSP 组。重开之前不要在 Options 里点 OK，否则可能用内存里的旧设置覆盖 `Base.uvprojx`。

## 二、验证结果

- 用 Keil 命令行 `UV4 -r`（全量重编译）：**0 Error(s), 0 Warning(s)**。
- 临时写了一段 `arm_cfft_f32()` + `arm_cfft_sR_f32_len64` 的调用做冒烟测试，链接后 map 文件里能看到：
  - `main.o(.text) refers to arm_cfft_f32.o(.text) for arm_cfft_f32`
  - `main.o(.text) refers to arm_const_structs.o(.constdata) for arm_cfft_sR_f32_len64`
  
  说明库里的符号能正常解析、FPU 版本匹配；测试代码已删除，当前工程仍然是 0 Error / 0 Warning。
- 工程静态 RAM：`RW-data 320 B + ZI-data 75688 B`（约 74 KB；其中双通道 ADC DMA 缓冲 16 KB，`measure.c` 的 2048 点复数 FFT 缓冲 16 KB）。
- `measure.c` 已经实际使用 **2048 点 `arm_cfft_f32()` + Hann 窗 + 峰值抛物线插值** 测频，并与过零点测频结果融合；DSP 库不是只“导入未用”。

## 三、怎么用

```c
#include "arm_math.h"
#include "arm_const_structs.h"   /* 提供 arm_cfft_sR_f32_len1024 等常量 */

#define FFT_N 1024
static float32_t fft_buf[FFT_N * 2];     /* 实部/虚部交替，共 2N 个 float */

void fft_example(const u16 *adc, float32_t vref_lsb)
{
    const arm_cfft_instance_f32 *S = &arm_cfft_sR_f32_len1024;
    u16 i;

    for (i = 0; i < FFT_N; i++)
    {
        fft_buf[2 * i]     = (float32_t)adc[i] * vref_lsb;  /* 实部 = 采样值 */
        fft_buf[2 * i + 1] = 0.0f;                          /* 虚部 = 0     */
    }

    arm_cfft_f32(S, fft_buf, 0, 1);   /* ifftFlag=0 正变换，bitReverseFlag=1 */

    /* 第 k 个 bin：re = fft_buf[2k]，im = fft_buf[2k+1]
     * 幅值（k != 0）：A = 2 * sqrt(re*re + im*im) / FFT_N
     * 直流（k == 0）：A = sqrt(re*re + im*im) / FFT_N                      */
}
```

要点：

- 本工程是 CMSIS-DSP **1.4.4**，**没有** `arm_cfft_init_f32()`；FFT 实例直接用 `arm_const_structs.h` 里的常量 `arm_cfft_sR_f32_len16/32/64/.../4096`。
- `arm_cfft_f32()` 是**原地**运算，输入输出共用 `fft_buf`；输入是复数数组（实部/虚部交替），不是实数数组。
- 如果不想手动补零当复数，也可以用实数 FFT：`arm_rfft_fast_instance_f32` + `arm_rfft_fast_init_f32()` + `arm_rfft_fast_f32()`，输出 `[0]=DC实部, [1]=DC虚部(0), [2]=bin1实部, ...`。
- 频率分辨率 `Δf = Fs / N`；1 kHz、Fs=100 kHz、N=1024 时 1 kHz 落在第 10.24 个 bin，不落在整数 bin，需要加窗/插值或用过零法测频。
- 常见报错：
  - `undefined symbol arm_cfft_f32`：Project 里没加 `.lib`（或加成了非 `lf` 版）。
  - `arm_math.h not found`：Include Paths 没指到 `STM32F4xx_DSP_Support` 这一层。
  - 编译过但结果全 0：FPU 没勾（本工程已勾 Single Precision）、点数与实例长度不一致，或忘了除 `N/2`。
- `lf` = little endian + FPU；`arm_cortexM4lf_math.lib` 必须配 **Single Precision** 的 FPU 选项，两边要一致。
