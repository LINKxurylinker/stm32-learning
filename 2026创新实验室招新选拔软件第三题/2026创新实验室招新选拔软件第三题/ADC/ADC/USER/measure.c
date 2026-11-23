#include "measure.h"
#include <math.h>
#include "arm_math.h"
#include "arm_const_structs.h"

#define MEAS_PI       3.14159265358979f
#define MEAS_MAX_CR   64U
#define MEAS_FFT_N    2048U
#define MEAS_VREF     3.3f
#define MEAS_ADC_FS   4096.0f

/* CMSIS-DSP FFT buffer: 2048 complex bins = 16 KB (ZI).  The FFT is
 * refreshed only every 4th call because MEAS_Compute() runs from the
 * 100 Hz UI loop as well as the auto-range helper. */
static float32_t s_fft_buf[MEAS_FFT_N * 2U];
/* Two cache slots: CH1 and CH2 are measured alternately in the UI, so
 * the FFT of one channel must never be reused for the other. */
static const u16 *s_fft_key[2] = {0, 0};
static float     s_fft_freq[2] = {0.0f, 0.0f};
static float     s_fft_fs[2]   = {0.0f, 0.0f};
static u16       s_fft_tick[2] = {0U, 0U};
static u8        s_fft_valid[2] = {0U, 0U};

typedef struct
{
    u16 idx[MEAS_MAX_CR];
    u16 count;
    float first_t;
    float last_t;
} meas_cross_t;

/* Rising zero crossings with hysteresis and sub-sample interpolation.
 * count is the total number of crossings; idx[] keeps the first
 * MEAS_MAX_CR sample indices for the slope fit. */
static u16 meas_crossings(const u16 *f, u16 n, u16 level, u16 hyst,
                          float fs, meas_cross_t *cr)
{
    u16 i;
    u8  armed = 0U;
    float a, b, frac, t;

    cr->count   = 0U;
    cr->first_t = 0.0f;
    cr->last_t  = 0.0f;

    for (i = 1U; i < n; i++)
    {
        if ((u32)f[i - 1U] + hyst <= level)
        {
            armed = 1U;
        }
        if (armed && f[i - 1U] < level && f[i] >= level)
        {
            a = (float)f[i - 1U];
            b = (float)f[i];
            frac = (b > a) ? (((float)level - a) / (b - a)) : 0.0f;
            t = ((float)(i - 1U) + frac) / fs;

            if (cr->count == 0U)
            {
                cr->first_t = t;
            }
            cr->last_t = t;
            if (cr->count < MEAS_MAX_CR)
            {
                cr->idx[cr->count] = i;
            }
            cr->count++;
            armed = 0U;
        }
    }
    return cr->count;
}

/* Least-squares slope of one rising sub-segment. */
static u8 meas_fit_slope(const u16 *f, u16 i0, u16 i1, float *slope)
{
    u16 i, cnt;
    float sx = 0.0f, sy = 0.0f, sxx = 0.0f, sxy = 0.0f;
    float a, d;

    if (i1 <= i0 + 2U)          /* need at least 4 points */
    {
        return 0U;
    }

    cnt = (u16)(i1 - i0 + 1U);
    for (i = i0; i <= i1; i++)
    {
        float x = (float)(i - i0);
        float y = (float)f[i];
        sx  += x;
        sy  += y;
        sxx += x * x;
        sxy += x * y;
    }

    d = (float)cnt * sxx - sx * sx;
    if (fabsf(d) < 0.001f)
    {
        return 0U;
    }

    a = ((float)cnt * sxy - sx * sy) / d;
    *slope = a;
    return 1U;
}

/* Triangle test: split each rising edge into 20-40%, 40-60%, 60-80%
 * segments, fit a line to each, and look at the coefficient of
 * variation of the three slopes.  A triangle is linear, so the slopes
 * are nearly equal; a sine is curved, so they differ. */
static u8 meas_slope_cv(const u16 *f, u16 n, const meas_cross_t *cr,
                        u16 vmin, u16 vmax, float *cv)
{
    u16 c, k, j, i;
    u16 span;
    u16 idxs[4];
    float slopes[3];
    float sum = 0.0f;
    u16 seg = 0U;

    if (cr->count < 2U)
    {
        return 0U;
    }

    span = (u16)(vmax - vmin);
    if (span < 20U)
    {
        return 0U;
    }

    for (c = 0U; c < cr->count && c < 8U; c++)
    {
        k = cr->idx[c];

        /* back up to the start of this rising edge (<= 5% of span) */
        while (k > 0U && (u32)f[k] > (u32)(vmin + span / 20U))
        {
            k--;
        }

        j = k;
        for (i = 0U; i < 4U; i++)
        {
            u16 lim = (u16)(vmin + (u16)((span * (i + 1U)) / 5U));
            while (j + 1U < n && f[j] < lim)
            {
                j++;
            }
            idxs[i] = j;
        }

        if (idxs[1] <= idxs[0] + 2U ||
            idxs[2] <= idxs[1] + 2U ||
            idxs[3] <= idxs[2] + 2U)
        {
            continue;           /* square / too few points */
        }

        if (meas_fit_slope(f, idxs[0], idxs[1], &slopes[0]) &&
            meas_fit_slope(f, idxs[1], idxs[2], &slopes[1]) &&
            meas_fit_slope(f, idxs[2], idxs[3], &slopes[2]))
        {
            float mean = (slopes[0] + slopes[1] + slopes[2]) / 3.0f;
            float var;

            if (fabsf(mean) < 1.0e-6f)
            {
                continue;
            }
            var = (slopes[0] - mean) * (slopes[0] - mean) +
                  (slopes[1] - mean) * (slopes[1] - mean) +
                  (slopes[2] - mean) * (slopes[2] - mean);
            var = sqrtf(var / 3.0f) / fabsf(mean);
            sum += var;
            seg++;
        }
    }

    if (seg < 2U)
    {
        return 0U;
    }

    *cv = sum / (float)seg;
    return 1U;
}

/* Single-bin Goertzel DFT (amplitude, LSB). */
static float meas_goertzel(const u16 *f, u16 n, float fs,
                           float freq, float dc)
{
    float w, coeff, q1 = 0.0f, q2 = 0.0f, s, p;
    u16 i;

    if (freq < 20.0f || freq >= fs * 0.5f)
    {
        return 0.0f;
    }

    w = 2.0f * MEAS_PI * freq / fs;
    coeff = 2.0f * cosf(w);

    for (i = 0U; i < n; i++)
    {
        s  = ((float)f[i] - dc) + coeff * q1 - q2;
        q2 = q1;
        q1 = s;
    }

    p = q1 * q1 + q2 * q2 - coeff * q1 * q2;
    if (p < 0.0f)
    {
        p = 0.0f;
    }
    return (2.0f * sqrtf(p)) / (float)n;
}

/* Hann-windowed 2048-point FFT, peak bin + parabolic interpolation.
 * The auto time base keeps about 8 cycles in the captured frame, so
 * the peak is well separated and the interpolation is accurate. */
static float meas_fft_freq(const u16 *f, u16 n, float fs, float mean)
{
    u16 i, k, km = 0U;
    float maxp = 0.0f;
    float p0, p1, p2, den, delta, freq;
    const arm_cfft_instance_f32 *S = &arm_cfft_sR_f32_len2048;

    if (n != MEAS_FFT_N)
    {
        return 0.0f;
    }

    for (i = 0U; i < MEAS_FFT_N; i++)
    {
        float w = 0.5f - 0.5f * arm_cos_f32(2.0f * MEAS_PI *
                                            (float)i /
                                            (float)(MEAS_FFT_N - 1U));
        s_fft_buf[2U * i]      = ((float)f[i] - mean) * w;
        s_fft_buf[2U * i + 1U] = 0.0f;
    }

    arm_cfft_f32(S, s_fft_buf, 0U, 1U);

    for (k = 2U; k < MEAS_FFT_N / 2U - 1U; k++)
    {
        float re = s_fft_buf[2U * k];
        float im = s_fft_buf[2U * k + 1U];
        float p  = re * re + im * im;
        if (p > maxp)
        {
            maxp = p;
            km = k;
        }
    }

    if (km < 2U || km >= MEAS_FFT_N / 2U - 1U)
    {
        return 0.0f;
    }

    p0 = sqrtf(s_fft_buf[2U * (km - 1U)] * s_fft_buf[2U * (km - 1U)] +
              s_fft_buf[2U * (km - 1U) + 1U] * s_fft_buf[2U * (km - 1U) + 1U]);
    p1 = sqrtf(s_fft_buf[2U * km] * s_fft_buf[2U * km] +
              s_fft_buf[2U * km + 1U] * s_fft_buf[2U * km + 1U]);
    p2 = sqrtf(s_fft_buf[2U * (km + 1U)] * s_fft_buf[2U * (km + 1U)] +
              s_fft_buf[2U * (km + 1U) + 1U] * s_fft_buf[2U * (km + 1U) + 1U]);

    den = p0 - 2.0f * p1 + p2;
    if (fabsf(den) < 1.0e-6f)
    {
        delta = 0.0f;
    }
    else
    {
        delta = 0.5f * (p0 - p2) / den;
    }
    if (delta > 0.5f)  delta = 0.5f;
    if (delta < -0.5f) delta = -0.5f;

    freq = ((float)km + delta) * fs / (float)MEAS_FFT_N;
    if (freq < 10.0f || freq > fs * 0.5f)
    {
        return 0.0f;
    }
    return freq;
}

u8 MEAS_Harmonics(const u16 *f, u16 n, float fs, float f0,
                  u8 count, float *amp, float *thd_percent)
{
    u16 i;
    u16 h;
    float mean = 0.0f;
    float wsum = 0.0f;
    float h1;
    float sum2 = 0.0f;
    const arm_cfft_instance_f32 *S = &arm_cfft_sR_f32_len2048;

    if (f == 0 || amp == 0 || thd_percent == 0) return 0U;
    if (n != MEAS_FFT_N || count == 0U || count > 5U) return 0U;
    if (f0 < 500.0f || f0 > 5500.0f) return 0U;
    if (fs < (2.2f * f0 * (float)count)) return 0U;   /* Nyquist guard */

    for (i = 0U; i < MEAS_FFT_N; i++) mean += (float)f[i];
    mean /= (float)MEAS_FFT_N;

    for (i = 0U; i < MEAS_FFT_N; i++)
    {
        float w = 0.5f - 0.5f * arm_cos_f32(2.0f * MEAS_PI *
                                            (float)i /
                                            (float)(MEAS_FFT_N - 1U));
        s_fft_buf[2U * i]      = ((float)f[i] - mean) * w;
        s_fft_buf[2U * i + 1U] = 0.0f;
        wsum += w;
    }

    arm_cfft_f32(S, s_fft_buf, 0U, 1U);

    for (h = 0U; h < count; h++)
    {
        float bin_f = f0 * (float)(h + 1U) * (float)MEAS_FFT_N / fs;
        u16 center = (u16)(bin_f + 0.5f);
        u16 lo;
        u16 hi;
        u16 k;
        float best = 0.0f;

        if (center < 2U) center = 2U;
        if (center > (u16)(MEAS_FFT_N / 2U - 2U))
            center = (u16)(MEAS_FFT_N / 2U - 2U);

        lo = (center > 2U) ? (u16)(center - 2U) : 2U;
        hi = (u16)(center + 2U);
        if (hi > (u16)(MEAS_FFT_N / 2U - 2U))
            hi = (u16)(MEAS_FFT_N / 2U - 2U);

        for (k = lo; k <= hi; k++)
        {
            float re = s_fft_buf[2U * k];
            float im = s_fft_buf[2U * k + 1U];
            float mag = sqrtf(re * re + im * im);
            if (mag > best) best = mag;
        }

        /* One-sided amplitude: 2*|X| / sum(window). */
        amp[h] = (2.0f * best / wsum) * MEAS_VREF / MEAS_ADC_FS;
        if (amp[h] < 0.0f) amp[h] = 0.0f;
    }

    h1 = amp[0];
    for (h = 1U; h < count; h++) sum2 += amp[h] * amp[h];

    if (h1 < 0.001f)
    {
        *thd_percent = 0.0f;
        return 0U;
    }

    *thd_percent = sqrtf(sum2) / h1 * 100.0f;
    return 1U;
}

static float meas_fft_cached(const u16 *f, u16 n, float fs, float mean)
{
    u8 slot;

    if (n != MEAS_FFT_N)
    {
        return 0.0f;
    }

    if (s_fft_key[0] == f)
    {
        slot = 0U;
    }
    else if (s_fft_key[1] == f)
    {
        slot = 1U;
    }
    else
    {
        slot = (s_fft_key[0] == 0) ? 0U : 1U;
        s_fft_key[slot]   = f;
        s_fft_valid[slot] = 0U;
        s_fft_tick[slot]  = 0U;
    }

    if (!s_fft_valid[slot] || fs != s_fft_fs[slot] ||
        s_fft_tick[slot] >= 3U)
    {
        s_fft_freq[slot]  = meas_fft_freq(f, n, fs, mean);
        s_fft_fs[slot]    = fs;
        s_fft_tick[slot]  = 0U;
        s_fft_valid[slot] = 1U;
    }
    else
    {
        s_fft_tick[slot]++;
    }
    return s_fft_freq[slot];
}

void MEAS_Compute(const u16 *frame, u16 n, float fs, MEAS_Result *r)
{
    u16 i;
    u16 min = 4095U;
    u16 max = 0U;
    u32 sum = 0U;
    unsigned long long sumsq = 0ULL;
    u16 span;
    u16 level;
    u16 hyst;
    u16 edge = 0U;
    u16 lo_cnt = 0U;
    u16 hi_cnt = 0U;
    float vdc;
    float vrms;
    float crest = 0.0f;
    float thd = 0.0f;
    float h1 = 0.0f, h3 = 0.0f, h5 = 0.0f;
    float cv = 0.0f;
    u8 cv_ok;
    meas_cross_t cr;

    r->freq = 0.0f;
    r->vpp  = 0.0f;
    r->vmax = 0.0f;
    r->vmin = 0.0f;
    r->dc   = 0.0f;
    r->vdc_bias = 0.0f;
    r->vrms_ac = 0.0f;
    r->vrms_total = 0.0f;
    r->type = 0U;
    r->clipped = 0U;
    r->conf = 0U;

    if (frame == 0 || n == 0U)
    {
        return;
    }

    for (i = 0U; i < n; i++)
    {
        u16 v = frame[i];
        if (v < min) min = v;
        if (v > max) max = v;
        if (v <= 2U)     lo_cnt++;
        if (v >= 4093U)  hi_cnt++;
        sum  += v;
        sumsq += (unsigned long long)v * v;
    }

    span = (u16)(max - min);
    r->dc  = ((float)sum / (float)n) * 3.3f / 4096.0f;
    r->vmax = (float)max * 3.3f / 4096.0f;
    r->vmin = (float)min * 3.3f / 4096.0f;
    r->vpp  = (float)span * 3.3f / 4096.0f;
    r->vdc_bias = ((float)max + (float)min) * 0.5f * 3.3f / 4096.0f;

    if (span < 40U)
    {
        r->type = 3U;           /* DC */
        r->conf = 2U;
        r->vrms_total = fabsf(r->dc);
        s_fft_valid[0] = 0U;
        s_fft_valid[1] = 0U;    /* do not reuse an old AC reading */
        return;
    }

    vdc = (float)sum / (float)n;
    vrms = sqrtf(((float)sumsq / (float)n) - vdc * vdc);
    if (vrms < 0.0f)
    {
        vrms = 0.0f;
    }

    /* DC and AC are reported separately: the vertical centre uses dc,
     * the vertical range uses vpp/vrms_ac, never their sum. */
    r->vrms_total = sqrtf((float)sumsq / (float)n) * MEAS_VREF / MEAS_ADC_FS;
    r->vrms_ac    = vrms * MEAS_VREF / MEAS_ADC_FS;

    if (vrms > 1.0f)
    {
        crest = ((float)span * 0.5f) / vrms;
    }

    level = (u16)((min + max) / 2U);
    hyst  = (u16)(span / 16U);
    if (hyst < 4U)
    {
        hyst = 4U;
    }

    meas_crossings(frame, n, level, hyst, fs, &cr);
    if (cr.count >= 2U && cr.last_t > cr.first_t)
    {
        r->freq = ((float)(cr.count - 1U)) / (cr.last_t - cr.first_t);
        if (r->freq < 20.0f || r->freq > 60000.0f)
        {
            r->freq = 0.0f;
        }
    }

    /* FFT refinement: the auto time base leaves about 8 cycles in the
     * frame, so the Hann-windowed peak interpolation is sub-bin.  Only
     * accept it when it agrees with the zero-crossing estimate; that
     * keeps noise/harmonic peaks from stealing the reading. */
    {
        float ff = meas_fft_cached(frame, n, fs, vdc);

        if (ff > 0.0f)
        {
            if (r->freq <= 0.0f)
            {
                r->freq = ff;
            }
            else if (fabsf(ff - r->freq) <= r->freq * 0.12f)
            {
                r->freq = ff;
            }
        }
    }

    if (r->freq > 0.0f)
    {
        h1 = meas_goertzel(frame, n, fs, r->freq, vdc);
        h3 = meas_goertzel(frame, n, fs, r->freq * 3.0f, vdc);
        h5 = meas_goertzel(frame, n, fs, r->freq * 5.0f, vdc);
        if (h1 > 1.0f)
        {
            thd = sqrtf(h3 * h3 + h5 * h5) / h1;
        }
    }

    for (i = 0U; i < n; i++)
    {
        if (frame[i] < (u16)(min + span / 10U) ||
            frame[i] > (u16)(max - span / 10U))
        {
            edge++;
        }
    }

    cv_ok = meas_slope_cv(frame, n, &cr, min, max, &cv);

    /* 1 sine, 2 square, 3 DC, 4 triangle.
     * conf=2 means the features are well inside the class region, so a
     * single frame may switch the state machine; conf=1 needs two
     * consecutive frames to avoid flapping at a threshold boundary. */
    if (crest < 1.20f || thd > 0.28f || edge > (u16)(n * 3U / 5U))
    {
        r->type = 2U;           /* flat tops / high THD: square */
        if (crest < 1.10f || thd > 0.32f || edge > (u16)(n * 3U / 4U))
        {
            r->conf = 2U;
        }
        else
        {
            r->conf = 1U;
        }
    }
    else if (cv_ok && cv < 0.030f && crest > 1.45f)
    {
        r->type = 4U;           /* linear rising edge: triangle */
        if (cv < 0.015f && crest > 1.60f)
        {
            r->conf = 2U;
        }
        else
        {
            r->conf = 1U;
        }
    }
    else if (crest > 1.22f && crest < 1.58f && thd < 0.08f)
    {
        r->type = 1U;           /* low THD, crest ~1.414: sine */
        if (crest > 1.30f && crest < 1.50f && thd < 0.04f)
        {
            r->conf = 2U;
        }
        else
        {
            r->conf = 1U;
        }
    }
    else if (crest > 1.55f)
    {
        r->type = 4U;           /* rounded triangle fallback */
        r->conf = 1U;
    }
    else if (thd < 0.12f)
    {
        r->type = 1U;
        r->conf = 1U;
    }
    else
    {
        r->type = 0U;
        r->conf = 0U;
    }

    /* Positive-only input: a sine/triangle that sits on 0 V for a large
     * part of the frame has its negative peaks clipped by the ADC.  A
     * square wave with a 0 V low level is legitimate, so skip type 2. */
    if (r->type != 2U && r->type != 3U)
    {
        if (lo_cnt > (u16)(n / 20U) || hi_cnt > (u16)(n / 20U))
        {
            r->clipped = 1U;
        }
    }
}

/* Phase difference between two simultaneously sampled channels.
 * A Hann-windowed single-bin correlation at the fundamental frequency
 * is insensitive to DC offset and does not depend on the trigger point. */
float MEAS_Phase(const u16 *frame1, const u16 *frame2, u16 n,
                 float fs, float freq)
{
    u16 i;
    float mean1 = 0.0f;
    float mean2 = 0.0f;
    float i1 = 0.0f, q1 = 0.0f;
    float i2 = 0.0f, q2 = 0.0f;
    float ph1, ph2, d;
    float w;

    if (frame1 == 0 || frame2 == 0 || n < 16U ||
        fs <= 0.0f || freq < 20.0f || freq >= fs * 0.5f)
    {
        return -1.0f;
    }

    for (i = 0; i < n; i++)
    {
        mean1 += (float)frame1[i];
        mean2 += (float)frame2[i];
    }
    mean1 /= (float)n;
    mean2 /= (float)n;

    w = 2.0f * MEAS_PI * freq / fs;

    for (i = 0; i < n; i++)
    {
        float win = 0.5f - 0.5f *
                    cosf(2.0f * MEAS_PI * (float)i / (float)(n - 1U));
        float ang = w * (float)i;
        float c = cosf(ang);
        float s = sinf(ang);
        float a = ((float)frame1[i] - mean1) * win;
        float b = ((float)frame2[i] - mean2) * win;

        i1 += a * c;
        q1 -= a * s;
        i2 += b * c;
        q2 -= b * s;
    }

    ph1 = atan2f(q1, i1);
    ph2 = atan2f(q2, i2);

    /* Phase by which CH2 leads CH1, 0..360 deg.
     * With the signal generator setting 60 deg this displays 60,
     * not the opposite 300 deg. */
    d = ph2 - ph1;
    while (d < 0.0f)            d += 2.0f * MEAS_PI;
    while (d >= 2.0f * MEAS_PI) d -= 2.0f * MEAS_PI;

    return d * 180.0f / MEAS_PI;
}

const char *MEAS_TypeName(u8 type)
{
    switch (type)
    {
        case 1: return "SINE";
        case 2: return "SQUARE";
        case 3: return "DC";
        case 4: return "TRIANGLE";
        default: return "UNKNOWN";
    }
}

/* ------------------------------------------------------------------ */
/* Waveform type state machine                                       */
/* ------------------------------------------------------------------ */
static u8 s_fsm_type  = 0U;
static u8 s_fsm_cand  = 0U;
static u8 s_fsm_count = 0U;

void MEAS_TypeFSMReset(void)
{
    s_fsm_type  = 0U;
    s_fsm_cand  = 0U;
    s_fsm_count = 0U;
}

u8 MEAS_TypeFSM(u8 candidate, u8 conf)
{
    if (candidate == s_fsm_type)
    {
        s_fsm_cand  = candidate;
        s_fsm_count = 0U;
        return s_fsm_type;
    }

    if (candidate == s_fsm_cand)
    {
        if (s_fsm_count < 255U)
        {
            s_fsm_count++;
        }
    }
    else
    {
        s_fsm_cand  = candidate;
        s_fsm_count = 1U;
    }

    /* Strong evidence switches in one frame; marginal evidence needs
     * two consecutive frames.  A one-frame outlier never changes the
     * displayed type. */
    if (conf >= 2U || s_fsm_count >= 2U)
    {
        s_fsm_type  = candidate;
        s_fsm_count = 0U;
    }

    return s_fsm_type;
}
