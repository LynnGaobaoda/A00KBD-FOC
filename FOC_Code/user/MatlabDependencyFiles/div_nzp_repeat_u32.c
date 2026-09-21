/*
 * File: div_nzp_repeat_u32.c
 *
 * Code generated for Simulink model 'FocCtrl'.
 *
 * Model version                  : 1.363
 * Simulink Coder version         : 9.2 (R2019b) 18-Jul-2019
 * C/C++ source code generated on : Thu Nov 21 20:47:05 2024
 */

#include "rtwtypes.h"
#include "div_nzp_repeat_u32.h"

uint32 div_nzp_repeat_u32(uint32 numerator, uint32 denominator, uint32
  nRepeatSub)
{
  uint32 quotient;
  uint32 iRepeatSub;
  boolean numeratorExtraBit;
  quotient = numerator / denominator;
  numerator %= denominator;
  for (iRepeatSub = 0U; iRepeatSub < nRepeatSub; iRepeatSub++) {
    numeratorExtraBit = (numerator >= 2147483648U);
    numerator <<= 1U;
    quotient <<= 1U;
    if (numeratorExtraBit || (numerator >= denominator)) {
      quotient++;
      numerator -= denominator;
    }
  }

  return quotient;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
