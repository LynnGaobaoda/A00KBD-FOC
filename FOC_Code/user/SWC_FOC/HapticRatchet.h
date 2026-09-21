#ifndef HAPTIC_RATCHET_H
#define HAPTIC_RATCHET_H

#include "Platform_Types.h"

/* Integer virtual detents. pos: millirad, FOC_2PI=6283. Return: Uq mV-scale. */

void HapticRatchet_Init(sint32 pos_now);
sint32 HapticRatchet_Step(sint32 pos_now);
sint32 HapticRatchet_GetIndex(void);
void HapticRatchet_Apply(uint8 detents, sint32 kp, sint32 uq_lim, sint32 dead_deg);
void HapticRatchet_GetCfg(uint8 *detents, sint32 *kp, sint32 *uq_lim, sint32 *dead_deg);

#endif
