/*
 * File: div_nde_s32_floor.c
 *
 * Code generated for Simulink model 'FocCtrl'.
 *
 * Model version                  : 1.251
 * Simulink Coder version         : 9.2 (R2019b) 18-Jul-2019
 * C/C++ source code generated on : Mon Nov 18 19:27:30 2024
 */

#include "rtwtypes.h"
#include "div_nde_s32_floor.h"

sint32 div_nde_s32_floor(sint32 numerator, sint32 denominator)
{
  return (((numerator < 0) != (denominator < 0)) && (numerator % denominator !=
           0) ? -1 : 0) + numerator / denominator;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
