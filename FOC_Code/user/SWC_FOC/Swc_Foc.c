#include "Swc_Foc.h"
#include "Rte_FocCtrl.h"
#include "FocDrv.h"
#include "HapticRatchet.h"
#include "delay.h"
#include "usart.h"
#include "stm32f10x_it.h"
#include "iap_map.h"

/* Handwritten voltage FOC (no float). Units: millirad, millivolt, duty 0..10000. */

#define FOC_SCALE       1000
#define FOC_SQRT3       1732
#define FOC_PWM_RES     10000
#define FOC_UDC_MV      12000
#define FOC_PI_2        1570
#define FOC_3PI_2       4712
#define FOC_2PI         6283

#define FOC_POLE_PAIRS  7             /* 3205B 12N14P */
#define UQ_ALIGN        1800          /* 1.8 V align, ~15% of 12 V bus */
#define ALIGN_STEPS     500
#define ALIGN_DT_MS     2
#define ALIGN_MIN_MOVE  80            /* ~4.6 deg; encoder must move */

#define FOC_MODE_RATCHET  0u
#define FOC_MODE_GOTO     1u
#define DEG_CMD(x)        ((sint32)(x) * FOC_2PI / 360)
#define GOTO_KP_DEF       2500
#define GOTO_UQ_DEF       1400
#define GOTO_SPD_DEF      1           /* millirad / ms; 60° ≈ 1.0 s */
#define GOTO_SETTLE       DEG_CMD(4)
#define GOTO_HOLD_MS      80u
#define GOTO_TIMEOUT_MS   8000u


typedef struct
{
    uint32 elec_ang;
    sint32 pos;
    sint32 uq;
    sint32 u_alpha;
    sint32 u_beta;
    sint32 tgt;
    sint32 tgt_cmd;
    sint32 goto_kp;
    sint32 goto_uq;
    sint32 goto_spd;
    uint32 goto_t0;
    uint16 duty[3];
    uint16 hold_ms;
    uint8  aligned;
    uint8  mode;     /* 0=ratchet, 1=goto */
    uint8  dbg;
    uint8  hb;
    uint8  goto_near;
} FocHand_t;

static FocHand_t g_foc;

static const uint8 k_sector_tbl[8] = {0, 2, 6, 1, 4, 3, 5, 0};

static const sint16 k_sin_tbl[101] =
{
    0, 16, 31, 47, 63, 78, 94, 110, 125, 141, 156, 172, 187, 203, 218, 233, 249,
    264, 279, 294, 309, 324, 339, 353, 368, 383, 397, 412, 426, 440, 454, 468,
    482, 495, 509, 522, 536, 549, 562, 575, 588, 600, 613, 625, 637, 649, 661,
    673, 685, 696, 707, 718, 729, 740, 750, 760, 771, 780, 790, 800, 809, 818,
    827, 836, 844, 853, 861, 869, 876, 884, 891, 898, 905, 911, 918, 924, 930,
    935, 941, 946, 951, 956, 960, 965, 969, 972, 976, 979, 982, 985, 988, 990,
    992, 994, 996, 997, 998, 999, 1000, 1000, 1000
};

static sint32 Wrap2Pi(sint32 ang)
{
    ang %= FOC_2PI;
    if (ang < 0)
    {
        ang += FOC_2PI;
    }
    return ang;
}

static sint16 Foc_Sin(sint32 ang)
{
    sint32 q;
    sint32 x;
    sint8  sign;

    ang = Wrap2Pi(ang);
    q = ang / FOC_PI_2;
    x = ang % FOC_PI_2;
    if (q >= 4)
    {
        return 0;
    }
    if ((q == 1) || (q == 3))
    {
        x = FOC_PI_2 - x;
    }
    sign = ((q == 2) || (q == 3)) ? -1 : 1;
    x = x * 100 / FOC_PI_2;
    return (sint16)(sign * k_sin_tbl[x]);
}

static sint16 Foc_Cos(sint32 ang)
{
    return Foc_Sin(ang + FOC_PI_2);
}

static void InvPark(sint32 uq, sint32 ud, uint32 ang, sint32 *alpha, sint32 *beta)
{
    sint16 s = Foc_Sin((sint32)ang);
    sint16 c = Foc_Cos((sint32)ang);

    *alpha = (sint32)(((sint64)ud * c - (sint64)uq * s) / FOC_SCALE);
    *beta  = (sint32)(((sint64)uq * c + (sint64)ud * s) / FOC_SCALE);
}

static void Svpwm(sint32 u_alpha, sint32 u_beta, uint16 duty[3])
{
    sint32 v1, v2, v3;
    sint32 x, y, z;
    sint32 t1, t2, t0;
    uint8  sector;

    v1 = u_beta;
    v2 = (sint32)((sint64)u_alpha * FOC_SQRT3 / 2 / FOC_SCALE) - u_beta / 2;
    v3 = (sint32)(-(sint64)u_alpha * FOC_SQRT3 / 2 / FOC_SCALE) - u_beta / 2;

    sector = (uint8)(((v1 > 0) ? 1 : 0) + ((v2 > 0) ? 2 : 0) + ((v3 > 0) ? 4 : 0));
    sector = k_sector_tbl[sector];

    x = (sint32)((sint64)v1 * 2 * FOC_SCALE * FOC_PWM_RES / FOC_SQRT3 / FOC_UDC_MV);
    y = (sint32)((sint64)v2 * 2 * FOC_SCALE * FOC_PWM_RES / FOC_SQRT3 / FOC_UDC_MV);
    z = (sint32)((sint64)v3 * 2 * FOC_SCALE * FOC_PWM_RES / FOC_SQRT3 / FOC_UDC_MV);

    switch (sector)
    {
        case 1: t1 =  y; t2 =  x; break;
        case 2: t1 = -z; t2 = -y; break;
        case 3: t1 =  x; t2 =  z; break;
        case 4: t1 = -y; t2 = -x; break;
        case 5: t1 =  z; t2 =  y; break;
        case 6: t1 = -x; t2 = -z; break;
        default: t1 = 0; t2 = 0; break;
    }

    if ((t1 + t2) > FOC_PWM_RES)
    {
        sint32 sum = t1 + t2;
        t1 = (sint32)((sint64)t1 * FOC_PWM_RES / sum);
        t2 = (sint32)((sint64)t2 * FOC_PWM_RES / sum);
    }

    t0 = (FOC_PWM_RES - t1 - t2) / 2;
    switch (sector)
    {
        case 1: duty[0] = (uint16)(t1 + t2 + t0); duty[1] = (uint16)(t2 + t0);       duty[2] = (uint16)t0;              break;
        case 2: duty[0] = (uint16)(t1 + t0);       duty[1] = (uint16)(t1 + t2 + t0); duty[2] = (uint16)t0;              break;
        case 3: duty[0] = (uint16)t0;              duty[1] = (uint16)(t1 + t2 + t0); duty[2] = (uint16)(t2 + t0);       break;
        case 4: duty[0] = (uint16)t0;              duty[1] = (uint16)(t1 + t0);       duty[2] = (uint16)(t1 + t2 + t0); break;
        case 5: duty[0] = (uint16)(t2 + t0);       duty[1] = (uint16)t0;              duty[2] = (uint16)(t1 + t2 + t0); break;
        case 6: duty[0] = (uint16)(t1 + t2 + t0); duty[1] = (uint16)t0;              duty[2] = (uint16)(t1 + t0);       break;
        default: duty[0] = 0; duty[1] = 0; duty[2] = 0; break;
    }
}

static void Foc_Output(sint32 uq, sint32 ud, uint32 elec)
{
    InvPark(uq, ud, elec, &g_foc.u_alpha, &g_foc.u_beta);
    Svpwm(g_foc.u_alpha, g_foc.u_beta, g_foc.duty);
    Rte_Write_PP_PhaseDutySet_Element(g_foc.duty);
}

/* Immediate PWM (align / openloop only). Closed-loop applies in main. */

static void Foc_Openloop(sint32 uq, uint32 elec)
{
    Foc_Output(uq, 0, elec);
    CDD_Foc_ApplyPwm();
}

void Swc_Foc_Init(void)
{
    g_foc.aligned = 0;
    g_foc.uq = 0;
    g_foc.mode = FOC_MODE_RATCHET;
    g_foc.dbg = 0u;
    g_foc.hb = 1u;
    g_foc.hold_ms = 0;
    g_foc.goto_kp = GOTO_KP_DEF;
    g_foc.goto_uq = GOTO_UQ_DEF;
    g_foc.goto_spd = GOTO_SPD_DEF;
    g_foc.tgt_cmd = 0;
    HapticRatchet_Apply(12u, 3200, 1100, 2);
}

static void AlignDelay(uint16 nms)
{
    uint16 t;

    for (t = 0; t < nms; t++)
    {
        delay_ms(1);
        if (u8_1msTask)
        {
            u8_1msTask = 0u;
            Swc_Foc_PollUart();
        }
    }
}
void Swc_Foc_Align(void)
{
    uint16 i;
    sint32 mid;
    sint32 end;
    sint32 moved;
    sint8  dir;
    sint32 zero;
    uint32 ang;

    printf("FOC align start\r\n");

    for (i = 0; i <= ALIGN_STEPS; i++)
    {
        ang = (uint32)(FOC_3PI_2 + (FOC_2PI * (sint32)i / ALIGN_STEPS));
        Foc_Openloop(UQ_ALIGN, ang);
        AlignDelay(ALIGN_DT_MS);
    }
    mid = CDD_Foc_ReadMechAng();

    for (i = ALIGN_STEPS; i > 0; i--)
    {
        ang = (uint32)(FOC_3PI_2 + (FOC_2PI * (sint32)i / ALIGN_STEPS));
        Foc_Openloop(UQ_ALIGN, ang);
        AlignDelay(ALIGN_DT_MS);
    }
    Foc_Openloop(0, 0);
    AlignDelay(200);
    end = CDD_Foc_ReadMechAng();

    moved = mid - end;
    if (moved < 0)
    {
        moved = -moved;
    }
    if (moved < ALIGN_MIN_MOVE)
    {
        printf("FOC align fail, d=%d\r\n", (int)(mid - end));
        g_foc.aligned = 0;
        return;
    }

    dir = (mid < end) ? (sint8)-1 : (sint8)1;
    CDD_Foc_SetCalib(dir, 0);

    Foc_Openloop(UQ_ALIGN, (uint32)FOC_3PI_2);
    AlignDelay(500);
    zero = Wrap2Pi(CDD_Foc_ReadMechAng() * FOC_POLE_PAIRS);
    CDD_Foc_SetCalib(dir, zero);
    Foc_Openloop(0, 0);
    AlignDelay(200);

    HapticRatchet_Init(CDD_Foc_ReadMechAng());
    g_foc.aligned = 1;
    g_foc.mode = FOC_MODE_RATCHET;
    g_foc.dbg = 0u;
    printf("FOC align ok dir=%d zero=%d\r\n", (int)dir, (int)zero);
    printf("uart 115200. help | r | cw | ccw | p | set ...\r\n");
}

void Swc_Foc_1ms(void)
{
    Swc_Foc_Loop();
}

/* Main-loop FOC: encoder -> mode (ratchet or goto) -> inverse Park + SVPWM. */
void Swc_Foc_Loop(void)
{
    sint32 err;
    sint32 uq;

    if (!g_foc.aligned)
    {
        return;
    }

    Rte_Call_CS_MachRad_Operation(&g_foc.pos);
    Rte_Call_CS_ElecRad_Operation(&g_foc.elec_ang);

    if (g_foc.mode == FOC_MODE_GOTO)
    {
        err = g_foc.pos - g_foc.tgt_cmd;
        uq = -g_foc.goto_kp * err / 1000;
        if (uq > g_foc.goto_uq)
        {
            uq = g_foc.goto_uq;
        }
        if (uq < -g_foc.goto_uq)
        {
            uq = -g_foc.goto_uq;
        }
        g_foc.uq = uq;
        if ((err <= GOTO_SETTLE) && (err >= -GOTO_SETTLE))
        {
            g_foc.goto_near = 1u;
        }
        else
        {
            g_foc.goto_near = 0u;
        }
    }
    else
    {
        g_foc.uq = HapticRatchet_Step(g_foc.pos);
    }

    Foc_Output(g_foc.uq, 0, g_foc.elec_ang);
}

static uint8 CmdIs(const char *s, const char *k)
{
    char c;
    char d;

    while (*k != 0)
    {
        c = *s;
        d = *k;
        if ((c >= 'A') && (c <= 'Z'))
        {
            c = (char)(c + 32);
        }
        if ((d >= 'A') && (d <= 'Z'))
        {
            d = (char)(d + 32);
        }
        if (c != d)
        {
            return 0u;
        }
        s++;
        k++;
    }
    return ((*s == 0) || (*s == ' ') || (*s == '\t')) ? 1u : 0u;
}

static void Foc_EnterRatchet(void)
{
    HapticRatchet_Init(g_foc.pos);
    g_foc.mode = FOC_MODE_RATCHET;
    printf("mode=ratchet\r\n");
}

static void Foc_GotoDelta(sint32 dpos)
{
    if (!g_foc.aligned)
    {
        printf("err not aligned\r\n");
        return;
    }
    g_foc.tgt_cmd = g_foc.pos;
    g_foc.tgt = g_foc.pos + dpos;
    g_foc.goto_t0 = time1_cntr;
    g_foc.hold_ms = 0;
    g_foc.goto_near = 0u;
    g_foc.mode = FOC_MODE_GOTO;
    printf("goto %s60 tgt=%d\r\n", (dpos >= 0) ? "+" : "-", (int)g_foc.tgt);
}

static void Foc_PrintStat(void)
{
    printf("st %d %d %d %d %d %d\r\n",
           (int)g_foc.mode,
           (int)g_foc.pos,
           (int)g_foc.uq,
           (int)HapticRatchet_GetIndex(),
           (int)g_foc.tgt,
           (int)g_foc.aligned);
}

static void SkipSp(char **p)
{
    while ((**p == ' ') || (**p == '\t'))
    {
        (*p)++;
    }
}

static sint32 ParseInt(char **p)
{
    sint32 v = 0;
    sint32 sign = 1;

    SkipSp(p);
    if (**p == '-')
    {
        sign = -1;
        (*p)++;
    }
    while ((**p >= '0') && (**p <= '9'))
    {
        v = (v * 10) + (sint32)(**p - '0');
        (*p)++;
    }
    return sign * v;
}

static void Foc_PrintParam(void)
{
    uint8 n;
    sint32 kp;
    sint32 uq;
    sint32 dead;

    HapticRatchet_GetCfg(&n, &kp, &uq, &dead);
    printf("cal 3205B pp=7 ualign=1800 n=%u kp=%d uq=%d dead=%d gkp=%d guq=%d spd=%d\r\n",
           (unsigned)n, (int)kp, (int)uq, (int)dead,
           (int)g_foc.goto_kp, (int)g_foc.goto_uq, (int)g_foc.goto_spd);
}

static uint8 Foc_HandleSet(char *line)
{
    char *p = line;
    uint8 n;
    sint32 kp;
    sint32 uq;
    sint32 dead;
    sint32 v;

    if (!CmdIs(line, "set"))
    {
        return 0u;
    }
    while (*p && (*p != ' ') && (*p != '\t'))
    {
        p++;
    }
    SkipSp(&p);
    HapticRatchet_GetCfg(&n, &kp, &uq, &dead);
    if (CmdIs(p, "kp"))
    {
        while (*p && (*p != ' ') && (*p != '\t')) { p++; }
        v = ParseInt(&p);
        if (v < 500) { v = 500; }
        if (v > 20000) { v = 20000; }
        HapticRatchet_Apply(n, v, uq, dead);
        HapticRatchet_Init(g_foc.pos);
    }
    else if (CmdIs(p, "uq"))
    {
        while (*p && (*p != ' ') && (*p != '\t')) { p++; }
        v = ParseInt(&p);
        if (v < 200) { v = 200; }
        if (v > 4000) { v = 4000; }
        HapticRatchet_Apply(n, kp, v, dead);
    }
    else if (CmdIs(p, "n"))
    {
        while (*p && (*p != ' ') && (*p != '\t')) { p++; }
        v = ParseInt(&p);
        HapticRatchet_Apply((uint8)v, kp, uq, dead);
        HapticRatchet_Init(g_foc.pos);
    }
    else if (CmdIs(p, "dead"))
    {
        while (*p && (*p != ' ') && (*p != '\t')) { p++; }
        v = ParseInt(&p);
        HapticRatchet_Apply(n, kp, uq, v);
    }
    else if (CmdIs(p, "gkp"))
    {
        while (*p && (*p != ' ') && (*p != '\t')) { p++; }
        v = ParseInt(&p);
        if (v < 500) { v = 500; }
        if (v > 20000) { v = 20000; }
        g_foc.goto_kp = v;
    }
    else if (CmdIs(p, "guq"))
    {
        while (*p && (*p != ' ') && (*p != '\t')) { p++; }
        v = ParseInt(&p);
        if (v < 200) { v = 200; }
        if (v > 4000) { v = 4000; }
        g_foc.goto_uq = v;
    }
    else if (CmdIs(p, "spd"))
    {
        while (*p && (*p != ' ') && (*p != '\t')) { p++; }
        v = ParseInt(&p);
        if (v < 1) { v = 1; }
        if (v > 20) { v = 20; }
        g_foc.goto_spd = v;
    }
    else
    {
        printf("set kp|uq|n|dead|gkp|guq|spd N\r\n");
        return 1u;
    }
    Foc_PrintParam();
    return 1u;
}

static void Foc_HandleLine(char *line)
{
    if (CmdIs(line, "help") || CmdIs(line, "?"))
    {
        printf("r ratchet  cw +60  ccw -60  p  set kp|uq|n|dead|gkp|guq|spd N\r\n");
        printf("stat  dbg on|off  hb on|off  iap  ver|ver boot|ver app\r\n");
    }
    else if (CmdIs(line, "r") || CmdIs(line, "ratchet"))
    {
        Foc_EnterRatchet();
    }
    else if (CmdIs(line, "cw") || CmdIs(line, "cw60"))
    {
        Foc_GotoDelta(DEG_CMD(60));
    }
    else if (CmdIs(line, "ccw") || CmdIs(line, "ccw60"))
    {
        Foc_GotoDelta(-DEG_CMD(60));
    }
    else if (CmdIs(line, "stat"))
    {
        Foc_PrintStat();
    }
    else if (CmdIs(line, "p") || CmdIs(line, "param"))
    {
        Foc_PrintParam();
    }
    else if (Foc_HandleSet(line))
    {
    }
    else if (CmdIs(line, "dbg on") || CmdIs(line, "dbg 1"))
    {
        g_foc.dbg = 1u;
        printf("dbg=1\r\n");
    }
    else if (CmdIs(line, "dbg off") || CmdIs(line, "dbg 0"))
    {
        g_foc.dbg = 0u;
        printf("dbg=0\r\n");
    }
    else if (CmdIs(line, "hb on") || CmdIs(line, "hb 1"))
    {
        g_foc.hb = 1u;
        printf("hb=1\r\n");
    }
    else if (CmdIs(line, "hb off") || CmdIs(line, "hb 0"))
    {
        g_foc.hb = 0u;
        printf("hb=0\r\n");
    }
    else if (CmdIs(line, "ver"))
    {
        const char *bootv = (const char *)IAP_BOOT_VER_ADDR;
        char *p = line + 3;

        if ((bootv[0] != 'B') || (bootv[1] != 'o'))
        {
            bootv = "Boot_??.???_????????";
        }
        while ((*p == ' ') || (*p == '\t'))
        {
            p++;
        }
        if (*p == 0)
        {
            printf("%s\r\n%s\r\n", bootv, IAP_APP_VER_STR);
        }
        else if (CmdIs(p, "boot"))
        {
            printf("%s\r\n", bootv);
        }
        else if (CmdIs(p, "app"))
        {
            printf("%s\r\n", IAP_APP_VER_STR);
        }
        else
        {
            printf("ver|ver boot|ver app\r\n");
        }
    }
    else if (CmdIs(line, "iap") || CmdIs(line, "boot"))
    {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
        PWR_BackupAccessCmd(ENABLE);
        BKP_WriteBackupRegister(BKP_DR1, IAP_BKP_MAGIC);
        printf("iap reset\r\n");
        NVIC_SystemReset();
    }
    else
    {
        printf("unk %s\r\n", line);
    }
}

void Swc_Foc_PollUart(void)
{
    static uint32 last_ms;
    static uint32 hb_t;
    static uint8 dbg_div;
    uint32 dt;

    if ((USART_RX_STA & 0x8000) != 0)
    {
        Foc_HandleLine((char *)USART_RX_BUF);
        USART_RX_STA = 0;
    }

    if (time1_cntr == last_ms)
    {
        return;
    }
    last_ms = time1_cntr;

    if (g_foc.mode == FOC_MODE_GOTO)
    {
        {
            sint32 d;
            sint32 step;

            d = g_foc.tgt - g_foc.tgt_cmd;
            step = g_foc.goto_spd;
            if (step < 1)
            {
                step = 1;
            }
            if (d > step)
            {
                g_foc.tgt_cmd += step;
            }
            else if (d < -step)
            {
                g_foc.tgt_cmd -= step;
            }
            else
            {
                g_foc.tgt_cmd = g_foc.tgt;
            }
        }
        dt = time1_cntr - g_foc.goto_t0;
        if ((g_foc.goto_near != 0u) && (g_foc.tgt_cmd == g_foc.tgt))
        {
            g_foc.hold_ms++;
        }
        else
        {
            g_foc.hold_ms = 0;
        }
        if ((g_foc.hold_ms >= GOTO_HOLD_MS) || (dt >= GOTO_TIMEOUT_MS))
        {
            HapticRatchet_Init(g_foc.pos);
            g_foc.mode = FOC_MODE_RATCHET;
            printf("ok ratchet pos=%d\r\n", (int)g_foc.pos);
        }
    }

    if ((g_foc.hb != 0u) && ((time1_cntr - hb_t) >= 500u))
    {
        hb_t = time1_cntr;
        printf("hb %d %d %d\r\n",
               (int)g_foc.aligned,
               (int)g_foc.pos,
               (int)g_foc.uq);
    }

    if (g_foc.dbg == 0u)
    {
        return;
    }
    dbg_div++;
    if (dbg_div >= 200u)
    {
        dbg_div = 0u;
        Foc_PrintStat();
    }
}
