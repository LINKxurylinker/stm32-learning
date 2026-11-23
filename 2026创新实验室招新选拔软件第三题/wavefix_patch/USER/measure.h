#ifndef __MEASURE_H
#define __MEASURE_H

#include "stm32f4xx.h"

typedef struct
{
    float freq;
    float vpp;
    float dc;
    u8    type;      /* 0 unknown, 1 sine, 2 square, 3 DC, 4 triangle */
} MEAS_Result;

void MEAS_Compute(const u16 *frame, u16 n, float fs, MEAS_Result *r);
const char *MEAS_TypeName(u8 type);

#endif
