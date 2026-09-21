/*
 * File: div_nzp_usu32.c
 *
 * Code generated for Simulink model 'Swc_LedOutputControl'.
 *
 * Model version                  : 1.11
 * Simulink Coder version         : 9.6 (R2021b) 14-May-2021
 * C/C++ source code generated on : Thu Nov 25 10:29:39 2021
 */

#include "rtwtypes.h"
#include "div_nzp_usu32.h"

uint32 div_nzp_usu32(sint32 numerator, uint32 denominator)
{
  uint32 tempAbsQuotient;
  tempAbsQuotient = (numerator < 0 ? ~(uint32)numerator + 1U : (uint32)
                     numerator) / denominator;
  return numerator < 0 ? (uint32)(sint32)-(sint32)tempAbsQuotient :
    tempAbsQuotient;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
