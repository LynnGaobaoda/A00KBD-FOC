/*
 * File: FocCtrl.h
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

#ifndef RTW_HEADER_FocCtrl_h_
#define RTW_HEADER_FocCtrl_h_
#include <math.h>
#ifndef FocCtrl_COMMON_INCLUDES_
# define FocCtrl_COMMON_INCLUDES_
#include "rtwtypes.h"
#include "Rte_FocCtrl.h"
#endif                                 /* FocCtrl_COMMON_INCLUDES_ */

/* Macros for accessing real-time model data structure */

/* Block states (default storage) for system '<Root>' */
typedef struct tag_DW_FocCtrl_T {
  float64 Delay_DSTATE;                /* '<S5>/Delay' */
  float64 Delay1_DSTATE;               /* '<S5>/Delay1' */
} DW_FocCtrl_T;

/* Constant parameters (default storage) */
typedef struct {
  /* Expression: [2 6 1 4 3 5]
   * Referenced by: '<S9>/Constant4'
   */
  float64 Constant4_Value[6];
} ConstP_FocCtrl_T;

/* Block states (default storage) */
extern DW_FocCtrl_T FocCtrl_DW;

/* Constant parameters (default storage) */
extern const ConstP_FocCtrl_T FocCtrl_ConstP;

/* Exported data declaration */

/* Volatile memory section */
/* Declaration for custom storage class: Volatile */
extern volatile float64 Itest;         /* Referenced by: '<S2>/Constant2' */
extern volatile float64 PosKd;         /* Referenced by: '<S5>/Kp2' */
extern volatile float64 PosKi;         /* Referenced by: '<S5>/Kp1' */
extern volatile float64 PosKp;         /* Referenced by: '<S5>/Kp4' */
extern volatile float64 Udc;           /* Referenced by: '<S8>/Gain2' */

/*-
 * The generated code includes comments that allow you to trace directly
 * back to the appropriate location in the model.  The basic format
 * is <system>/block_name, where system is the system number (uniquely
 * assigned by Simulink) and block_name is the name of the block.
 *
 * Use the MATLAB hilite_system command to trace the generated code back
 * to the model.  For example,
 *
 * hilite_system('<S3>')    - opens system 3
 * hilite_system('<S3>/Kp') - opens and selects block Kp which resides in S3
 *
 * Here is the system hierarchy for this model
 *
 * '<Root>' : 'FocCtrl'
 * '<S1>'   : 'FocCtrl/FocCtrl_Init'
 * '<S2>'   : 'FocCtrl/Runnable_1ms_sys'
 * '<S3>'   : 'FocCtrl/Runnable_1ms_sys/Anti_Park'
 * '<S4>'   : 'FocCtrl/Runnable_1ms_sys/Clark'
 * '<S5>'   : 'FocCtrl/Runnable_1ms_sys/PD_Pos'
 * '<S6>'   : 'FocCtrl/Runnable_1ms_sys/PI_Speed'
 * '<S7>'   : 'FocCtrl/Runnable_1ms_sys/Park'
 * '<S8>'   : 'FocCtrl/Runnable_1ms_sys/SVPWM'
 * '<S9>'   : 'FocCtrl/Runnable_1ms_sys/SVPWM/GetSector'
 * '<S10>'  : 'FocCtrl/Runnable_1ms_sys/SVPWM/MATLAB Function2'
 * '<S11>'  : 'FocCtrl/Runnable_1ms_sys/SVPWM/MATLAB Function3'
 */
#endif                                 /* RTW_HEADER_FocCtrl_h_ */

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
