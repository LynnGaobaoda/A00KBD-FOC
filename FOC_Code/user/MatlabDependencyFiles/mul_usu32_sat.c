/*
 * File: mul_usu32_sat.c
 *
 * Code generated for Simulink model 'Swc_LedLightLogic'.
 *
 * Model version                  : 1.25
 * Simulink Coder version         : 9.1 (R2019a) 23-Nov-2018
 * C/C++ source code generated on : Tue Oct 26 13:42:18 2021
 */

#include "rtwtypes.h"
#include "mul_wide_su32.h"
#include "mul_usu32_sat.h"

uint32 mul_usu32_sat(sint32 a, uint32 b)
{
  uint32 result;
  uint32 u32_chi;
  mul_wide_su32(a, b, &u32_chi, &result);
  if ((sint32)u32_chi >= 0) {
    if (u32_chi != 0U) {
      result = MAX_uint32_T;
    }
  } else {
    result = 0U;
  }

  return result;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
