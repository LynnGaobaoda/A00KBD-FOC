#ifndef _FOC_DRV_H
#define _FOC_DRV_H
#include "Platform_Types.h"

void Rte_Call_SP_PhaseCur_Operation(sint32 *data);
void Rte_Call_SP_ElecRad_Operation(uint32 *data);
void Rte_Call_SP_MachRadVelocity_Operation(sint32 *data);
void Rte_Call_SP_MachRad_Operation(sint32 *data);
void Rte_Write_RP_PhaseDutySet_Element(uint16 *data);
void Rte_Write_RP_FocDrvEbl_Element(uint8 data);

void CDD_Foc_Init(sint8 dir, uint8 pole_pairs, sint32 zero_elec, sint32 turn_offset);
void CDD_Foc_SetCalib(sint8 dir, sint32 zero_elec);
void CDD_Foc_ApplyPwm(void);
sint32 CDD_Foc_ReadMechAng(void);
void CDD_Foc_1ms(void);

#endif
