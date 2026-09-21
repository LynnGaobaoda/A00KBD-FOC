/*
 * File: mul_wide_u32.c
 *
 * Code generated for Simulink model 'FocCtrl'.
 *
 * Model version                  : 1.363
 * Simulink Coder version         : 9.2 (R2019b) 18-Jul-2019
 * C/C++ source code generated on : Thu Nov 21 20:47:05 2024
 */

#include "rtwtypes.h"
#include "mul_wide_u32.h"

void mul_wide_u32(uint32 in0, uint32 in1, uint32 *ptrOutBitsHi, uint32
                  *ptrOutBitsLo)
{
  uint32 outBitsLo;
  uint32 in0Lo;
  uint32 in0Hi;
  uint32 in1Lo;
  uint32 in1Hi;
  uint32 productHiLo;
  uint32 productLoHi;
  in0Hi = in0 >> 16U;
  in0Lo = in0 & 65535U;
  in1Hi = in1 >> 16U;
  in1Lo = in1 & 65535U;
  productHiLo = in0Hi * in1Lo;
  productLoHi = in0Lo * in1Hi;
  in0Lo *= in1Lo;
  in1Lo = 0U;
  outBitsLo = (productLoHi << 16U) + in0Lo;
  if (outBitsLo < in0Lo) {
    in1Lo = 1U;
  }

  in0Lo = outBitsLo;
  outBitsLo += productHiLo << 16U;
  if (outBitsLo < in0Lo) {
    in1Lo++;
  }

  *ptrOutBitsHi = (((productLoHi >> 16U) + (productHiLo >> 16U)) + in0Hi * in1Hi)
    + in1Lo;
  *ptrOutBitsLo = outBitsLo;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
