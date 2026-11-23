# Base 工程修复补丁：显示闪变 + 三角波识别

> **⚠️ 已过时（2026-11-19 19:38）**：这个目录是 19:00 的补丁快照。之后工程又加了 CMSIS FFT 测频、clipping 检测和波形类型状态机；当前代码已经直接更新到 `ADC\ADC\USER`，**不要再按本目录覆盖**。最新状态机说明见 `..\waveform-fsm.md`。
>
> 下面内容保留作“当时改了什么”的记录。

> 快照时间：2026-11-19 19:00
> 对应源码：`adc.c`（18:50）、`cmd.c`（18:59）、`ui.c`（19:00）、`measure.c`（16:24）
> 构建验证：Keil ARMCC V5.06，**0 Error(s), 0 Warning(s)**（见 `build.log`）
> 注意：这是基于上面时间点的一份副本；如果你在 19:00 之后又改了 `cmd.c` / `ui.c`，请按第 4 节做合并，不要直接覆盖。

---

## 1. 闪变的根因

### 1.1 自动触发只认“单步跨带”

`adc.c` 的 `ADC1_FindTrigger()` 自动档原来是：

```c
low  = min + span/4;
high = max - span/4;
if (s[i-1] <= low && s[i] >= high)   /* 要求相邻两点跨过整个 1/4~3/4 带 */
```

- 方波边沿经过 3 点中值滤波或板上 RC 后，可能变成 `low -> mid -> high` 两个采样点；没有任何一对点能一步从 `<=low` 跳到 `>=high`，函数返回 0。
- 正弦每点只变化几十 LSB，更不可能一步跨过半个 Vpp。
- 触发找不到时，`CMD_Task()` 在 AUTO 档仍然接受这一帧，`s_disp_trig = 0`；下一帧又找到了触发（`s_disp_trig = 边沿`）。UI 从 0 开始画和从边沿开始画交替，看起来就是“半个周期的上升/下降沿打架”，也就是闪变。

### 1.2 自动量程用了未对齐帧 + 错误频率

`cmd.c` 的 `cmd_auto_range()` 原来测的是 `ADC1_Frame()`（原始 DMA 半区），不是屏幕上已经触发对齐的 `CMD_DisplayFrame()`；而且 `measure.c` 的频率算法有两个问题：

- 过零判据同样要求“单点跨过 `mid±10%` 带”，1 kHz 正弦得到 `cross=0`，`F=0`；
- 就算方波能跨过去，`f = (cross-1)/(2*duration)` 多除了 2，1 kHz 方波会算成 500 Hz。

于是自动量程里的 `cycles = m.freq * win_us / 1e6` 是错的：方波显示 4~5 个周期，正弦直接 `m.freq<0.5` 返回、量程不动；一旦 fs/span 被改，显示就会缩放/跳动。

---

## 2. 补丁内容

### 2.1 `adc.c`：自动触发改成“armed + 中值 + 滞回”

```c
u16 mid  = (min + max) / 2;
u16 hyst = span / 8;              /* 至少 4 LSB */
u8  armed = 0;

for (i = 1; i < ADC_FRAME_LEN; i++)
{
    if (s_frame[i-1] + hyst <= mid) armed = 1;
    if (armed && s_frame[i-1] < mid && s_frame[i] >= mid)
    {
        *idx = i;
        return 1;
    }
}
```

下降沿对称。这样无论边沿跨 1 个点还是 3 个点，都能稳定找到同一个过中点位置；`ui_trig_raw()` 在自动档画的中值线也正好对应这个 `mid`。

### 2.2 `cmd.c`：AUTO 找不到触发时先保持，不要立刻跳回 0

新增 `s_trig_miss`：

```c
if (found) s_trig_miss = 0;
else if (g_mode == CMD_MODE_AUTO)
{
    if (s_disp_found && s_trig_miss < 3U) accept = 0;  /* 保持上一帧 */
    else s_trig_miss++;
}
```

- 有信号时偶发漏触发：最多保持 3 帧（约 30 ms），不会半周期跳变，也不会让新波形等太久；
- 信号真的没了：3 帧后进入自由刷新，符合 AUTO 档“无信号也刷新基线”；
- NORM / SINGLE 逻辑不变。

> 识别本身是**逐帧独立**的：每来一帧就重新算 crest/THD/斜率 cv 并判型，没有“上一帧是什么类型”的状态，也没有按测试顺序查表。这里的 3 帧只用于“漏触发时暂时保持上一帧画面”，不是分类去抖；正常情况下新波形在下一帧（约 10 ms）就更新。

`cmd_auto_range()` 改成测 `CMD_DisplayFrame()`，并加 `CMD_DisplaySeq()/CMD_DisplayFound()` 保护：

```c
if (CMD_DisplaySeq() == 0U) return;
if (!CMD_DisplayFound()) return;
MEAS_Compute(CMD_DisplayFrame(), ADC_FRAME_LEN, (float)fs, &m);
```

这样自动量程只看屏幕上真实显示的那一帧；漏触发时保持原量程，不会追着噪声改档。

### 2.3 `measure.c`：重写频率 + 三角/正弦/方波判据

**频率**（修正原 bug）：

```c
level = (min + max) / 2;
hyst  = max(4, span / 16);
/* armed：先等 f[i-1]+hyst <= level，再找 f[i-1] < level && f[i] >= level */
freq = (count - 1) / (last_t - first_t);   /* 不再除 2 */
```

**三角波（最小二乘斜率一致性）**：

- 对每个上升沿，从 5% Vpp 处开始，取 20%/40%/60%/80% 四个点；
- 对 `20-40%`、`40-60%`、`60-80%` 三段分别做最小二乘拟合，得到 `s0/s1/s2`；
- 计算段间斜率变异系数 `cv = std(s0,s1,s2) / mean(s0,s1,s2)`；
- 至少 2 个边沿有效，且 `cv < 0.03`、`crest > 1.45` → 三角波。

> 为什么不是只拟合一条线看残差：1 kHz 正弦在 20~80% 区间也很接近直线（残差约 0.003 Vpp，三角约 0.0002 Vpp），单条线的 R² 区分度不够；拆成三段看“斜率是否不变”更稳。

**正弦波**：`crest ≈ 1.414` + `THD < 0.08`；THD 用 Goertzel 在 f0/3f0/5f0 算 `sqrt(h3²+h5²)/h1`。

**方波**：`crest < 1.20` 或 `THD > 0.28` 或“靠近 min/max 的采样点 > 60%”（保留原来的平顶判据）。

**DC**：`Vpp < 40 LSB`（约 32 mV）。

类型编号：`1=SINE, 2=SQUARE, 3=DC, 4=TRIANGLE`。

### 2.4 `measure.h` / `ui.c`

- `measure.h` 的类型注释加上 4=triangle；
- `ui.c` 的 `ui_type_short()` 加 `case 4: return CN_TRI;`；
- `ui_cn.h` 里已经有 `CN_TRI "\xC8\xFD\xBD\xC7"`（三角）和“三/角”字模，不需要改。

---

## 3. 验证结果

### 3.1 斜率一致性（200 kSa/s，2048 点，1 Vpp）

| 波形 | 1 kHz | 2 kHz | 5 kHz |
|---|---:|---:|---:|
| 正弦 cv | 0.043 | 0.047 | 0.041 |
| 三角 cv | 0.0004 | 0.0008 | 0.0015 |

0.2 Vpp 时：正弦 0.046~0.047，三角 0.004~0.010；1 LSB 噪声、1 Vpp 时：正弦 0.038~0.047，三角 0.002~0.005。阈值取 0.03，余量足够。

### 3.2 分类结果（Python 按同一判据仿真）

- 9 个验收组合（3 波形 × 1/2/5 kHz，Vpp=1 V，DC=1.5 V）：**9/9**；
- 0.2~2 Vpp × DC 1/1.5/2 V × 3 波形 × 3 频点：**135/135**；
- 再加 1 LSB RMS 噪声：**54/54**；
- 随机顺序、随机幅度/频率/相位（每次独立判型，不依赖上一帧）：**300/300**。

方波：crest≈1.000，THD≈0.39，edge≈1.00；正弦：crest≈1.414，THD≈0.00~0.02，cv≈0.04；三角：crest≈1.73，THD≈0.12，cv≈0.001。

### 3.3 编译

ARMCC V5.06，0 Error / 0 Warning，`Code=59484 RO-data=9228 RW-data=204 RW-data ZI=34980`。

---

## 4. 怎么应用

1. 在 Keil 里 **Save All → Close Project**（不要在工程打开时覆盖源文件）。
2. 备份你现在的 5 个文件：
   - `USER\adc.c`、`USER\cmd.c`、`USER\ui.c`、`USER\measure.c`、`USER\measure.h`
3. 把本目录 `USER\` 下的 5 个文件复制到工程的 `USER\` 覆盖。
4. 重新打开 `USER\Base.uvprojx`，Build；预期 0 Error / 0 Warning。
5. 上板先测 1 kHz 方波：屏幕应稳定、不闪；再测 1/2/5 kHz × 正弦/方波/三角。
6. 串口 `MEAS?` 的 `TYPE=` 应与屏幕类型卡一致（SINE/SQUARE/TRIANGLE）。

> 如果 19:00 之后你又改过 `cmd.c` / `ui.c`，不要直接覆盖：`measure.c` 是独立重写，`adc.c` 只改 `ADC1_FindTrigger()` 自动档，`cmd.c` 是“s_trig_miss + 自动量程测显示帧”，`ui.c` 只加 `case 4`——按这三处合并即可。

---

## 5. 还要注意

- 这次没有动 `ui_cn.h`、`adc.h`、`cmd.h` 的接口；
- `cmd.c` 里的自动量程 debounce/hysteresis 是你 18:59 的新版本，补丁保留，只改了“测哪一帧”和“漏触发保护”；
- 三角判据用的是“段间斜率变异系数”，不是单条线残差；如果现场三角波被信号源圆角化很严重，`cv` 会升高，可把 `cv < 0.030f` 放宽到 0.04，或保留 `crest > 1.55f` 的兜底；
- 方波占空比不是 50% 时，`crest` 会偏离 1.0，但 THD/平顶判据仍能兜住。
