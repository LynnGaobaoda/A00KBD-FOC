
#include "MyProject.h"


/******************************************************************************/
float shaft_angle;//!< current motor angle
float electrical_angle;
float shaft_velocity;
float current_sp;
float shaft_velocity_sp;
float shaft_angle_sp;
DQVoltage_s voltage;


TorqueControlType torque_controller;//转矩Torque的控制方式
MotionControlType controller;//运动Motion的控制方式

float sensor_offset=0;
float zero_electric_angle;
/******************************************************************************/
// shaft angle calculation
float shaftAngle(void)
{
  // if no sensor linked return previous value ( for open loop )
  //if(!sensor) return shaft_angle;
  return sensor_direction*getAngle();
}
// shaft velocity calculation
float shaftVelocity(void)
{
  // if no sensor linked return previous value ( for open loop )
  //if(!sensor) return shaft_velocity;
  return sensor_direction*LPF_velocity(getVelocity());
}
/******************************************************************************/
float electricalAngle(void)
{
  return _normalizeAngle((shaft_angle) * pole_pairs - zero_electric_angle);
}
/******************************************************************************/


