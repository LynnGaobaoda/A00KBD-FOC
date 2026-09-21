#include "FocDrv.h"
#include "MyProject.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_i2c.h"

#define ENCODER_MAX   4095u
#define FOC_2PI       6283
#define DUTY_RES      10000u

#define AS5600_ADDR   0x36u
#define AS5600_RAW_HI 0x0Cu
#define ENC_PUMP_MAX  4000u
#define ENC_TO_MAX    8000u

typedef enum
{
    ENC_IDLE = 0,
    ENC_W_SB,
    ENC_W_ADDR,
    ENC_W_REG,
    ENC_R_SB,
    ENC_R_ADDR,
    ENC_R_DATA
} EncSt_t;

typedef struct
{
    uint16 duty[3];
    uint32 elec_ang;
    sint32 mech_ang;
    sint32 speed;
    sint32 iabc[3];
} FocDrvRte_t;

typedef struct
{
    sint8  dir;
    sint32 zero_elec;
    sint32 turn_offset;
    uint8  pole_pairs;
    sint32 mech_ang_prev;
    uint8  max_duty_ch;
    uint8  periodic;
    EncSt_t enc_st;
    uint16 enc_to;
    uint16 enc_raw;
    uint8  enc_new;
    sint16 enc_prev;
} FocDrvLocal_t;

static FocDrvRte_t   g_rte;
static FocDrvLocal_t g_loc;

u16 FocGet_Adc(u8 ch);

static sint32 Wrap2Pi(sint32 ang)
{
    ang %= FOC_2PI;
    if (ang < 0)
    {
        ang += FOC_2PI;
    }
    return ang;
}

static sint32 AdcToCurrent(u8 ch)
{
    return -(((sint32)FocGet_Adc(ch) * 3300 / 4095) - 1650) * 2 / 5;
}

static void Enc_Recover(void)
{
    I2C_SoftwareResetCmd(I2C1, ENABLE);
    I2C_SoftwareResetCmd(I2C1, DISABLE);
    I2C_Init_();
    g_loc.enc_st = ENC_IDLE;
    g_loc.enc_to = 0;
}

static void Enc_Start(void)
{
    if (g_loc.enc_st != ENC_IDLE)
    {
        return;
    }
    if ((I2C1->SR2 & I2C_SR2_BUSY) != 0u)
    {
        Enc_Recover();
    }
    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->CR1 &= (uint16)~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_START;
    g_loc.enc_st = ENC_W_SB;
    g_loc.enc_to = 0;
}

static uint8 Enc_HasErr(void)
{
    uint16 sr1 = I2C1->SR1;
    if ((sr1 & (I2C_SR1_AF | I2C_SR1_ARLO | I2C_SR1_BERR | I2C_SR1_OVR | I2C_SR1_TIMEOUT)) != 0u)
    {
        I2C1->SR1 = (uint16)(sr1 & (uint16)~(I2C_SR1_AF | I2C_SR1_ARLO | I2C_SR1_BERR
                                             | I2C_SR1_OVR | I2C_SR1_TIMEOUT));
        Enc_Recover();
        return 1u;
    }
    return 0u;
}

/* Non-blocking AS5600 2-byte read. Returns 1 if a hardware event was consumed. */
static uint8 Enc_Step(void)
{
    volatile uint16 tmp;
    uint16 sr1;
    uint8 dh;
    uint8 dl;

    if (Enc_HasErr() != 0u)
    {
        return 1u;
    }
    if (g_loc.enc_st == ENC_IDLE)
    {
        return 0u;
    }

    g_loc.enc_to++;
    if (g_loc.enc_to > ENC_TO_MAX)
    {
        Enc_Recover();
        return 1u;
    }

    sr1 = I2C1->SR1;
    switch (g_loc.enc_st)
    {
        case ENC_W_SB:
            if ((sr1 & I2C_SR1_SB) == 0u)
            {
                return 0u;
            }
            I2C1->DR = (uint16)(AS5600_ADDR << 1);
            g_loc.enc_st = ENC_W_ADDR;
            return 1u;

        case ENC_W_ADDR:
            if ((sr1 & I2C_SR1_ADDR) == 0u)
            {
                return 0u;
            }
            tmp = I2C1->SR2;
            (void)tmp;
            I2C1->DR = AS5600_RAW_HI;
            g_loc.enc_st = ENC_W_REG;
            return 1u;

        case ENC_W_REG:
            if ((sr1 & I2C_SR1_BTF) == 0u)
            {
                return 0u;
            }
            I2C1->CR1 |= I2C_CR1_POS;
            I2C1->CR1 |= I2C_CR1_START;
            g_loc.enc_st = ENC_R_SB;
            return 1u;

        case ENC_R_SB:
            if ((sr1 & I2C_SR1_SB) == 0u)
            {
                return 0u;
            }
            I2C1->DR = (uint16)((AS5600_ADDR << 1) | 1u);
            g_loc.enc_st = ENC_R_ADDR;
            return 1u;

        case ENC_R_ADDR:
            if ((sr1 & I2C_SR1_ADDR) == 0u)
            {
                return 0u;
            }
            tmp = I2C1->SR2;
            (void)tmp;
            I2C1->CR1 &= (uint16)~I2C_CR1_ACK;
            g_loc.enc_st = ENC_R_DATA;
            return 1u;

        case ENC_R_DATA:
            if ((sr1 & I2C_SR1_BTF) == 0u)
            {
                return 0u;
            }
            I2C1->CR1 |= I2C_CR1_STOP;
            dh = (uint8)I2C1->DR;
            dl = (uint8)I2C1->DR;
            I2C1->CR1 |= I2C_CR1_ACK;
            I2C1->CR1 &= (uint16)~I2C_CR1_POS;
            g_loc.enc_raw = (uint16)(((uint16)dh << 8) | (uint16)dl) & ENCODER_MAX;
            g_loc.enc_new = 1u;
            g_loc.enc_st = ENC_IDLE;
            g_loc.enc_to = 0;
            return 1u;

        default:
            Enc_Recover();
            return 1u;
    }
}

static void Enc_Pump(uint32 budget)
{
    uint32 i;
    for (i = 0u; i < budget; i++)
    {
        if (Enc_Step() == 0u)
        {
            if (g_loc.enc_st == ENC_IDLE)
            {
                break;
            }
        }
        else if (g_loc.enc_st == ENC_IDLE)
        {
            break;
        }
        else
        {
        }
    }
}

static void Enc_Apply(uint16 enc)
{
    sint16 denc;

    denc = (sint16)enc - g_loc.enc_prev;
    if ((denc > 3276) || (denc < -3276))
    {
        g_loc.turn_offset += (denc > 0) ? -FOC_2PI : FOC_2PI;
    }
    g_loc.enc_prev = (sint16)enc;

    g_rte.mech_ang = (sint32)g_loc.dir * (g_loc.turn_offset
                     + ((sint32)enc * FOC_2PI / (sint32)ENCODER_MAX));
}

static void Enc_Publish(void)
{
    if (g_loc.enc_new != 0u)
    {
        g_loc.enc_new = 0u;
        Enc_Apply(g_loc.enc_raw);
    }
}

static void Enc_SyncOnce(void)
{
    Enc_Start();
    Enc_Pump(ENC_PUMP_MAX);
    Enc_Publish();
}

static sint32 CalElecAngle(void)
{
    return Wrap2Pi(g_rte.mech_ang * (sint32)g_loc.pole_pairs - g_loc.zero_elec);
}

static sint32 CalSpeed(void)
{
    /* 1 ms period: mrad/ms -> rpm */
    return (sint32)(((sint64)(g_rte.mech_ang - g_loc.mech_ang_prev) * 60000)
                    / FOC_2PI);
}

static void SampleCurrentIfIdle(void)
{
    if ((g_rte.duty[0] == 0u) && (g_rte.duty[1] == 0u) && (g_rte.duty[2] == 0u))
    {
        g_rte.iabc[0] = AdcToCurrent(ADC_Channel_3);
        g_rte.iabc[1] = AdcToCurrent(ADC_Channel_4);
    }
}

static void SetPwm(void)
{
    if (g_rte.duty[0] > g_rte.duty[1])
    {
        g_loc.max_duty_ch = (g_rte.duty[0] > g_rte.duty[2]) ? 0u : 2u;
    }
    else
    {
        g_loc.max_duty_ch = (g_rte.duty[1] > g_rte.duty[2]) ? 1u : 2u;
    }

    TIM_SetCompare1(TIM2, (uint16)((uint32)g_rte.duty[0] * PWM_Period / DUTY_RES));
    TIM_SetCompare2(TIM2, (uint16)((uint32)g_rte.duty[1] * PWM_Period / DUTY_RES));
    TIM_SetCompare3(TIM2, (uint16)((uint32)g_rte.duty[2] * PWM_Period / DUTY_RES));
}

void CDD_PwmNotificatin(uint8 ch)
{
    if (g_loc.max_duty_ch == ch)
    {
        g_rte.iabc[0] = AdcToCurrent(ADC_Channel_3);
        g_rte.iabc[1] = AdcToCurrent(ADC_Channel_4);
    }
}

void Rte_Call_SP_PhaseCur_Operation(sint32 *data)
{
    data[0] = g_rte.iabc[0];
    data[1] = g_rte.iabc[1];
}

void Rte_Call_SP_ElecRad_Operation(uint32 *data)
{
    g_rte.elec_ang = (uint32)CalElecAngle();
    *data = g_rte.elec_ang;
}

void Rte_Call_SP_MachRadVelocity_Operation(sint32 *data)
{
    *data = g_rte.speed;
}

void Rte_Call_SP_MachRad_Operation(sint32 *data)
{
    *data = g_rte.mech_ang;
}

void Rte_Write_RP_PhaseDutySet_Element(uint16 *data)
{
    g_rte.duty[0] = data[0];
    g_rte.duty[1] = data[1];
    g_rte.duty[2] = data[2];
}

void Rte_Write_RP_FocDrvEbl_Element(uint8 data)
{
    (void)data;
}

void CDD_Foc_Init(sint8 dir, uint8 pole_pairs, sint32 zero_elec, sint32 turn_offset)
{
    g_loc.dir         = (dir >= 0) ? 1 : -1;
    g_loc.pole_pairs  = pole_pairs;
    g_loc.zero_elec   = zero_elec;
    g_loc.turn_offset = turn_offset;
    g_loc.enc_prev    = 3141;
    g_loc.periodic    = 0u;
    g_loc.enc_st      = ENC_IDLE;
    Enc_SyncOnce();
}

void CDD_Foc_SetCalib(sint8 dir, sint32 zero_elec)
{
    g_loc.dir       = (dir >= 0) ? 1 : -1;
    g_loc.zero_elec = zero_elec;
}

void CDD_Foc_ApplyPwm(void)
{
    SetPwm();
}

sint32 CDD_Foc_ReadMechAng(void)
{
    if (g_loc.periodic == 0u)
    {
        Enc_SyncOnce();
    }
    return g_rte.mech_ang;
}

void CDD_Foc_1ms(void)
{
    g_loc.periodic = 1u;

    /* Finish last async transfer, publish, kick the next one. */
    Enc_Pump(ENC_PUMP_MAX);
    Enc_Publish();
    Enc_Start();

    g_rte.elec_ang = (uint32)CalElecAngle();
    SampleCurrentIfIdle();
    g_rte.speed = CalSpeed();
    g_loc.mech_ang_prev = g_rte.mech_ang;
}
