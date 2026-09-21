/*
 * File: asr_s32.c
 *
 * Code generated for Simulink model 'FocCtrl'.
 *
 * Model version                  : 1.265
 * Simulink Coder version         : 9.2 (R2019b) 18-Jul-2019
 * C/C++ source code generated on : Mon Nov 18 21:00:12 2024
 */

#include "rtwtypes.h"
#include "asr_s32.h"

sint32 asr_s32(sint32 u, uint32 n)
{
  sint32 y;
  if (u >= 0) {
    y = (sint32)(uint32)((uint32)u >> n);
  } else {
    y = -(sint32)(uint32)((uint32)(sint32)(-1 - u) >> n) - 1;
  }

  return y;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
