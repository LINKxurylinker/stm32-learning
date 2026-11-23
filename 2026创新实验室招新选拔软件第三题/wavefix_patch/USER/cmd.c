#include "cmd.h"
#include "adc.h"
#include "measure.h"
#include "usart.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Time base and vertical gain tables used by the touch UI.
 * TB is the time per horizontal division; the whole frame is 10 div. */
#define CMD_TB_COUNT   6U
#define CMD_VD_COUNT   5U

static const u32 s_tb_list[CMD_TB_COUNT] = {500U, 1000U, 2000U, 5000U, 10000U, 20000U};
static const u16 s_vd_list[CMD_VD_COUNT] = {200U, 500U, 1000U, 2000U, 5000U};

static u8  g_run = 1;
static u8  g_mode = CMD_MODE_AUTO;
static u8  g_single_armed = 0;
static u8  g_edge_rise = 1;
static u8  g_trig_manual = 0;
static u8  g_auto_tb = 1;
static u8  g_auto_vdiv = 1;
static u8  g_auto_paused = 0;
static u16 g_trig_level = 2048;
static u32 g_tb_us = 1000;
static u32 g_capture_tb_us = 1000;
static u16 g_disp_span = ADC_FRAME_LEN;
static u16 g_vdiv_mv = 1000;
static u16 g_vcenter_mv = 1650;
static u16 g_send_len = ADC_FRAME_LEN;

/* Last frame accepted for the display.  NORM only accepts frames with a
 * trigger, SINGLE accepts exactly one frame after arming. */
static u16 s_disp_frame[ADC_FRAME_LEN];
static u16 s_disp_trig = 0;
static u8  s_disp_found = 0;
static u32 s_disp_seq = 0;
static u16 s_trig_miss = 0;

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
    g_send_len = ADC_FRAME_LEN;
}

static void cmd_set_vdiv(u16 mv)
{
    g_auto_vdiv = 0;
    if (mv < 100U)  mv = 100U;
    if (mv > 5000U) mv = 5000U;
    g_vdiv_mv = mv;
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
        d = (g_vdiv_mv > s_vd_list[i]) ? (g_vdiv_mv - s_vd_list[i])
                                       : (s_vd_list[i] - g_vdiv_mv);
        if (d < diff)
        {
            diff = d;
            best = i;
        }
    }
    return best;
}

static u8 cmd_parse_time(const char *s, u32 *us)
{
    double v = atof(s);
    if (v <= 0.0) return 0;

    if (strstr(s, "MS")) v *= 1000.0;
    else if (strstr(s, "US")) v *= 1.0;
    else if (strstr(s, "S")) v *= 1000000.0;

    if (v < 10.0 || v > 1000000.0) return 0;
    *us = (u32)(v + 0.5);
    return 1;
}

static void cmd_measure(void)
{
    MEAS_Result m;
    char resp[160];

    MEAS_Compute(CMD_DisplayFrame(), ADC_FRAME_LEN,
                 (float)ADC1_GetSampleRate(), &m);

    sprintf(resp, "MEAS:FREQ=%.1fHz,VPP=%.3fV,DC=%.3fV,TYPE=%s\n",
            m.freq, m.vpp, m.dc, MEAS_TypeName(m.type));
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
            v = atof(arg);
            if (v < 0.0 || v > 3.3)
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
        else
        {
            cmd_set_timebase(us);
            cmd_reply("OK:TB\n");
        }
    }
    else if (strcmp(cmd, "VDIV") == 0 && arg != 0)
    {
        v = atof(arg);
        if (v < 0.1 || v > 5.0)
        {
            cmd_err();
        }
        else
        {
            cmd_set_vdiv((u16)(v * 1000.0 + 0.5));
            sprintf(resp, "OK:VDIV %umV\n", g_vdiv_mv);
            cmd_reply(resp);
        }
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
#define CMD_CYC_HI          4.0f
#define CMD_CYC_LO          1.5f
#define CMD_CYC_TARGET      2.5f
#define CMD_FILL_HI         0.85f
#define CMD_FILL_LO         0.45f
#define CMD_FILL_TARGET     0.70f

static void cmd_auto_range(void)
{
    static u8 tick = 0;
    static u8 cyc_hi_cnt = 0;
    static u8 cyc_lo_cnt = 0;
    static u8 fill_hi_cnt = 0;
    static u8 fill_lo_cnt = 0;

    MEAS_Result m;
    u32 fs;
    u32 win_us;
    u32 new_win;
    u32 target_fs;
    u16 span;
    u16 vd;
    float cycles;
    float fill;
    u8 changed = 0;

    if (!g_run || g_auto_paused)
    {
        cyc_hi_cnt = 0;
        cyc_lo_cnt = 0;
        fill_hi_cnt = 0;
        fill_lo_cnt = 0;
        return;
    }

    tick++;
    if (tick < CMD_AUTO_TICK) return;
    tick = 0;

    /* Measure the frame that is actually on screen (trigger-aligned and
     * held by CMD_Task).  If the trigger was missed, don't chase a
     * stale/free-running frame with the auto range. */
    if (CMD_DisplaySeq() == 0U) return;
    if (!CMD_DisplayFound()) return;

    fs = ADC1_GetSampleRate();
    MEAS_Compute(CMD_DisplayFrame(), ADC_FRAME_LEN, (float)fs, &m);

    /* No usable signal: hold the current range instead of hunting noise. */
    if (m.vpp < 0.03f || m.freq < 0.5f)
    {
        cyc_hi_cnt = 0;
        cyc_lo_cnt = 0;
        fill_hi_cnt = 0;
        fill_lo_cnt = 0;
        return;
    }

    /* ---------------- vertical comparator ---------------- */
    if (g_auto_vdiv)
    {
        fill = m.vpp / (((float)g_vdiv_mv / 1000.0f) * 8.0f);

        if (fill > CMD_FILL_HI)
        {
            cyc_hi_cnt = 0;
            cyc_lo_cnt = 0;
            fill_lo_cnt = 0;
            if (++fill_hi_cnt >= CMD_AUTO_DEBOUNCE)
            {
                fill_hi_cnt = 0;
                changed = 1;
            }
        }
        else if (fill < CMD_FILL_LO)
        {
            cyc_hi_cnt = 0;
            cyc_lo_cnt = 0;
            fill_hi_cnt = 0;
            if (++fill_lo_cnt >= CMD_AUTO_DEBOUNCE)
            {
                fill_lo_cnt = 0;
                changed = 1;
            }
        }
        else
        {
            fill_hi_cnt = 0;
            fill_lo_cnt = 0;
        }

        if (changed)
        {
            vd = (u16)(m.vpp * 1000.0f / (8.0f * CMD_FILL_TARGET) + 0.5f);
            if (vd < 20U)   vd = 20U;
            if (vd > 5000U) vd = 5000U;
            vd = (u16)((vd / 10U) * 10U);       /* 10 mV quantisation */
            if (vd < 20U) vd = 20U;

            if (vd != g_vdiv_mv) g_vdiv_mv = vd;
            else                 changed = 0;   /* no visible change */
        }

        /* Vertical centre follows the DC slowly, so it never jumps. */
        {
            s32 target = (s32)(m.dc * 1000.0f + 0.5f);
            s32 cur = (s32)g_vcenter_mv;
            s32 d = target - cur;

            if (d > 8)       cur += (d + 7) / 8;
            else if (d < -8) cur -= ((-d + 7) / 8);

            if (cur < 0)    cur = 0;
            if (cur > 3300) cur = 3300;
            g_vcenter_mv = (u16)cur;
        }
    }
    else
    {
        fill_hi_cnt = 0;
        fill_lo_cnt = 0;
    }

    /* ---------------- horizontal comparator ---------------- */
    if (g_auto_tb && !changed)
    {
        win_us = ((u32)g_disp_span * 1000000U) / fs;
        if (win_us == 0U) win_us = 1U;

        cycles = m.freq * (float)win_us / 1000000.0f;
        new_win = 0;

        if (cycles > CMD_CYC_HI)
        {
            cyc_lo_cnt = 0;
            if (++cyc_hi_cnt >= CMD_AUTO_DEBOUNCE)
            {
                cyc_hi_cnt = 0;
                new_win = (u32)(CMD_CYC_TARGET * 1000000.0f / m.freq);
            }
        }
        else if (cycles < CMD_CYC_LO)
        {
            cyc_hi_cnt = 0;
            if (++cyc_lo_cnt >= CMD_AUTO_DEBOUNCE)
            {
                cyc_lo_cnt = 0;
                new_win = (u32)(CMD_CYC_TARGET * 1000000.0f / m.freq);
            }
        }
        else
        {
            cyc_hi_cnt = 0;
            cyc_lo_cnt = 0;
        }

        if (new_win != 0U)
        {
            if (new_win < 20U)       new_win = 20U;
            if (new_win > 1000000U)  new_win = 1000000U;

            target_fs = (u32)(((float)ADC_FRAME_LEN * 1000000.0f) /
                              (float)new_win);
            if (target_fs < 1000U)   target_fs = 1000U;
            if (target_fs > 500000U) target_fs = 500000U;

            if (target_fs < (fs * 9U / 10U) || target_fs > (fs * 11U / 10U))
            {
                ADC1_SetSampleRate(target_fs);
                fs = ADC1_GetSampleRate();
            }

            span = (u16)((float)new_win * (float)fs / 1000000.0f);
            if (span < 16U)          span = 16U;
            if (span > ADC_FRAME_LEN) span = ADC_FRAME_LEN;

            g_disp_span = span;
            g_capture_tb_us = (ADC_FRAME_LEN * 1000000U) / (fs * 10U);
            g_tb_us = ((u32)span * 1000000U) / (fs * 10U);
            if (g_tb_us == 0U) g_tb_us = 1U;
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
    g_trig_level = 2048;
    g_tb_us = 1000;
    g_capture_tb_us = 1000;
    g_disp_span = ADC_FRAME_LEN;
    g_vdiv_mv = 1000;
    g_vcenter_mv = 1650;
    g_send_len = ADC_FRAME_LEN;
    g_cmd_len = 0;
    g_cmd_state = 0;
    g_pending_flag = 0;
    g_wave_pending = 0;

    s_disp_trig = 0;
    s_disp_found = 0;
    s_disp_seq = 0;
    s_trig_miss = 0;
    for (i = 0; i < ADC_FRAME_LEN; i++) s_disp_frame[i] = 0;
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
        u16 trig = 0;
        u8  found = ADC1_FindTrigger(g_edge_rise, g_trig_manual,
                                     g_trig_level, &trig);
        u8  accept = 1;

        if (found)
        {
            s_trig_miss = 0;
        }
        else if (g_mode == CMD_MODE_AUTO)
        {
            /* A trigger can be missed for a frame or two around an edge
             * (filtering / sampling phase).  Hold the last good frame
             * instead of showing the same signal at a random start
             * phase; free-run after ~3 missing frames so a new waveform
             * is still identified immediately. */
            if (s_disp_found && s_trig_miss < 3U)
            {
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
            const u16 *src = ADC1_Frame();

            for (i = 0; i < ADC_FRAME_LEN; i++)
            {
                s_disp_frame[i] = src[i];
            }
            s_disp_trig  = trig;
            s_disp_found = found;
            s_disp_seq++;

            if (g_mode == CMD_MODE_SINGLE)
            {
                g_single_armed = 0;
                g_run = 0;              /* captured: stop, re-arm to repeat */
            }
        }
    }

    cmd_auto_range();

    /* VOFA+/text stream: only when the UART is free (never blocks UI). */
    if (g_run && new_frame && !uart2_tx_busy() &&
        !g_pending_flag && !g_wave_pending)
    {
        u16 trig = ADC1_TriggerIndex(g_edge_rise, g_trig_manual, g_trig_level);
        u16 pos = 0;
        const u16 *frame = ADC1_Frame();

        for (i = 0; i < g_send_len; i++)
        {
            idx = trig + i;
            if (idx >= ADC_FRAME_LEN) idx -= ADC_FRAME_LEN;

            if (pos + 6U >= UART2_TX_BUF_SIZE) break;
            pos += (u16)sprintf(&g_uart2_tx_buf[pos], "%u\n", frame[idx]);
        }

        uart2_send_dma(pos);
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
u16 CMD_GetVdivMv(void)      { return g_vdiv_mv; }
u16 CMD_GetVcenterMv(void)   { return g_vcenter_mv; }

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
}

void CMD_CycleMode(void)
{
    if (g_mode == CMD_MODE_NORM) CMD_SetMode(CMD_MODE_AUTO);
    else                         CMD_SetMode(CMD_MODE_NORM);
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
const u16 *CMD_DisplayFrame(void) { return s_disp_frame; }
u16 CMD_DisplayTrig(void)         { return s_disp_trig; }
u8  CMD_DisplayFound(void)        { return s_disp_found; }
u32 CMD_DisplaySeq(void)          { return s_disp_seq; }
u16 CMD_DisplaySpan(void)         { return g_disp_span; }

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

void CMD_ToggleRun(void)
{
    CMD_SetRunning((u8)(g_run ? 0 : 1));
}
