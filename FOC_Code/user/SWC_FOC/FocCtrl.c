/*
 * File: FocCtrl.c
 *
 * Code generated for Simulink model 'FocCtrl'.
 *
 * Model version                  : 1.273
 * Simulink Coder version         : 9.2 (R2019b) 18-Jul-2019
 * C/C++ source code generated on : Sun Dec  8 15:32:56 2024
 *
 * Target selection: autosar.tlc
 * Embedded hardware selection: Custom Processor->Custom Processor
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */

#include "FocCtrl.h"
#ifndef UCHAR_MAX
#include <limits.h>
#endif

#if ( UCHAR_MAX != (0xFFU) ) || ( SCHAR_MAX != (0x7F) )
#error Code was generated for compiler with different sized uchar/char. \
Consider adjusting Test hardware word size settings on the \
Hardware Implementation pane to match your compiler word sizes as \
defined in limits.h of the compiler. Alternatively, you can \
select the Test hardware is the same as production hardware option and \
select the Enable portable word sizes option on the Code Generation > \
Verification pane for ERT based targets, which will disable the \
preprocessor word size checks.
#endif

#if ( USHRT_MAX != (0xFFFFU) ) || ( SHRT_MAX != (0x7FFF) )
#error Code was generated for compiler with different sized ushort/short. \
Consider adjusting Test hardware word size settings on the \
Hardware Implementation pane to match your compiler word sizes as \
defined in limits.h of the compiler. Alternatively, you can \
select the Test hardware is the same as production hardware option and \
select the Enable portable word sizes option on the Code Generation > \
Verification pane for ERT based targets, which will disable the \
preprocessor word size checks.
#endif

#if ( UINT_MAX != (0xFFFFFFFFU) ) || ( INT_MAX != (0x7FFFFFFF) )
#error Code was generated for compiler with different sized uint/int. \
Consider adjusting Test hardware word size settings on the \
Hardware Implementation pane to match your compiler word sizes as \
defined in limits.h of the compiler. Alternatively, you can \
select the Test hardware is the same as production hardware option and \
select the Enable portable word sizes option on the Code Generation > \
Verification pane for ERT based targets, which will disable the \
preprocessor word size checks.
#endif

#if ( ULONG_MAX != (0xFFFFFFFFU) ) || ( LONG_MAX != (0x7FFFFFFF) )
#error Code was generated for compiler with different sized ulong/long. \
Consider adjusting Test hardware word size settings on the \
Hardware Implementation pane to match your compiler word sizes as \
defined in limits.h of the compiler. Alternatively, you can \
select the Test hardware is the same as production hardware option and \
select the Enable portable word sizes option on the Code Generation > \
Verification pane for ERT based targets, which will disable the \
preprocessor word size checks.
#endif

/* Exported data definition */

/* Volatile memory section */
/* Definition for custom storage class: Volatile */
volatile float64 Itest = 0.0;          /* Referenced by: '<S2>/Constant2' */
volatile float64 PosKd = 0.001;        /* Referenced by: '<S5>/Kp2' */
volatile float64 PosKi = 0.0;          /* Referenced by: '<S5>/Kp1' */
volatile float64 PosKp = 0.01;         /* Referenced by: '<S5>/Kp4' */
volatile float64 Udc = 10.0;           /* Referenced by: '<S8>/Gain2' */

/* Constant parameters (default storage) */
const ConstP_FocCtrl_T FocCtrl_ConstP = {
  /* Expression: [2 6 1 4 3 5]
   * Referenced by: '<S9>/Constant4'
   */
  { 2.0, 6.0, 1.0, 4.0, 3.0, 5.0 }
};

/* Block states (default storage) */
DW_FocCtrl_T FocCtrl_DW;

/* Model step function */
void Runnable_1ms(void)
{
  float64 L_Duty1;
  sint32 rtb_CS_MachRad_Operation;
  float64 rtb_Add_n;
  uint32 rtb_CS_ElecRad_Operation;
  float64 rtb_Switch1_e;
  sint32 rtb_CS_MachRadVelocity_Operation;
  float64 rtb_Add3;
  float64 rtb_Saturation2;
  sint32 rtb_CS_PhaseCur_Operation[2];
  uint16 tmp[3];
  float64 rtb_Gain3_idx_1;

  /* Outputs for Atomic SubSystem: '<Root>/Runnable_1ms_sys' */
  /* FunctionCaller: '<S2>/CS_MachRad_Operation' */
  Rte_Call_CS_MachRad_Operation(&rtb_CS_MachRad_Operation);

  /* Sum: '<S2>/Add3' incorporates:
   *  Constant: '<S2>/Constant2'
   *  Gain: '<S2>/Gain6'
   *  Gain: '<S2>/Gain7'
   */
  rtb_Add3 = Itest - 0.001 * (float64)rtb_CS_MachRad_Operation;

  /* Outputs for Atomic SubSystem: '<S2>/PD_Pos' */
  /* Sum: '<S5>/Add' incorporates:
   *  Constant: '<S5>/Constant'
   *  Delay: '<S5>/Delay'
   *  Gain: '<S5>/Kp1'
   *  Product: '<S5>/Divide1'
   */
  FocCtrl_DW.Delay_DSTATE += rtb_Add3 / 1000.0 * PosKi;

  /* Saturate: '<S5>/Saturation1' */
  if (FocCtrl_DW.Delay_DSTATE > 3.0) {
    /* Sum: '<S5>/Add' */
    FocCtrl_DW.Delay_DSTATE = 3.0;
  } else {
    if (FocCtrl_DW.Delay_DSTATE < -3.0) {
      /* Sum: '<S5>/Add' */
      FocCtrl_DW.Delay_DSTATE = -3.0;
    }
  }

  /* End of Saturate: '<S5>/Saturation1' */

  /* Sum: '<S5>/Sum6' incorporates:
   *  Constant: '<S5>/Constant'
   *  Delay: '<S5>/Delay'
   *  Delay: '<S5>/Delay1'
   *  Gain: '<S5>/Kp2'
   *  Gain: '<S5>/Kp4'
   *  Product: '<S5>/Divide'
   *  Sum: '<S5>/Add1'
   */
  rtb_Saturation2 = (rtb_Add3 - FocCtrl_DW.Delay1_DSTATE) * 1000.0 * PosKd +
    (PosKp * rtb_Add3 + FocCtrl_DW.Delay_DSTATE);

  /* Saturate: '<S5>/Saturation2' */
  if (rtb_Saturation2 > 3.0) {
    rtb_Saturation2 = 3.0;
  } else {
    if (rtb_Saturation2 < -3.0) {
      rtb_Saturation2 = -3.0;
    }
  }

  /* End of Saturate: '<S5>/Saturation2' */

  /* Update for Delay: '<S5>/Delay1' */
  FocCtrl_DW.Delay1_DSTATE = rtb_Add3;

  /* End of Outputs for SubSystem: '<S2>/PD_Pos' */

  /* FunctionCaller: '<S2>/CS_ElecRad_Operation' */
  Rte_Call_CS_ElecRad_Operation(&rtb_CS_ElecRad_Operation);

  /* Gain: '<S2>/Gain3' incorporates:
   *  Gain: '<S2>/Gain2'
   */
  rtb_Add_n = 0.001 * (float64)rtb_CS_ElecRad_Operation;

  /* Fcn: '<S3>/Fcn' incorporates:
   *  Constant: '<S2>/Constant1'
   */
  rtb_Add3 = 0.0 - rtb_Saturation2 * sin(rtb_Add_n);

  /* Fcn: '<S3>/Fcn1' incorporates:
   *  Fcn: '<S3>/Fcn'
   */
  rtb_Add_n = rtb_Saturation2 * cos(rtb_Add_n);

  /* Fcn: '<S8>/Fcn1' */
  rtb_Switch1_e = 0.8660254037844386 * rtb_Add3 - rtb_Add_n / 2.0;

  /* Fcn: '<S8>/Fcn5' */
  rtb_Add3 = -0.8660254037844386 * rtb_Add3 - rtb_Add_n / 2.0;

  /* Gain: '<S8>/Gain2' */
  L_Duty1 = 0.57735026918962573 * Udc;

  /* Gain: '<S8>/Gain3' incorporates:
   *  Gain: '<S8>/Gain2'
   */
  rtb_Gain3_idx_1 = L_Duty1 * rtb_Switch1_e * 10000.0;
  rtb_Saturation2 = L_Duty1 * rtb_Add3 * 10000.0;

  /* Switch: '<S9>/Switch1' incorporates:
   *  Constant: '<S9>/Constant1'
   *  Constant: '<S9>/Constant2'
   */
  if (rtb_Switch1_e > 0.0) {
    rtb_Switch1_e = 2.0;
  } else {
    rtb_Switch1_e = 0.0;
  }

  /* End of Switch: '<S9>/Switch1' */

  /* Switch: '<S9>/Switch2' incorporates:
   *  Constant: '<S9>/Constant1'
   *  Constant: '<S9>/Constant3'
   */
  if (rtb_Add3 > 0.0) {
    rtb_Add3 = 4.0;
  } else {
    rtb_Add3 = 0.0;
  }

  /* End of Switch: '<S9>/Switch2' */

  /* S-Function (sdspperm2): '<S9>/Variable Selector' incorporates:
   *  Fcn: '<S8>/Fcn'
   *  Sum: '<S9>/Add'
   *  Switch: '<S9>/Switch3'
   */
  rtb_CS_MachRad_Operation = (sint32)(float64)(((float64)(rtb_Add_n > 0.0 ? 1 :
    0) + rtb_Switch1_e) + rtb_Add3) - 1;
  if (rtb_CS_MachRad_Operation < 0) {
    rtb_CS_MachRad_Operation = 0;
  } else {
    if (rtb_CS_MachRad_Operation >= 6) {
      rtb_CS_MachRad_Operation = 5;
    }
  }

  /* MATLAB Function: '<S8>/MATLAB Function2' incorporates:
   *  Constant: '<S9>/Constant4'
   *  Fcn: '<S8>/Fcn'
   *  Gain: '<S8>/Gain2'
   *  Gain: '<S8>/Gain3'
   *  S-Function (sdspperm2): '<S9>/Variable Selector'
   */
  /* MATLAB Function 'Runnable_1ms_sys/SVPWM/MATLAB Function2': '<S10>:1' */
  switch ((sint32)FocCtrl_ConstP.Constant4_Value[rtb_CS_MachRad_Operation]) {
   case 1:
    /* '<S10>:1:5' */
    rtb_Add3 = rtb_Gain3_idx_1;

    /* '<S10>:1:6' */
    rtb_Saturation2 = L_Duty1 * rtb_Add_n * 10000.0;
    break;

   case 2:
    /* '<S10>:1:8' */
    rtb_Add3 = -rtb_Saturation2;

    /* '<S10>:1:9' */
    rtb_Saturation2 = -rtb_Gain3_idx_1;
    break;

   case 3:
    /* '<S10>:1:11' */
    rtb_Add3 = L_Duty1 * rtb_Add_n * 10000.0;

    /* '<S10>:1:12' */
    break;

   case 4:
    /* '<S10>:1:14' */
    rtb_Add3 = -rtb_Gain3_idx_1;

    /* '<S10>:1:15' */
    rtb_Saturation2 = -(L_Duty1 * rtb_Add_n * 10000.0);
    break;

   case 5:
    /* '<S10>:1:17' */
    rtb_Add3 = rtb_Saturation2;

    /* '<S10>:1:18' */
    rtb_Saturation2 = rtb_Gain3_idx_1;
    break;

   case 6:
    /* '<S10>:1:20' */
    rtb_Add3 = -(L_Duty1 * rtb_Add_n * 10000.0);

    /* '<S10>:1:21' */
    rtb_Saturation2 = -rtb_Saturation2;
    break;

   default:
    /* '<S10>:1:23' */
    rtb_Add3 = 0.0;

    /* '<S10>:1:24' */
    rtb_Saturation2 = 0.0;
    break;
  }

  /* Sum: '<S8>/Add' incorporates:
   *  MATLAB Function: '<S8>/MATLAB Function2'
   */
  /* '<S10>:1:26' */
  /* '<S10>:1:27' */
  rtb_Add_n = rtb_Add3 + rtb_Saturation2;

  /* Switch: '<S8>/Switch1' incorporates:
   *  Fcn: '<S8>/Fcn6'
   *  Fcn: '<S8>/Fcn7'
   *  MATLAB Function: '<S8>/MATLAB Function2'
   *  Switch: '<S8>/Switch2'
   */
  if (rtb_Add_n > 10000.0) {
    rtb_Add3 = rtb_Add3 / rtb_Add_n * 10000.0;
    rtb_Saturation2 = rtb_Saturation2 / rtb_Add_n * 10000.0;
  }

  /* End of Switch: '<S8>/Switch1' */

  /* Product: '<S8>/Divide1' incorporates:
   *  Constant: '<S8>/7SegPwm'
   *  Constant: '<S8>/PwmWidth'
   *  Sum: '<S8>/Add1'
   */
  rtb_Add_n = ((10000.0 - rtb_Add3) - rtb_Saturation2) / 2.0;

  /* MATLAB Function: '<S8>/MATLAB Function3' incorporates:
   *  Constant: '<S9>/Constant4'
   *  S-Function (sdspperm2): '<S9>/Variable Selector'
   */
  /* MATLAB Function 'Runnable_1ms_sys/SVPWM/MATLAB Function3': '<S11>:1' */
  switch ((sint32)FocCtrl_ConstP.Constant4_Value[rtb_CS_MachRad_Operation]) {
   case 1:
    /* '<S11>:1:5' */
    L_Duty1 = (rtb_Add3 + rtb_Saturation2) + rtb_Add_n;

    /* '<S11>:1:6' */
    rtb_Gain3_idx_1 = rtb_Saturation2 + rtb_Add_n;

    /* '<S11>:1:7' */
    break;

   case 2:
    /* '<S11>:1:9' */
    L_Duty1 = rtb_Add3 + rtb_Add_n;

    /* '<S11>:1:10' */
    rtb_Gain3_idx_1 = (rtb_Add3 + rtb_Saturation2) + rtb_Add_n;

    /* '<S11>:1:11' */
    break;

   case 3:
    /* '<S11>:1:13' */
    L_Duty1 = rtb_Add_n;

    /* '<S11>:1:14' */
    rtb_Gain3_idx_1 = (rtb_Add3 + rtb_Saturation2) + rtb_Add_n;

    /* '<S11>:1:15' */
    rtb_Add_n += rtb_Saturation2;
    break;

   case 4:
    /* '<S11>:1:17' */
    L_Duty1 = rtb_Add_n;

    /* '<S11>:1:18' */
    rtb_Gain3_idx_1 = rtb_Add3 + rtb_Add_n;

    /* '<S11>:1:19' */
    rtb_Add_n += rtb_Add3 + rtb_Saturation2;
    break;

   case 5:
    /* '<S11>:1:21' */
    L_Duty1 = rtb_Saturation2 + rtb_Add_n;

    /* '<S11>:1:22' */
    rtb_Gain3_idx_1 = rtb_Add_n;

    /* '<S11>:1:23' */
    rtb_Add_n += rtb_Add3 + rtb_Saturation2;
    break;

   case 6:
    /* '<S11>:1:25' */
    L_Duty1 = (rtb_Add3 + rtb_Saturation2) + rtb_Add_n;

    /* '<S11>:1:26' */
    rtb_Gain3_idx_1 = rtb_Add_n;

    /* '<S11>:1:27' */
    rtb_Add_n += rtb_Add3;
    break;

   default:
    /* '<S11>:1:29' */
    L_Duty1 = 0.0;

    /* '<S11>:1:30' */
    rtb_Gain3_idx_1 = 0.0;

    /* '<S11>:1:31' */
    rtb_Add_n = 0.0;
    break;
  }

  /* FunctionCaller: '<S2>/CS_MachRadVelocity_Operation' */
  /* '<S11>:1:33' */
  /* '<S11>:1:34' */
  /* '<S11>:1:35' */
  Rte_Call_CS_MachRadVelocity_Operation(&rtb_CS_MachRadVelocity_Operation);

  /* FunctionCaller: '<S2>/CS_PhaseCur_Operation' */
  Rte_Call_CS_PhaseCur_Operation(rtb_CS_PhaseCur_Operation);

  /* SignalConversion generated from: '<S2>/Gain5' incorporates:
   *  Gain: '<S2>/Gain5'
   *  MATLAB Function: '<S8>/MATLAB Function3'
   */
  tmp[0] = (uint16)L_Duty1;
  tmp[1] = (uint16)rtb_Gain3_idx_1;
  tmp[2] = (uint16)rtb_Add_n;

  /* End of Outputs for SubSystem: '<Root>/Runnable_1ms_sys' */

  /* Outport: '<Root>/PP_PhaseDutySet_Element' */
  (void) Rte_Write_PP_PhaseDutySet_Element(tmp);

  /* Outport: '<Root>/PP_FocDrvEbl_Element' */
  (void) Rte_Write_PP_FocDrvEbl_Element((uint8)0U);
}

/* Model initialize function */
void FocCtrl_Init(void)
{
  /* (no initialization code required) */
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
