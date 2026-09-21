#include "stm32f10x.h"
#include "MyProject.h"
#include "adc.h"
#include "FocCfg.h"
#include "FocDrv.h"
#if (FOC_ALGO == FOC_ALGO_MATLAB)
#include "FocCtrl.h"
extern void FocCtrl_Init(void);
#else
#include "Swc_Foc.h"
#endif

#define LED_blink  (GPIOC->ODR ^= (1 << 13))

float target;

#if (FOC_ALGO == FOC_ALGO_MATLAB)
static void MotorPwmOff(void)
{
    TIM_SetCompare1(TIM2, 0);
    TIM_SetCompare2(TIM2, 0);
    TIM_SetCompare3(TIM2, 0);
}
#endif

static void GpioConfig(void)
{
    GPIO_InitTypeDef io;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB
                           | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    io.GPIO_Pin = GPIO_Pin_13;
    io.GPIO_Mode = GPIO_Mode_Out_PP;
    io.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOC, &io);
    GPIO_SetBits(GPIOC, GPIO_Pin_13);

    io.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOA, &io);
    GPIO_SetBits(GPIOA, GPIO_Pin_6);

    io.GPIO_Pin = GPIO_Pin_9;
    GPIO_Init(GPIOB, &io);
    GPIO_SetBits(GPIOB, GPIO_Pin_9);
}

static void Task_1ms(void)
{
    CDD_Foc_1ms();
#if (FOC_ALGO == FOC_ALGO_MATLAB)
    Runnable_1ms();
    CDD_Foc_ApplyPwm();
#else
    Swc_Foc_PollUart();
#endif
}

static void LedKick(void)
{
    volatile uint32 i;
    uint8 n;

    for (n = 0u; n < 16u; n++)
    {
        GPIOC->ODR ^= (1u << 13);
        for (i = 0ul; i < 200000ul; i++)
        {
        }
    }
}

int main(void)
{
    GpioConfig();
    LedKick();
    TIM3_1ms_Init();
    uart_init(115200);
    printf("B1\r\n");

    I2C_Init_();
    TIM2_PWM_Init();
    delay_ms(100);
    Adc_Init();

#if (FOC_ALGO == FOC_ALGO_MATLAB)
    MagneticSensor_Init();
    voltage_power_supply = 10;
    voltage_limit = 4;
    velocity_limit = 10;
    voltage_sensor_align = 1;
    torque_controller = Type_voltage;
    controller = Type_angle;
    pole_pairs = 11;
    Motor_init();
    Motor_initFOC();
    PID_init();
    MotorPwmOff();
    CDD_Foc_Init((sint8)sensor_direction,
                 (uint8)pole_pairs,
                 (sint32)(zero_electric_angle * 1000.0f),
                 (sint32)(full_rotation_offset * 1000.0f));
    FocCtrl_Init();
#else
    CDD_Foc_Init(1, 7, 0, 0);
    Swc_Foc_Init();
    Swc_Foc_Align();
#endif

    systick_CountMode();

    /* 1 ms: async AS5600 + UART. Tight loop: detent FOC + PWM.
     * PC13 LED toggles in TIM3 every 100 ms, including during align. */

    while (1)
    {
        if (u8_1msTask)
        {
            u8_1msTask = 0u;
            Task_1ms();
        }
#if (FOC_ALGO == FOC_ALGO_HANDWRITTEN)
        Swc_Foc_Loop();
        CDD_Foc_ApplyPwm();
#endif
    }
}
