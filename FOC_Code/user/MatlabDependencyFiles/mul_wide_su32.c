/*
 * File: mul_wide_su32.c
 *
 * Code generated for Simulink model 'Swc_LedLightLogic'.
 *
 * Model version                  : 1.25
 * Simulink Coder version         : 9.1 (R2019a) 23-Nov-2018
 * C/C++ source code generated on : Tue Oct 26 13:42:18 2021
 */

#include "rtwtypes.h"
#include "mul_wide_su32.h"

void mul_wide_su32(sint32 in0, uint32 in1, uint32 *ptrOutBitsHi, uint32
                   *ptrOutBitsLo)
{
  uint32 outBitsLo;
  uint32 absIn0;
  uint32 in0Hi;
  uint32 in1Lo;
  uint32 in1Hi;
  uint32 productHiLo;
  uint32 productLoHi;
  absIn0 = in0 < 0 ? ~(uint32)in0 + 1U : (uint32)in0;
  in0Hi = absIn0 >> 16U;
  absIn0 &= 65535U;
  in1Hi = in1 >> 16U;
  in1Lo = in1 & 65535U;
  productHiLo = in0Hi * in1Lo;
  productLoHi = absIn0 * in1Hi;
  absIn0 *= in1Lo;
  in1Lo = 0U;
  outBitsLo = (productLoHi << 16U) + absIn0;
  if (outBitsLo < absIn0) {
    in1Lo = 1U;
  }

  absIn0 = outBitsLo;
  outBitsLo += productHiLo << 16U;
  if (outBitsLo < absIn0) {
    in1Lo++;
  }

  absIn0 = (((productLoHi >> 16U) + (productHiLo >> 16U)) + in0Hi * in1Hi) +
    in1Lo;
  if ((in1 != 0U) && (in0 < 0)) {
    absIn0 = ~absIn0;
    outBitsLo = ~outBitsLo;
    outBitsLo++;
    if (outBitsLo == 0U) {
      absIn0++;
    }
  }

  *ptrOutBitsHi = absIn0;
  *ptrOutBitsLo = outBitsLo;
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
