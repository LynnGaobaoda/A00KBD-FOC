/*
 * File: mul_wide_s32.c
 *
 * Code generated for Simulink model 'FocCtrl'.
 *
 * Model version                  : 1.251
 * Simulink Coder version         : 9.2 (R2019b) 18-Jul-2019
 * C/C++ source code generated on : Mon Nov 18 19:27:30 2024
 */

#include "rtwtypes.h"
#include "mul_wide_s32.h"

void mul_wide_s32(sint32 in0, sint32 in1, uint32 *ptrOutBitsHi, uint32
                  *ptrOutBitsLo)
{
  uint32 absIn0;
  uint32 absIn1;
  uint32 in0Lo;
  uint32 in0Hi;
  uint32 in1Hi;
  uint32 productHiLo;
  uint32 productLoHi;
  absIn0 = in0 < 0 ? ~(uint32)in0 + 1U : (uint32)in0;
  absIn1 = in1 < 0 ? ~(uint32)in1 + 1U : (uint32)in1;
  in0Hi = absIn0 >> 16U;
  in0Lo = absIn0 & 65535U;
  in1Hi = absIn1 >> 16U;
  absIn0 = absIn1 & 65535U;
  productHiLo = in0Hi * absIn0;
  productLoHi = in0Lo * in1Hi;
  absIn0 *= in0Lo;
  absIn1 = 0U;
  in0Lo = (productLoHi << 16U) + absIn0;
  if (in0Lo < absIn0) {
    absIn1 = 1U;
  }

  absIn0 = in0Lo;
  in0Lo += productHiLo << 16U;
  if (in0Lo < absIn0) {
    absIn1++;
  }

  absIn0 = (((productLoHi >> 16U) + (productHiLo >> 16U)) + in0Hi * in1Hi) +
    absIn1;
  if ((in0 != 0) && ((in1 != 0) && ((in0 > 0) != (in1 > 0)))) {
    absIn0 = ~absIn0;
    in0Lo = ~in0Lo;
    in0Lo++;
    if (in0Lo == 0U) {
      absIn0++;
    }
  }

  *ptrOutBitsHi = absIn0;
  *ptrOutBitsLo = in0Lo;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
