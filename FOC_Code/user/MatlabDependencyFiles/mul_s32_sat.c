/*
 * File: mul_s32_sat.c
 *
 * Code generated for Simulink model 'FocCtrl'.
 *
 * Model version                  : 1.251
 * Simulink Coder version         : 9.2 (R2019b) 18-Jul-2019
 * C/C++ source code generated on : Mon Nov 18 19:27:30 2024
 */

#include "rtwtypes.h"
#include "mul_wide_s32.h"
#include "mul_s32_sat.h"

sint32 mul_s32_sat(sint32 a, sint32 b)
{
  sint32 result;
  uint32 u32_chi;
  uint32 u32_clo;
  mul_wide_s32(a, b, &u32_chi, &u32_clo);
  if (((sint32)u32_chi > 0) || ((u32_chi == 0U) && (u32_clo >= 2147483648U))) {
    result = MAX_int32_T;
  } else if (((sint32)u32_chi < -1) || (((sint32)u32_chi == -1) && (u32_clo <
               2147483648U))) {
    result = MIN_int32_T;
  } else {
    result = (sint32)u32_clo;
  }

  return result;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
