#ifndef __MEASURE_H
#define __MEASURE_H

#include "stm32f4xx.h"

typedef struct
{
    float freq;
    float vpp;       /* AC peak-to-peak = vmax - vmin          */
    float vmax;      /* highest input voltage (V)              */
    float vmin;      /* lowest  input voltage (V)              */
    float dc;        /* true mean (V)                          */
    float vdc_bias;  /* DC bias = (vmax + vmin) / 2 (V)        */
    float vrms_ac;   /* AC RMS, DC removed (V)                 */
    float vrms_total;/* total RMS (V)                          */
    u8    type;      /* 0 unknown, 1 sine, 2 square, 3 DC, 4 triangle */
    u8    clipped;   /* 1: flat top/bottom at 0 V or 3.3 V     */
    u8    conf;      /* 2 = strong candidate, 1 = normal, 0 = none */
} MEAS_Result;

void MEAS_Compute(const u16 *frame, u16 n, float fs, MEAS_Result *r);

/* Phase by which CH2 leads CH1, in degrees, 0..360.
 * Returns -1 when the frequency or signal is invalid. */
float MEAS_Phase(const u16 *frame1, const u16 *frame2, u16 n,
                 float fs, float freq);

/* Harmonic analysis with the same CMSIS-DSP 2048-point CFFT.
 * f0 is the fundamental frequency, amp[0..count-1] receives the
 * fundamental ... count-th harmonic amplitudes in volts, thd_percent
 * receives THD = sqrt(sum(H2..H5)^2) / H1 * 100. */
u8 MEAS_Harmonics(const u16 *frame, u16 n, float fs, float f0,
                  u8 count, float *amp, float *thd_percent);

const char *MEAS_TypeName(u8 type);

/* Waveform type state machine.  Call once per accepted frame. */
u8   MEAS_TypeFSM(u8 candidate, u8 conf);
void MEAS_TypeFSMReset(void);

#endif
