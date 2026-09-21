#ifndef PID_H
#define PID_H

/******************************************************************************/
void PID_init(void);
float PID_velocity(float error);
float PID_angle(float error);
float PID_current(float error);
float PID_current_my(float error);
/******************************************************************************/

#endif

