
#include "MyProject.h"


PhaseCurrent_s current_ADC;
/************************************************
本程序仅供学习，引用代码请标明出处
使用教程：https://blog.csdn.net/loop222/article/details/119220638
创建日期：20210905
作    者：loop222 @郑州
************************************************/
PhaseCurrent_s getPhaseCurrents(){
	PhaseCurrent_s current;
	current.a=(float)Get_Adc_Average(ADC_Channel_3,1)*(3.3/4096)*0.4;//1÷0.05÷50=0.4
	current.b=(float)Get_Adc_Average(ADC_Channel_4,1)*(3.3/4096)*0.4;
	return current;
}

/******************************************************************************/
// get current magnitude 得到DC电流的幅值
//   - absolute  - if no electrical_angle provided 
//   - signed    - if angle provided
float getDCCurrent(float motor_electrical_angle)
{
	PhaseCurrent_s current;
	float sign=1;       // currnet sign - if motor angle not provided the magnitude is always positive
	float i_alpha, i_beta;
	
	// read current phase currents
	current = current_ADC;

//	current.a=(float)Get_Adc_Average(ADC_Channel_3,1)*(3.3/4096)*0.4;//1÷0.05÷50=0.4
//	current.b=(float)Get_Adc_Average(ADC_Channel_4,1)*(3.3/4096)*0.4;
	

	
	
	// calculate clarke transform
	if(!current.c)
	{
		// if only two measured currents
		i_alpha = current.a;
		i_beta = _1_SQRT3 * current.a + _2_SQRT3 * current.b;
	}
	else
	{
		// signal filtering using identity a + b + c = 0. Assumes measurement error is normally distributed.
		float mid = (1.f/3) * (current.a + current.b + current.c);
		float a = current.a - mid;
		float b = current.b - mid;
		i_alpha = a;
		i_beta = _1_SQRT3 * a + _2_SQRT3 * b;
	}
	
	// if motor angle provided function returns signed value of the current
	// determine the sign of the current
	// sign(atan2(current.q, current.d)) is the same as c.q > 0 ? 1 : -1  
	if(motor_electrical_angle)sign = (i_beta * _cos(motor_electrical_angle) - i_alpha*_sin(motor_electrical_angle)) > 0 ? 1 : -1;  
	// return current magnitude
	return sign*_sqrt(i_alpha*i_alpha + i_beta*i_beta);
}
/******************************************************************************/
// function used with the foc algorihtm
//   calculating DQ currents from phase currents
//   - function calculating park and clarke transform of the phase currents 
//   - using getPhaseCurrents internally
DQCurrent_s getFOCCurrents(float angle_el)
{
	PhaseCurrent_s current;
	float i_alpha, i_beta;
	float ct,st;
	DQCurrent_s ret;

	// read current phase currents
	current = current_ADC;
	//current = getPhaseCurrents();
	
	// calculate clarke transform
	if(1)
	{
		// if only two measured currents
		i_alpha = current.a;  
		i_beta = _1_SQRT3 * current.a + _2_SQRT3 * current.b;
	}
	else
	{
		// signal filtering using identity a + b + c = 0. Assumes measurement error is normally distributed.
		float mid = (1.f/3) * (current.a + current.b + current.c);
		float a = current.a - mid;
		float b = current.b - mid;
		i_alpha = a;
		i_beta = _1_SQRT3 * a + _2_SQRT3 * b;
	}
	
	// calculate park transform
	ct = _cos(angle_el);
	st = _sin(angle_el);
	ret.d = i_alpha * ct + i_beta * st;
	ret.q = i_beta * ct - i_alpha * st;
	return ret;
}
/******************************************************************************/
