

#ifndef MYPROJECT_H
#define MYPROJECT_H

/* Includes ------------------------------------------------------------------*/

#include "stm32f10x_it.h" 
#include "usart.h"
#include "delay.h"
#include "timer.h"
#include "iic.h"

#include "MagneticSensor.h" 
#include "foc_utils.h" 
#include "FOCMotor.h" 
#include "BLDCmotor.h" 
#include "lowpass_filter.h" 
#include "pid.h"
#include "adc.h"
#include "CurrentSense.h"


/******************************************************************************/
#define M1_Enable    GPIO_SetBits(GPIOB,GPIO_Pin_9);          //高电平使能
#define M1_Disable   GPIO_ResetBits(GPIOB,GPIO_Pin_9);        //低电平失能
/******************************************************************************/
//编码器类型，二者只能选一
#define M1_AS5600    1
#define M1_TLE5012B  0
/******************************************************************************/

#endif

