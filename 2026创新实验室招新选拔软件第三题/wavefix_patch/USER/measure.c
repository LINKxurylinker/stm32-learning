#include "measure.h"
#include <math.h>

#define MEAS_PI       3.14159265358979f
#define MEAS_MAX_CR   64U

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
    r->dc   = 0.0f;
    r->type = 0U;

    if (frame == 0 || n == 0U)
    {
        return;
    }

    for (i = 0U; i < n; i++)
    {
        u16 v = frame[i];
        if (v < min) min = v;
        if (v > max) max = v;
        sum  += v;
        sumsq += (unsigned long long)v * v;
    }

    span = (u16)(max - min);
    r->dc  = ((float)sum / (float)n) * 3.3f / 4096.0f;
    r->vpp = (float)span * 3.3f / 4096.0f;

    if (span < 40U)
    {
        r->type = 3U;           /* DC */
        return;
    }

    vdc = (float)sum / (float)n;
    vrms = sqrtf(((float)sumsq / (float)n) - vdc * vdc);
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

    /* 1 sine, 2 square, 3 DC, 4 triangle */
    if (crest < 1.20f || thd > 0.28f || edge > (u16)(n * 3U / 5U))
    {
        r->type = 2U;           /* flat tops / high THD: square */
    }
    else if (cv_ok && cv < 0.030f && crest > 1.45f)
    {
        r->type = 4U;           /* linear rising edge: triangle */
    }
    else if (crest > 1.22f && crest < 1.58f && thd < 0.08f)
    {
        r->type = 1U;           /* low THD, crest ~1.414: sine */
    }
    else if (crest > 1.55f)
    {
        r->type = 4U;           /* rounded triangle fallback */
    }
    else if (thd < 0.12f)
    {
        r->type = 1U;
    }
    else
    {
        r->type = 0U;
    }
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
