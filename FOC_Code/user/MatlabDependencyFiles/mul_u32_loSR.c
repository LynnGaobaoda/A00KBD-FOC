/*
 * File: mul_u32_loSR.c
 *
 * Code generated for Simulink model 'FocCtrl'.
 *
 * Model version                  : 1.363
 * Simulink Coder version         : 9.2 (R2019b) 18-Jul-2019
 * C/C++ source code generated on : Thu Nov 21 20:47:05 2024
 */

#include "rtwtypes.h"
#include "mul_wide_u32.h"
#include "mul_u32_loSR.h"

uint32 mul_u32_loSR(uint32 a, uint32 b, uint32 aShift)
{
  uint32 result;
  uint32 u32_chi;
  mul_wide_u32(a, b, &u32_chi, &result);
  return u32_chi << (32U - aShift) | result >> aShift;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
