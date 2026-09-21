#ifndef CURRENTSENSE_H
#define CURRENTSENSE_H

/******************************************************************************/
#include "foc_utils.h" 
/******************************************************************************/
float getDCCurrent(float motor_electrical_angle);
DQCurrent_s getFOCCurrents(float angle_el);
/******************************************************************************/

extern PhaseCurrent_s current_ADC;
#endif

