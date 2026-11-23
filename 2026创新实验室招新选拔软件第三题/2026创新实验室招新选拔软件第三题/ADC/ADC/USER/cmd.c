#include "cmd.h"
#include "adc.h"
#include "measure.h"
#include "usart.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Time base and vertical gain tables used by the touch UI.
 * TB is the time per horizontal division; the whole frame is 10 div. */
#define CMD_TB_COUNT   7U
#define CMD_VD_COUNT   6U

static const u32 s_tb_list[CMD_TB_COUNT] = {250U, 500U, 1000U, 2000U, 5000U, 10000U, 20000U};
static const u16 s_vd_list[CMD_VD_COUNT] = {200U, 250U, 500U, 1000U, 2000U, 5000U};

/* Nice voltage steps.  A 1-2-5 list alone is too coarse for the 60-80%
 * screen-fill requirement: for example 3 Vpp rounds to 1 V/div and only
 * fills 37%.  The intermediate steps keep the fill near 70% while the
 * axis labels stay on regular decimal values. */
static u16 cmd_nice_vdiv(u16 raw)
{
    static const u16 list[25] = {
        20U, 25U, 30U, 40U, 50U, 60U, 80U, 100U, 125U, 150U,
        200U, 250U, 300U, 400U, 500U, 600U, 800U, 1000U, 1250U,
        1500U, 2000U, 2500U, 3000U, 4000U, 5000U
    };
    u8 i;
    u8 best = 0U;
    u8 have_band = 0U;
    u16 diff = 0xFFFFU;
    float best_score = 0.0f;

    /* First choice: any step that puts the waveform inside 60-80%.
     * raw is the V/div that would give exactly 70%, so pick the step
     * whose resulting fill is closest to 70%. */
    for (i = 0U; i < 25U; i++)
    {
        float fill = (float)raw * 0.7f / (float)list[i];

        if (fill >= 0.60f && fill <= 0.80f)
        {
            float score = (fill >= 0.70f) ? (fill - 0.70f) : (0.70f - fill);
            if (!have_band || score < best_score)
            {
                have_band = 1U;
                best_score = score;
                best = i;
            }
        }
    }
    if (have_band) return list[best];

    /* Physical limits (below 0.16 Vpp or above 8 Vpp): use nearest. */
    for (i = 0U; i < 25U; i++)
    {
        u16 d = (list[i] > raw) ? (u16)(list[i] - raw)
                                : (u16)(raw - list[i]);
        if (d < diff)
        {
            diff = d;
            best = i;
        }
    }
    return list[best];
}

static u32 cmd_nice_tb(u32 raw)
{
    static const u32 list[16] = {
        1U, 2U, 5U, 10U, 20U, 50U, 100U, 200U,
        250U, 500U, 1000U, 2000U, 2500U, 5000U, 10000U, 20000U
    };
    u8 i;

    if (raw < list[0]) return list[0];
    for (i = 0U; i < 15U; i++)
    {
        if (raw <= list[i]) return list[i];
    }
    return list[15];
}

static u8  g_run = 1;
static u8  g_mode = CMD_MODE_AUTO;
static u8  g_single_armed = 0;
static u8  g_edge_rise = 1;
static u8  g_trig_manual = 0;
static u8  g_auto_tb = 1;
static u8  g_auto_vdiv = 1;
static u8  g_auto_paused = 0;
static u8  g_ch_en[2] = {1U, 1U};
static u16 g_trig_level = 2048;
static u32 g_tb_us = 1000;
static u32 g_capture_tb_us = 1000;
static u16 g_disp_span = ADC_FRAME_LEN;
/* Snapshot used to draw the frame currently on screen.  The live TB / span
 * can change while the auto range is settling; applying a new timebase to
 * an old frame is what creates transient edge glitches. */
static u32 g_disp_tb_us = 1000;
static u16 g_disp_span_snap = ADC_FRAME_LEN;
static u16 g_vdiv_mv[2] = {250U, 250U};
static u16 g_vcenter_mv[2] = {1500U, 1500U};
/* Last frames accepted for the display.  Both channels are sampled by
 * the same trigger, and share one trigger index so the two traces keep
 * their real time relationship. */
static u16 s_disp_frame1[ADC_FRAME_LEN];
static u16 s_disp_frame2[ADC_FRAME_LEN];
static u16 s_disp_trig = 0;
static u8  s_disp_found = 0;
static u32 s_disp_seq = 0;
static u16 s_trig_miss = 0;
static u8  s_norm_hold = 0;      /* NORM with no trigger: really hold */
static u8  s_wave_type = 0;

static char g_cmd_line[201];
static u8   g_cmd_len = 0;
static u8   g_cmd_state = 0;      /* 0 idle, 1 data, 2 discard */

static char g_pending[192];
static u8   g_pending_flag = 0;
static u8   g_wave_pending = 0;

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

static void cmd_reply(const char *s)
{
    u16 len = (u16)strlen(s);

    if (len > 180U) len = 180U;

    if (!uart2_tx_busy() && !g_pending_flag && !g_wave_pending)
    {
        memcpy(g_uart2_tx_buf, s, len);
        uart2_send_dma(len);
    }
    else
    {
        memcpy(g_pending, s, len);
        g_pending[len] = 0;
        g_pending_flag = 1;
    }
}

static void cmd_err(void)
{
    cmd_reply("ERR\n");
}

static void cmd_set_timebase(u32 us)
{
    u32 fs;

    g_auto_tb = 0;

    if (us < 200U)     us = 200U;
    if (us > 100000U)  us = 100000U;

    fs = (ADC_FRAME_LEN * 1000000U) / (us * 10U);
    ADC1_SetSampleRate(fs);

    /* ADC1_SetSampleRate() may clamp fs; keep TB consistent with reality. */
    g_tb_us = (ADC_FRAME_LEN * 1000000U) / (ADC1_GetSampleRate() * 10U);
    g_capture_tb_us = g_tb_us;
    g_disp_span = ADC_FRAME_LEN;
}

static void cmd_set_vdiv(u16 mv)
{
    g_auto_vdiv = 0;
    if (mv < 100U)  mv = 100U;
    if (mv > 5000U) mv = 5000U;
    g_vdiv_mv[0] = mv;
    g_vdiv_mv[1] = mv;
    /* Keep the trace centred without jumping it to a whole V/div. */
    g_vcenter_mv[0] = (u16)(((g_vcenter_mv[0] + 5U) / 10U) * 10U);
    g_vcenter_mv[1] = (u16)(((g_vcenter_mv[1] + 5U) / 10U) * 10U);
    if (g_vcenter_mv[0] > 3300U) g_vcenter_mv[0] = 3300U;
    if (g_vcenter_mv[1] > 3300U) g_vcenter_mv[1] = 3300U;
}

static u8 cmd_nearest_tb_index(void)
{
    u8 i;
    u8 best = 0;
    u32 diff = 0xFFFFFFFFU;
    u32 d;

    for (i = 0; i < CMD_TB_COUNT; i++)
    {
        d = (g_tb_us > s_tb_list[i]) ? (g_tb_us - s_tb_list[i])
                                     : (s_tb_list[i] - g_tb_us);
        if (d < diff)
        {
            diff = d;
            best = i;
        }
    }
    return best;
}

static u8 cmd_nearest_vd_index(void)
{
    u8 i;
    u8 best = 0;
    u32 diff = 0xFFFFFFFFU;
    u32 d;

    for (i = 0; i < CMD_VD_COUNT; i++)
    {
        d = (g_vdiv_mv[0] > s_vd_list[i]) ? (g_vdiv_mv[0] - s_vd_list[i])
                                          : (s_vd_list[i] - g_vdiv_mv[0]);
        if (d < diff)
        {
            diff = d;
            best = i;
        }
    }
    return best;
}

static u8 cmd_parse_float(const char *s, double *v)
{
    char *end;

    if (s == 0 || *s == 0) return 0;
    *v = strtod(s, &end);
    if (end == s || *end != 0) return 0;
    if (*v != *v) return 0;             /* NaN */
    return 1;
}

static u8 cmd_parse_time(const char *s, u32 *us)
{
    char *end;
    double v = strtod(s, &end);

    if (end == s || v != v) return 0;

    if (*end == 0)          v *= 1.0;
    else if (strcmp(end, "US") == 0) v *= 1.0;
    else if (strcmp(end, "MS") == 0) v *= 1000.0;
    else if (strcmp(end, "S") == 0)  v *= 1000000.0;
    else return 0;

    if (v < 10.0 || v > 1000000.0) return 0;
    *us = (u32)(v + 0.5);
    return 1;
}

static void cmd_measure(void)
{
    MEAS_Result m1, m2;
    char resp[300];
    char phbuf[16];
    float fs = (float)ADC1_GetSampleRate();
    float phase = -1.0f;
    float f1, f2, fmax, df;

    MEAS_Compute(CMD_DisplayFrame(), ADC_FRAME_LEN, fs, &m1);
    MEAS_Compute(CMD_DisplayFrame2(), ADC_FRAME_LEN, fs, &m2);

    f1 = m1.freq;
    f2 = m2.freq;
    fmax = (f1 > f2) ? f1 : f2;
    df = (f1 > f2) ? (f1 - f2) : (f2 - f1);

    /* Phase only has a fixed value when both channels are on and their
     * fundamentals agree; otherwise report -- instead of a rotating
     * (meaningless) angle. */
    if (CMD_GetChannelEnabled(0) && CMD_GetChannelEnabled(1) &&
        m1.vpp >= 0.03f && m2.vpp >= 0.03f &&
        fmax >= 20.0f && df <= (fmax * 0.20f))
    {
        phase = MEAS_Phase(CMD_DisplayFrame(), CMD_DisplayFrame2(),
                           ADC_FRAME_LEN, fs, (f1 + f2) * 0.5f);
    }

    if (phase >= 0.0f) sprintf(phbuf, "%.1fdeg", (double)phase);
    else               strcpy(phbuf, "--");

    sprintf(resp,
            "MEAS:CH1,F=%.1fHz,VPP=%.3fV,DC=%.3fV,BIAS=%.3fV,T=%s;"
            "CH2,F=%.1fHz,VPP=%.3fV,DC=%.3fV,BIAS=%.3fV,T=%s;"
            "PH=%s\n",
            m1.freq, m1.vpp, m1.dc, m1.vdc_bias,
            MEAS_TypeName(CMD_GetWaveType()),
            m2.freq, m2.vpp, m2.dc, m2.vdc_bias, MEAS_TypeName(m2.type),
            phbuf);
    cmd_reply(resp);
}

/* THD at the 1/2/3/4/5 kHz test points, using exactly the same rounded
 * fundamental as the on-screen harmonic page so the numbers agree. */
static u8 cmd_thd_one(const u16 *f, float fs, float *thd)
{
    MEAS_Result m;
    float amp[5];
    u8 k;
    float err;

    MEAS_Compute(f, ADC_FRAME_LEN, fs, &m);
    if (m.freq < 500.0f || m.freq > 5500.0f) return 0U;

    k = (u8)(m.freq / 1000.0f + 0.5f);
    err = m.freq - (float)(k * 1000U);
    if (err < 0.0f) err = -err;
    if (k < 1U || k > 5U || err > (m.freq * 0.08f + 50.0f)) return 0U;

    return MEAS_Harmonics(f, ADC_FRAME_LEN, fs,
                          m.freq, 5U, amp, thd);
}

static void cmd_thd(void)
{
    char resp[80];
    u8 ok1, ok2;
    float t1 = 0.0f, t2 = 0.0f;
    float fs = (float)ADC1_GetSampleRate();

    ok1 = cmd_thd_one(CMD_DisplayFrame(),  fs, &t1);
    ok2 = cmd_thd_one(CMD_DisplayFrame2(), fs, &t2);

    sprintf(resp, "THD:CH1=%s", ok1 ? "" : "--");
    if (ok1) sprintf(resp + strlen(resp), "%.2f%%", (double)t1);
    sprintf(resp + strlen(resp), ",CH2=%s", ok2 ? "" : "--");
    if (ok2) sprintf(resp + strlen(resp), "%.2f%%", (double)t2);
    strcat(resp, "\n");
    cmd_reply(resp);
}

static u16 cmd_build_wave(void)
{
    const u16 *frame = ADC1_Frame();
    u16 k, idx, trig, pos = 0;
    u16 val;

    trig = ADC1_TriggerIndex(g_edge_rise, g_trig_manual, g_trig_level);
    pos = (u16)sprintf(g_uart2_tx_buf, "WAVE,512\n");

    for (k = 0; k < 512U; k++)
    {
        idx = (u16)(trig + (u16)(((u32)k * ADC_FRAME_LEN) / 512U));
        if (idx >= ADC_FRAME_LEN) idx = (u16)(idx - ADC_FRAME_LEN);
        val = frame[idx];
        pos += (u16)sprintf(&g_uart2_tx_buf[pos], (k == 511U) ? "%u\n" : "%u,", val);
    }

    return pos;
}

/* ------------------------------------------------------------------ */
/* command parser                                                     */
/* ------------------------------------------------------------------ */

static void cmd_process(char *line)
{
    char work[201];
    char resp[64];
    char *cmd;
    char *arg;
    u16 i;
    double v;
    u32 us;

    for (i = 0; line[i] && i < 200U; i++)
    {
        work[i] = (char)toupper((unsigned char)line[i]);
    }
    work[i] = 0;

    cmd = strtok(work, " ");
    if (cmd == 0) return;
    arg = strtok(0, " ");

    if (strcmp(cmd, "RUN") == 0 && arg == 0)
    {
        g_run = 1;
        if (g_mode == CMD_MODE_SINGLE) g_mode = CMD_MODE_AUTO;
        g_single_armed = 0;
        cmd_reply("OK:RUN\n");
    }
    else if (strcmp(cmd, "STOP") == 0 && arg == 0)
    {
        g_run = 0;
        g_single_armed = 0;
        cmd_reply("OK:STOP\n");
    }
    else if (strcmp(cmd, "SINGLE") == 0 && arg == 0)
    {
        g_mode = CMD_MODE_SINGLE;
        g_run = 1;
        g_single_armed = 1;
        cmd_reply("OK:SINGLE\n");
    }
    else if (strcmp(cmd, "MODE") == 0 && arg != 0)
    {
        if (strcmp(arg, "AUTO") == 0)
        {
            g_mode = CMD_MODE_AUTO;
            g_run = 1;
            g_single_armed = 0;
            cmd_reply("OK:MODE AUTO\n");
        }
        else if (strcmp(arg, "NORM") == 0 || strcmp(arg, "NORMAL") == 0)
        {
            g_mode = CMD_MODE_NORM;
            g_run = 1;
            g_single_armed = 0;
            cmd_reply("OK:MODE NORM\n");
        }
        else if (strcmp(arg, "SINGLE") == 0)
        {
            g_mode = CMD_MODE_SINGLE;
            g_run = 1;
            g_single_armed = 1;
            cmd_reply("OK:MODE SINGLE\n");
        }
        else
        {
            cmd_err();
        }
    }
    else if (strcmp(cmd, "TRIG") == 0 && arg != 0)
    {
        if (strcmp(arg, "AUTO") == 0)
        {
            g_trig_manual = 0;
            cmd_reply("OK:TRIG AUTO\n");
        }
        else
        {
            if (!cmd_parse_float(arg, &v) || v < 0.0 || v > 3.3)
            {
                cmd_err();
            }
            else
            {
                g_trig_level = (u16)(v * 4095.0 / 3.3 + 0.5);
                g_trig_manual = 1;
                cmd_reply("OK:TRIG\n");
            }
        }
    }
    else if (strcmp(cmd, "EDGE") == 0 && arg != 0)
    {
        if (strcmp(arg, "RISE") == 0)
        {
            g_edge_rise = 1;
            cmd_reply("OK:EDGE RISE\n");
        }
        else if (strcmp(arg, "FALL") == 0)
        {
            g_edge_rise = 0;
            cmd_reply("OK:EDGE FALL\n");
        }
        else
        {
            cmd_err();
        }
    }
    else if (strcmp(cmd, "TB") == 0 && arg != 0)
    {
        if (!cmd_parse_time(arg, &us))
        {
            cmd_err();
        }
        else if (us < 200U || us > 100000U)
        {
            cmd_err();
        }
        else
        {
            cmd_set_timebase(us);
            cmd_reply("OK:TB\n");
        }
    }
    else if (strcmp(cmd, "VDIV") == 0 && arg != 0)
    {
        if (!cmd_parse_float(arg, &v) || v < 0.1 || v > 5.0)
        {
            cmd_err();
        }
        else
        {
            cmd_set_vdiv((u16)(v * 1000.0 + 0.5));
            sprintf(resp, "OK:VDIV %umV\n", g_vdiv_mv[0]);
            cmd_reply(resp);
        }
    }
    else if ((strcmp(cmd, "CH1") == 0 || strcmp(cmd, "CH2") == 0) && arg == 0)
    {
        u8 ch = (u8)(cmd[2] - '1');
        CMD_ToggleChannel(ch);
        sprintf(resp, "OK:%s %s\n", (ch == 0) ? "CH1" : "CH2",
                CMD_GetChannelEnabled(ch) ? "ON" : "OFF");
        cmd_reply(resp);
    }
    else if (strcmp(cmd, "AUTO") == 0 && arg == 0)
    {
        g_auto_tb = 1;
        g_auto_vdiv = 1;
        g_auto_paused = 0;
        cmd_reply("OK:AUTO\n");
    }
    else if (strcmp(cmd, "MEAS?") == 0 && arg == 0)
    {
        cmd_measure();
    }
    else if (strcmp(cmd, "THD?") == 0 && arg == 0)
    {
        cmd_thd();
    }
    else if (strcmp(cmd, "TB?") == 0 && arg == 0)
    {
        sprintf(resp, "OK:TB %luus\n", (unsigned long)g_tb_us);
        cmd_reply(resp);
    }
    else if (strcmp(cmd, "VDIV?") == 0 && arg == 0)
    {
        sprintf(resp, "OK:VDIV %umV,%umV\n",
                g_vdiv_mv[0], g_vdiv_mv[1]);
        cmd_reply(resp);
    }
    else if (strcmp(cmd, "TRIG?") == 0 && arg == 0)
    {
        if (g_trig_manual)
        {
            sprintf(resp, "OK:TRIG %.2fV\n",
                    (double)g_trig_level * 3.3 / 4095.0);
        }
        else
        {
            sprintf(resp, "OK:TRIG AUTO\n");
        }
        cmd_reply(resp);
    }
    else if (strcmp(cmd, "EDGE?") == 0 && arg == 0)
    {
        cmd_reply(g_edge_rise ? "OK:EDGE RISE\n" : "OK:EDGE FALL\n");
    }
    else if (strcmp(cmd, "MODE?") == 0 && arg == 0)
    {
        sprintf(resp, "OK:MODE %s\n",
                (g_mode == CMD_MODE_AUTO) ? "AUTO" :
                (g_mode == CMD_MODE_NORM) ? "NORM" : "SINGLE");
        cmd_reply(resp);
    }
    else if (strcmp(cmd, "CH?") == 0 && arg == 0)
    {
        sprintf(resp, "OK:CH1 %s,CH2 %s\n",
                g_ch_en[0] ? "ON" : "OFF",
                g_ch_en[1] ? "ON" : "OFF");
        cmd_reply(resp);
    }
    else if (strcmp(cmd, "WAVE?") == 0 && arg == 0)
    {
        if (!uart2_tx_busy() && !g_pending_flag && !g_wave_pending)
        {
            u16 n = cmd_build_wave();
            uart2_send_dma(n);
        }
        else
        {
            g_wave_pending = 1;
        }
    }
    else
    {
        cmd_err();
    }
}


/* Auto range: comparator + hysteresis.
 *
 * Horizontal "density" = how many cycles fit on one screen:
 *   density > CMD_CYC_HI -> too crowded, shrink the time window (smaller TB)
 *   density < CMD_CYC_LO -> too sparse,  widen  the time window (larger TB)
 *   in between           -> hold        (no continuous hunting)
 *
 * Vertical "fill" = Vpp / (8 divisions * V/div):
 *   fill > CMD_FILL_HI -> raise V/div (larger range, smaller wave)
 *   fill < CMD_FILL_LO -> lower V/div (smaller range, larger wave)
 *   in between         -> hold
 *
 * Both axes need CMD_AUTO_DEBOUNCE consecutive out-of-band measurements
 * before one step is taken, and only one axis moves per update. */
#define CMD_AUTO_TICK       5U
#define CMD_AUTO_DEBOUNCE   3U
#define CMD_CYC_HI          12.0f
#define CMD_CYC_LO          4.0f
#define CMD_CYC_TARGET      2.5f
#define CMD_MEAS_CYCLES     8.0f
#define CMD_FILL_HI         0.85f
#define CMD_FILL_LO         0.45f
#define CMD_FILL_TARGET     0.70f

static void cmd_auto_range(void)
{
    static u8 tick = 0;
    static u8 cyc_hi_cnt = 0;
    static u8 cyc_lo_cnt = 0;
    static u8 fill_hi_cnt[2] = {0U, 0U};
    static u8 fill_lo_cnt[2] = {0U, 0U};
    static u8 no_cyc_cnt = 0;

    MEAS_Result m;
    MEAS_Result m1;
    MEAS_Result m2;
    u32 fs;
    u32 meas_win;
    u32 target_fs;
    u16 vd;
    float fill;
    float f_high = 0.0f;
    u8 need_step = 0;
    u8 ch;

    if (!g_run || g_auto_paused ||
        (g_mode == CMD_MODE_NORM && s_norm_hold))
    {
        cyc_hi_cnt = 0;
        cyc_lo_cnt = 0;
        fill_hi_cnt[0] = fill_hi_cnt[1] = 0U;
        fill_lo_cnt[0] = fill_lo_cnt[1] = 0U;
        return;
    }

    tick++;
    if (tick < CMD_AUTO_TICK) return;
    tick = 0;

    /* Measure the frame that is actually on screen (trigger-aligned and
     * held by CMD_Task).  A missed trigger must not stop the auto range:
     * at a low frequency the frame may contain less than one cycle, and
     * the only way to recover is to widen the capture window. */
    if (CMD_DisplaySeq() == 0U) return;

    fs = ADC1_GetSampleRate();
    MEAS_Compute(CMD_DisplayFrame(),  ADC_FRAME_LEN, (float)fs, &m1);
    MEAS_Compute(CMD_DisplayFrame2(), ADC_FRAME_LEN, (float)fs, &m2);

    /* Select the visible channel(s) for the shared vertical scale and
     * the time base.  One channel alone gets its own auto range; with
     * both visible use the larger Vpp and the average bias. */
    if (!g_ch_en[0] && !g_ch_en[1])
    {
        cyc_hi_cnt = 0;
        cyc_lo_cnt = 0;
        fill_hi_cnt[0] = fill_hi_cnt[1] = 0U;
        fill_lo_cnt[0] = fill_lo_cnt[1] = 0U;
        no_cyc_cnt = 0;
        return;
    }
    else if (g_ch_en[0] && g_ch_en[1])
    {
        /* A dead/noisy channel must not steal the shared time base.
         * Only channels with a real AC waveform are candidates; if both
         * are valid, use the lower frequency so the captured frame still
         * contains enough cycles for both channels' frequency estimate. */
        u8 ok1 = (u8)((m1.freq >= 20.0f && m1.vpp >= 0.03f &&
                       m1.type != 0U && m1.type != 3U) ? 1U : 0U);
        u8 ok2 = (u8)((m2.freq >= 20.0f && m2.vpp >= 0.03f &&
                       m2.type != 0U && m2.type != 3U) ? 1U : 0U);

        m = m1;
        if (m2.vpp > m.vpp) m.vpp = m2.vpp;
        if (ok1 && ok2)
        {
            m.freq = (m1.freq < m2.freq) ? m1.freq : m2.freq;
            f_high = (m1.freq > m2.freq) ? m1.freq : m2.freq;
        }
        else if (ok1)
        {
            m.freq = m1.freq;
            f_high = m1.freq;
        }
        else if (ok2)
        {
            m.freq = m2.freq;
            f_high = m2.freq;
        }
        else
        {
            m.freq = 0.0f;
            f_high = 0.0f;
        }
        m.dc = (m1.vdc_bias + m2.vdc_bias) * 0.5f;
        m.type = (m1.vpp >= m2.vpp) ? m1.type : m2.type;
    }
    else if (g_ch_en[0])
    {
        m = m1;
        m.dc = m1.vdc_bias;
        f_high = m1.freq;
    }
    else
    {
        m = m2;
        m.dc = m2.vdc_bias;
        f_high = m2.freq;
    }

    if (m.vpp < 0.03f)
    {
        cyc_hi_cnt = 0;
        cyc_lo_cnt = 0;
        fill_hi_cnt[0] = fill_hi_cnt[1] = 0U;
        fill_lo_cnt[0] = fill_lo_cnt[1] = 0U;
        no_cyc_cnt = 0;
        return;
    }

    if ((!g_ch_en[0] || m1.type == 3U) &&
        (!g_ch_en[1] || m2.type == 3U))
    {
        cyc_hi_cnt = 0;
        cyc_lo_cnt = 0;
        fill_hi_cnt[0] = fill_hi_cnt[1] = 0U;
        fill_lo_cnt[0] = fill_lo_cnt[1] = 0U;
        no_cyc_cnt = 0;
        return;
    }

    if (m.freq < 0.5f)
    {
        cyc_hi_cnt = 0;
        cyc_lo_cnt = 0;
        fill_hi_cnt[0] = fill_hi_cnt[1] = 0U;
        fill_lo_cnt[0] = fill_lo_cnt[1] = 0U;

        /* Less than one complete cycle in the frame: halve the sample
         * rate (double the capture time) until a period is visible. */
        if (g_auto_tb && ++no_cyc_cnt >= CMD_AUTO_DEBOUNCE)
        {
            no_cyc_cnt = 0;
            if (fs > 1000U)
            {
                ADC1_SetSampleRate(fs / 2U);
                fs = ADC1_GetSampleRate();
            }
            g_disp_span = ADC_FRAME_LEN;
            g_capture_tb_us = (ADC_FRAME_LEN * 1000000U) / (fs * 10U);
            g_tb_us = g_capture_tb_us;
        }
        return;
    }
    no_cyc_cnt = 0;

    /* ---------------- vertical comparator (per channel) ---------------- */
    if (g_auto_vdiv)
    {
        for (ch = 0U; ch < 2U; ch++)
        {
            MEAS_Result *mm = (ch == 0U) ? &m1 : &m2;

            if (!g_ch_en[ch] || mm->vpp < 0.03f || mm->type == 3U)
            {
                fill_hi_cnt[ch] = 0U;
                fill_lo_cnt[ch] = 0U;
                continue;
            }

            fill = mm->vpp / (((float)g_vdiv_mv[ch] / 1000.0f) * 8.0f);

            if (fill > CMD_FILL_HI)
            {
                fill_lo_cnt[ch] = 0U;
                if (++fill_hi_cnt[ch] >= CMD_AUTO_DEBOUNCE)
                {
                    fill_hi_cnt[ch] = 0U;
                    vd = (u16)(mm->vpp * 1000.0f /
                               (8.0f * CMD_FILL_TARGET) + 0.5f);
                    if (vd < 20U)   vd = 20U;
                    if (vd > 5000U) vd = 5000U;
                    if (vd != g_vdiv_mv[ch])
                    {
                        g_vdiv_mv[ch] = vd;
                    }
                }
            }
            else if (fill < CMD_FILL_LO)
            {
                fill_hi_cnt[ch] = 0U;
                if (++fill_lo_cnt[ch] >= CMD_AUTO_DEBOUNCE)
                {
                    fill_lo_cnt[ch] = 0U;
                    vd = (u16)(mm->vpp * 1000.0f /
                               (8.0f * CMD_FILL_TARGET) + 0.5f);
                    if (vd < 20U)   vd = 20U;
                    if (vd > 5000U) vd = 5000U;
                    if (vd != g_vdiv_mv[ch])
                    {
                        g_vdiv_mv[ch] = vd;
                    }
                }
            }
            else
            {
                fill_hi_cnt[ch] = 0U;
                fill_lo_cnt[ch] = 0U;
            }
        }
    }
    else
    {
        fill_hi_cnt[0] = 0U;
        fill_hi_cnt[1] = 0U;
        fill_lo_cnt[0] = 0U;
        fill_lo_cnt[1] = 0U;
    }

    if (g_auto_vdiv)
    {
        u16 vd_use;

        if (g_ch_en[0] && g_ch_en[1])
        {
            /* Same V/div for both channels: the same voltage step on
             * both left and right axes (the centres may differ). */
            vd_use = (g_vdiv_mv[0] > g_vdiv_mv[1]) ? g_vdiv_mv[0]
                                                   : g_vdiv_mv[1];
            vd_use = cmd_nice_vdiv(vd_use);
            g_vdiv_mv[0] = vd_use;
            g_vdiv_mv[1] = vd_use;
        }
        else if (g_ch_en[0])
        {
            g_vdiv_mv[0] = cmd_nice_vdiv(g_vdiv_mv[0]);
        }
        else if (g_ch_en[1])
        {
            g_vdiv_mv[1] = cmd_nice_vdiv(g_vdiv_mv[1]);
        }
    }

    /* Vertical centre follows the channel's real DC bias even when the
     * V/div is manual.  Only a 10 mV quantisation is applied: snapping
     * the centre to a whole V/div (the old behaviour) moved the trace by
     * up to half a division and made it look off-centre. */
    for (ch = 0U; ch < 2U; ch++)
    {
        MEAS_Result *mm = (ch == 0U) ? &m1 : &m2;
        s32 target;
        s32 cur;
        s32 d;

        if (!g_ch_en[ch]) continue;

        target = (s32)(mm->vdc_bias * 1000.0f + 0.5f);
        cur = (s32)g_vcenter_mv[ch];
        d = target - cur;

        if (d > 8)       cur += (d + 7) / 8;
        else if (d < -8) cur -= ((-d + 7) / 8);

        cur = ((cur + 5) / 10) * 10;
        if (cur < 0)    cur = 0;
        if (cur > 3300) cur = 3300;
        g_vcenter_mv[ch] = (u16)cur;
    }

    /* ---------------- horizontal comparator ---------------- */
    if (g_auto_tb)
    {
        float frame_us;
        float frame_cycles;
        u16 target_span;

        /* Density of the CAPTURED frame (not just the visible part).
         * Too many cycles per frame -> raise fs / shorten the capture;
         * too few -> lower fs / widen it.  The visible window is then
         * always set to about 2.5 cycles, while the frame keeps ~8
         * cycles for the FFT and zero-crossing measurement. */
        frame_us = ((float)ADC_FRAME_LEN * 1000000.0f) / (float)fs;
        frame_cycles = m.freq * frame_us / 1000000.0f;

        if (frame_cycles > CMD_CYC_HI)
        {
            cyc_lo_cnt = 0;
            if (++cyc_hi_cnt >= CMD_AUTO_DEBOUNCE)
            {
                cyc_hi_cnt = 0;
                need_step = 1;
            }
        }
        else if (frame_cycles < CMD_CYC_LO)
        {
            cyc_hi_cnt = 0;
            if (++cyc_lo_cnt >= CMD_AUTO_DEBOUNCE)
            {
                cyc_lo_cnt = 0;
                need_step = 1;
            }
        }
        else
        {
            cyc_hi_cnt = 0;
            cyc_lo_cnt = 0;
        }

        if (need_step)
        {
            meas_win = (u32)(CMD_MEAS_CYCLES * 1000000.0f / m.freq);
            if (meas_win < 20U)       meas_win = 20U;
            if (meas_win > 8000000U)  meas_win = 8000000U;

            target_fs = (u32)(((float)ADC_FRAME_LEN * 1000000.0f) /
                              (float)meas_win);
            if (target_fs < 1000U)   target_fs = 1000U;
            if (target_fs > 500000U) target_fs = 500000U;

            /* If CH1 and CH2 have very different frequencies, a sample
             * rate chosen only for the lower channel can alias the high
             * channel.  Keep at least 2.5x the higher fundamental. */
            if (f_high > 0.0f)
            {
                u32 fs_floor = (u32)(f_high * 2.5f + 0.5f);
                if (target_fs < fs_floor) target_fs = fs_floor;
                if (target_fs > 500000U) target_fs = 500000U;
            }

            if (target_fs < (fs * 9U / 10U) || target_fs > (fs * 11U / 10U))
            {
                ADC1_SetSampleRate(target_fs);
                fs = ADC1_GetSampleRate();
            }
        }

        target_span = (u16)((CMD_CYC_TARGET * 1000000.0f / m.freq) *
                            (float)fs / 1000000.0f);
        if (target_span < 16U)           target_span = 16U;
        if (target_span > ADC_FRAME_LEN) target_span = ADC_FRAME_LEN;

        if (target_span != g_disp_span)
        {
            g_disp_span = target_span;
            g_capture_tb_us = (ADC_FRAME_LEN * 1000000U) / (fs * 10U);
            g_tb_us = ((u32)target_span * 1000000U) / (fs * 10U);
            if (g_tb_us == 0U) g_tb_us = 1U;
        }

        /* Snap the automatic timebase to a regular step so the X axis
         * reads 0, 0.5, 1.0, 1.5 ... instead of arbitrary values. */
        {
            u32 nice = cmd_nice_tb(g_tb_us);
            if (nice != g_tb_us)
            {
                u32 sp = (nice * 10U * fs) / 1000000U;
                if (sp < 16U) sp = 16U;
                if (sp > ADC_FRAME_LEN) sp = ADC_FRAME_LEN;
                g_tb_us = nice;
                g_disp_span = (u16)sp;
            }
        }
    }
    else if (!g_auto_tb)
    {
        cyc_hi_cnt = 0;
        cyc_lo_cnt = 0;
    }
}

/* ------------------------------------------------------------------ */
/* public interface                                                   */
/* ------------------------------------------------------------------ */

void CMD_Init(void)
{
    u16 i;

    g_run = 1;
    g_mode = CMD_MODE_AUTO;
    g_single_armed = 0;
    g_edge_rise = 1;
    g_trig_manual = 0;
    g_auto_tb = 1;
    g_auto_vdiv = 1;
    g_auto_paused = 0;
    g_ch_en[0] = 1U;
    g_ch_en[1] = 1U;
    g_trig_level = 2048;
    g_tb_us = 1000;
    g_capture_tb_us = 1000;
    g_disp_span = ADC_FRAME_LEN;
    g_disp_tb_us = 1000;
    g_disp_span_snap = ADC_FRAME_LEN;
    g_vdiv_mv[0] = 250U;
    g_vdiv_mv[1] = 250U;
    g_vcenter_mv[0] = 1500U;
    g_vcenter_mv[1] = 1500U;
g_cmd_len = 0;
    g_cmd_state = 0;
    g_pending_flag = 0;
    g_wave_pending = 0;

    s_disp_trig = 0;
    s_disp_found = 0;
    s_disp_seq = 0;
    s_trig_miss = 0;
    s_norm_hold = 0;
    s_wave_type = 0;
    MEAS_TypeFSMReset();
    for (i = 0; i < ADC_FRAME_LEN; i++)
    {
        s_disp_frame1[i] = 0;
        s_disp_frame2[i] = 0;
    }
}

void CMD_Task(void)
{
    u8 ch;
    u8 new_frame;
    u16 i;
    u16 idx;
    static u16 s_no_frame = 0;

    if (uart2_rx_overflow())
    {
        g_cmd_len = 0;
        g_cmd_state = 2;
        cmd_err();
    }

    while (uart2_getc(&ch))
    {
        if (ch == '\r') continue;

        if (g_cmd_state == 2)
        {
            if (ch == '\n') g_cmd_state = 0;
            continue;
        }

        if (ch == '\n')
        {
            if (g_cmd_len > 0U)
            {
                g_cmd_line[g_cmd_len] = 0;
                cmd_process(g_cmd_line);
            }
            g_cmd_len = 0;
            g_cmd_state = 0;
        }
        else if (g_cmd_len >= 200U)
        {
            g_cmd_len = 0;
            g_cmd_state = 2;
            cmd_err();
        }
        else
        {
            g_cmd_line[g_cmd_len++] = ch;
            g_cmd_state = 1;
        }
    }

    if (g_pending_flag && !uart2_tx_busy())
    {
        u16 n = (u16)strlen(g_pending);
        memcpy(g_uart2_tx_buf, g_pending, n);
        g_pending_flag = 0;
        uart2_send_dma(n);
    }

    if (g_wave_pending && !uart2_tx_busy() && !g_pending_flag)
    {
        u16 n = cmd_build_wave();
        g_wave_pending = 0;
        uart2_send_dma(n);
    }

    /* Keep the acquisition frame fresh even while the UART is busy. */
    new_frame = g_run ? ADC1_CopyFrame() : 0;

    /* No-reset recovery: if the ADC/DMA stream stops delivering frames,
     * reinitialize the capture path instead of asking the user to reset. */
    if (g_run)
    {
        u32 fs = ADC1_GetSampleRate();
        u32 frame_ms = (ADC_FRAME_LEN * 1000U) / fs;
        u16 limit = (u16)(frame_ms / 20U + 100U);

        if (new_frame)
        {
            s_no_frame = 0;
        }
        else if (++s_no_frame > limit)
        {
            s_no_frame = 0;
            ADC1_Init();
        }
    }
    else
    {
        s_no_frame = 0;
    }

    if (new_frame)
    {
        u16 trig1 = 0;
        u16 trig2 = 0;
        u16 trig;
        u8  found1;
        u8  found2;
        u8  found;
        u8  use1;
        u8  use2;
        u8  accept = 1;

        found1 = ADC1_FindTrigger(g_edge_rise, g_trig_manual,
                                  g_trig_level, &trig1);
        found2 = ADC2_FindTrigger(g_edge_rise, g_trig_manual,
                                  g_trig_level, &trig2);
        use1 = (u8)((g_ch_en[0] && found1) ? 1U : 0U);
        use2 = (u8)((g_ch_en[1] && found2) ? 1U : 0U);
        found = (u8)((use1 || use2) ? 1U : 0U);
        trig  = use1 ? trig1 : (use2 ? trig2 : 0U);

        /* NORM must not let the auto range keep rescaling the last
         * frame after the trigger disappears.  Keep everything frozen
         * until a real trigger returns. */
        if (g_mode == CMD_MODE_NORM)
            s_norm_hold = found ? 0U : 1U;
        else
            s_norm_hold = 0U;

        if (found)
        {
            s_trig_miss = 0;
        }
        else if (g_mode == CMD_MODE_AUTO)
        {
            /* Hold only one frame when the trigger disappears, then
             * free-run immediately so a changed signal source is
             * reflected well inside the 2 s requirement. */
            if (s_disp_found && s_trig_miss < 1U)
            {
                s_trig_miss++;
                accept = 0;
            }
            else
            {
                s_trig_miss++;
            }
        }
        else if (g_mode == CMD_MODE_NORM)
        {
            accept = 0;                 /* hold last frame until a trigger */
        }
        else if (g_mode == CMD_MODE_SINGLE)
        {
            if (!g_single_armed || !found) accept = 0;
        }

        if (accept)
        {
            const u16 *src1 = ADC1_Frame();
            const u16 *src2 = ADC2_Frame();
            u16 t = trig;

            if (t >= ADC_FRAME_LEN) t = 0U;

            /* Rotate both frames so the trigger is at sample 0.  The
             * display window then never wraps from the end of the frame
             * back to the start, which used to leave a one-column spike
             * at the right edge of the plot. */
            for (i = t; i < ADC_FRAME_LEN; i++)
            {
                s_disp_frame1[i - t] = src1[i];
                s_disp_frame2[i - t] = src2[i];
            }
            for (i = 0U; i < t; i++)
            {
                s_disp_frame1[ADC_FRAME_LEN - t + i] = src1[i];
                s_disp_frame2[ADC_FRAME_LEN - t + i] = src2[i];
            }

            s_disp_trig   = 0U;
            s_disp_found  = found;
            g_disp_tb_us  = g_tb_us;
            g_disp_span_snap = g_disp_span;
            s_disp_seq++;

            /* CH1 type uses the smoothing state machine; CH2 is
             * classified directly by MEAS_Compute in the UI/MEAS?. */
            {
                MEAS_Result m_type;
                MEAS_Compute(s_disp_frame1, ADC_FRAME_LEN,
                             (float)ADC1_GetSampleRate(), &m_type);
                s_wave_type = MEAS_TypeFSM(m_type.type, m_type.conf);
            }

            if (g_mode == CMD_MODE_SINGLE)
            {
                g_single_armed = 0;
                g_run = 0;              /* captured: stop, re-arm to repeat */
            }
        }
    }

    cmd_auto_range();

    /* VOFA+ FireWater stream: fixed two columns CH1,CH2 in millivolts.
     * Always send both columns (even if one scope card is off) so the
     * host channel count never changes; hiding a channel is a screen
     * operation, not a stream-format operation. */
    if (g_run && new_frame && !uart2_tx_busy() &&
        !g_pending_flag && !g_wave_pending &&
        CMD_DisplaySeq() != 0U)
    {
        u16 span = CMD_DisplaySpan();
        u16 step = 1U;
        u16 pts;
        u16 pos = 0;
        u16 k;
        u16 trig = CMD_DisplayTrig();
        const u16 *f1 = CMD_DisplayFrame();
        const u16 *f2 = CMD_DisplayFrame2();

        if (span == 0U || span > ADC_FRAME_LEN) span = ADC_FRAME_LEN;
        pts = span;
        if (pts > 256U)
        {
            step = (u16)((pts + 255U) / 256U);
            pts  = (u16)((pts + step - 1U) / step);
        }

        for (k = 0; k < pts; k++)
        {
            u16 mv1;
            u16 mv2;

            idx = (u16)(trig + (u32)k * step);
            if (idx >= ADC_FRAME_LEN) idx = (u16)(idx - ADC_FRAME_LEN);

            if (pos + 16U >= UART2_TX_BUF_SIZE) break;

            mv1 = (u16)(((u32)f1[idx] * 3300U + 2048U) / 4096U);
            mv2 = (u16)(((u32)f2[idx] * 3300U + 2048U) / 4096U);
            pos += (u16)sprintf(&g_uart2_tx_buf[pos], "%u,%u\n", mv1, mv2);
        }

        if (pos > 0U) uart2_send_dma(pos);
    }
}

/* state read back */
u8 CMD_IsRunning(void)       { return g_run; }
u8 CMD_GetMode(void)         { return g_mode; }
u8 CMD_GetSingleArmed(void)  { return g_single_armed; }
u8 CMD_GetEdgeRise(void)     { return g_edge_rise; }
u8 CMD_GetTrigManual(void)   { return g_trig_manual; }
u16 CMD_GetTrigLevel(void)   { return g_trig_level; }
u32 CMD_GetTimebaseUs(void)  { return g_tb_us; }
u32 CMD_GetDisplayTimebaseUs(void) { return g_disp_tb_us; }
u16 CMD_GetDisplaySpan(void) { return g_disp_span_snap; }
u16 CMD_GetVdivMv(void)      { return g_vdiv_mv[0]; }
u16 CMD_GetVcenterMv(void)   { return g_vcenter_mv[0]; }
u16 CMD_GetVdivMvCh(u8 ch)   { return (ch < 2U) ? g_vdiv_mv[ch] : g_vdiv_mv[0]; }
u16 CMD_GetVcenterMvCh(u8 ch){ return (ch < 2U) ? g_vcenter_mv[ch] : g_vcenter_mv[0]; }

/* UI setters */
void CMD_SetRunning(u8 run)
{
    if (run)
    {
        g_run = 1;
        if (g_mode == CMD_MODE_SINGLE)
        {
            g_mode = CMD_MODE_AUTO;
            g_single_armed = 0;
        }
    }
    else
    {
        g_run = 0;
        g_single_armed = 0;
    }
}

void CMD_ArmSingle(void)
{
    g_mode = CMD_MODE_SINGLE;
    g_run = 1;
    g_single_armed = 1;
    s_norm_hold = 0;
}

void CMD_SetMode(u8 mode)
{
    if (mode == CMD_MODE_SINGLE)
    {
        CMD_ArmSingle();
        return;
    }
    if (mode > CMD_MODE_NORM) mode = CMD_MODE_AUTO;
    g_mode = mode;
    g_run = 1;
    g_single_armed = 0;
    s_norm_hold = 0;
}

void CMD_CycleMode(void)
{
    if (g_mode == CMD_MODE_AUTO)       CMD_SetMode(CMD_MODE_NORM);
    else if (g_mode == CMD_MODE_NORM)  CMD_SetMode(CMD_MODE_SINGLE);
    else                               CMD_SetMode(CMD_MODE_AUTO);
}

void CMD_CycleEdge(void)
{
    g_edge_rise = (u8)(g_edge_rise ? 0 : 1);
}

void CMD_StepTimebase(s8 dir)
{
    u8 idx = cmd_nearest_tb_index();

    if (dir > 0)
    {
        if (idx < CMD_TB_COUNT - 1U) idx++;
    }
    else
    {
        if (idx > 0U) idx--;
    }
    cmd_set_timebase(s_tb_list[idx]);
}

void CMD_SetTimebaseUs(u32 us)
{
    cmd_set_timebase(us);
}

void CMD_StepVdiv(s8 dir)
{
    u8 idx = cmd_nearest_vd_index();

    if (dir > 0)
    {
        if (idx < CMD_VD_COUNT - 1U) idx++;
    }
    else
    {
        if (idx > 0U) idx--;
    }
    cmd_set_vdiv(s_vd_list[idx]);
}

void CMD_SetVdivMv(u16 mv)
{
    cmd_set_vdiv(mv);
}

void CMD_StepTrigLevel(s16 delta)
{
    s32 v = (s32)g_trig_level + delta;

    if (v < 0)    v = 0;
    if (v > 4095) v = 4095;

    g_trig_level = (u16)v;
    g_trig_manual = 1;
}

void CMD_SetTrigLevelRaw(u16 raw)
{
    if (raw > 4095U) raw = 4095U;
    g_trig_level = raw;
    g_trig_manual = 1;
}

void CMD_EnableAutoTrigLevel(void)
{
    g_trig_manual = 0;
}

/* display frame */
const u16 *CMD_DisplayFrame(void)  { return s_disp_frame1; }
const u16 *CMD_DisplayFrame2(void) { return s_disp_frame2; }
u16 CMD_DisplayTrig(void)         { return s_disp_trig; }
u8  CMD_DisplayFound(void)        { return s_disp_found; }
u32 CMD_DisplaySeq(void)          { return s_disp_seq; }
u16 CMD_DisplaySpan(void)         { return g_disp_span_snap; }
u8  CMD_GetWaveType(void)         { return s_wave_type; }

/* auto timebase flag (serial "AUTO" command; reserved for the auto-range
 * helper, exposed so the UI can show the state when that is wired up) */
u8 CMD_GetAutoTimebase(void)
{
    return g_auto_tb;
}

void CMD_EnableAutoRange(void)
{
    g_auto_tb = 1;
    g_auto_vdiv = 1;
    g_auto_paused = 0;
}

void CMD_ToggleAutoRange(void)
{
    if (g_auto_tb || g_auto_vdiv)
    {
        /* Freeze the current scale: back to the full captured frame. */
        g_auto_tb = 0;
        g_auto_vdiv = 0;
        g_auto_paused = 0;
        g_disp_span = ADC_FRAME_LEN;
        g_capture_tb_us = (ADC_FRAME_LEN * 1000000U) /
                          (ADC1_GetSampleRate() * 10U);
        g_tb_us = g_capture_tb_us;
    }
    else
    {
        g_auto_tb = 1;
        g_auto_vdiv = 1;
        g_auto_paused = 0;
    }
}

void CMD_SetAutoPause(u8 pause)
{
    g_auto_paused = pause ? 1U : 0U;
}

u8 CMD_GetAutoVdiv(void)
{
    return g_auto_vdiv;
}

u8 CMD_GetChannelEnabled(u8 ch)
{
    if (ch > 1U) return 0U;
    return g_ch_en[ch];
}

void CMD_ToggleChannel(u8 ch)
{
    if (ch > 1U) return;
    g_ch_en[ch] = (u8)(g_ch_en[ch] ? 0U : 1U);
}

void CMD_SetChannelEnabled(u8 ch, u8 en)
{
    if (ch > 1U) return;
    g_ch_en[ch] = en ? 1U : 0U;
}

void CMD_ToggleRun(void)
{
    CMD_SetRunning((u8)(g_run ? 0 : 1));
}
