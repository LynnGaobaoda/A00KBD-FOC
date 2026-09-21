/*
 * File: look1_is32lu32n31Du32_binlcse.c
 *
 * Code generated for Simulink model 'FocCtrl'.
 *
 * Model version                  : 1.363
 * Simulink Coder version         : 9.2 (R2019b) 18-Jul-2019
 * C/C++ source code generated on : Thu Nov 21 20:47:05 2024
 */

#include "rtwtypes.h"
#include "div_nzp_repeat_u32.h"
#include "mul_u32_loSR.h"
#include "look1_is32lu32n31Du32_binlcse.h"

sint32 look1_is32lu32n31Du32_binlcse(sint32 u0, const sint32 bp0[], const
  sint32 table[], uint32 maxIndex)
{
  sint32 y;
  uint32 frac;
  sint32 yR_0d0;
  uint32 iRght;
  uint32 iLeft;

  /* Column-major Lookup 1-D
     Search method: 'binary'
     Use previous index: 'off'
     Interpolation method: 'Linear point-slope'
     Extrapolation method: 'Clip'
     Use last breakpoint for index at or above upper limit: 'off'
     Remove protection against out-of-range input in generated code: 'off'
     Rounding mode: 'simplest'
   */
  /* Prelookup - Index and Fraction
     Index Search method: 'binary'
     Extrapolation method: 'Clip'
     Use previous index: 'off'
     Use last breakpoint for index at or above upper limit: 'off'
     Remove protection against out-of-range input in generated code: 'off'
     Rounding mode: 'simplest'
   */
  if (u0 <= bp0[0U]) {
    iLeft = 0U;
    frac = 0U;
  } else if (u0 < bp0[maxIndex]) {
    /* Binary Search */
    frac = maxIndex >> 1U;
    iLeft = 0U;
    iRght = maxIndex;
    while (iRght - iLeft > 1U) {
      if (u0 < bp0[frac]) {
        iRght = frac;
      } else {
        iLeft = frac;
      }

      frac = (iRght + iLeft) >> 1U;
    }

    frac = div_nzp_repeat_u32((uint32)u0 - (uint32)bp0[iLeft], (uint32)
      bp0[iLeft + 1U] - (uint32)bp0[iLeft], 31U);
  } else {
    iLeft = maxIndex - 1U;
    frac = 2147483648U;
  }

  /* Column-major Interpolation 1-D
     Interpolation method: 'Linear point-slope'
     Use last breakpoint for index at or above upper limit: 'off'
     Rounding mode: 'simplest'
     Overflow mode: 'wrapping'
   */
  yR_0d0 = table[iLeft + 1U];
  if (yR_0d0 >= table[iLeft]) {
    y = (sint32)mul_u32_loSR(frac, (uint32)yR_0d0 - (uint32)table[iLeft],
      31U) + table[iLeft];
  } else {
    y = table[iLeft] - (sint32)mul_u32_loSR(frac, (uint32)table[iLeft] -
      (uint32)yR_0d0, 31U);
  }

  return y;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
