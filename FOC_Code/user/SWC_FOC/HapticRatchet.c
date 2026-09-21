#include "HapticRatchet.h"

#define FOC_2PI              6283
#define DEG(x)               ((sint32)(x) * FOC_2PI / 360)

/* 3205B 12 V voltage FOC, 6 detents/rev (60 deg). Snap 65% + lock. */
#define HR_DETENTS_DEF       12
#define HR_KP_DEF            3200
#define HR_UQ_DEF            1100
#define HR_DEAD_DEG_DEF      2
#define HR_SNAP_LOCK         40

typedef struct
{
    sint32 center;
    sint32 index;
    sint32 pos_f;
    sint32 uq_prev;
    sint32 kp;
    sint32 uq_lim;
    sint32 dead;
    sint32 width;
    sint32 snap;
    uint16 snap_lock;
    uint8  detents;
} HapticRatchet_t;

static HapticRatchet_t g_hr;

static sint32 HrClamp(sint32 x, sint32 lo, sint32 hi)
{
    if (x > hi)
    {
        return hi;
    }
    if (x < lo)
    {
        return lo;
    }
    return x;
}

static void HrRebuild(void)
{
    if (g_hr.detents < 3u)
    {
        g_hr.detents = 3u;
    }
    if (g_hr.detents > 12u)
    {
        g_hr.detents = 12u;
    }
    g_hr.width = FOC_2PI / (sint32)g_hr.detents;
    g_hr.snap = (g_hr.width * 13) / 20;
}

void HapticRatchet_Apply(uint8 detents, sint32 kp, sint32 uq_lim, sint32 dead_deg)
{
    g_hr.detents = detents;
    g_hr.kp = kp;
    g_hr.uq_lim = uq_lim;
    if (dead_deg < 0)
    {
        dead_deg = 0;
    }
    if (dead_deg > 15)
    {
        dead_deg = 15;
    }
    g_hr.dead = DEG(dead_deg);
    HrRebuild();
}

void HapticRatchet_GetCfg(uint8 *detents, sint32 *kp, sint32 *uq_lim, sint32 *dead_deg)
{
    *detents = g_hr.detents;
    *kp = g_hr.kp;
    *uq_lim = g_hr.uq_lim;
    *dead_deg = g_hr.dead * 360 / FOC_2PI;
}

void HapticRatchet_Init(sint32 pos_now)
{
    if (g_hr.detents == 0u)
    {
        HapticRatchet_Apply(HR_DETENTS_DEF, HR_KP_DEF, HR_UQ_DEF, HR_DEAD_DEG_DEF);
    }
    g_hr.center = pos_now;
    g_hr.index = 0;
    g_hr.pos_f = pos_now;
    g_hr.uq_prev = 0;
    g_hr.snap_lock = 0;
}

sint32 HapticRatchet_GetIndex(void)
{
    return g_hr.index;
}

sint32 HapticRatchet_Step(sint32 pos_now)
{
    sint32 err;
    sint32 input;
    sint32 uq;

    g_hr.pos_f = pos_now;
    err = g_hr.pos_f - g_hr.center;

    if (g_hr.snap_lock > 0u)
    {
        g_hr.snap_lock--;
    }
    else if (err > g_hr.snap)
    {
        g_hr.center += g_hr.width;
        err -= g_hr.width;
        g_hr.index--;
        g_hr.snap_lock = HR_SNAP_LOCK;
    }
    else if (err < -g_hr.snap)
    {
        g_hr.center -= g_hr.width;
        err += g_hr.width;
        g_hr.index++;
        g_hr.snap_lock = HR_SNAP_LOCK;
    }

    if ((err <= g_hr.dead) && (err >= -g_hr.dead))
    {
        input = 0;
    }
    else if (err > 0)
    {
        input = -(err - g_hr.dead);
    }
    else
    {
        input = -(err + g_hr.dead);
    }

    uq = g_hr.kp * input / 1000;
    uq = HrClamp(uq, -g_hr.uq_lim, g_hr.uq_lim);
    uq = (g_hr.uq_prev + uq) / 2;
    g_hr.uq_prev = uq;
    return uq;
}
