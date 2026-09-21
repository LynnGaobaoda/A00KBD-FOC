
#ifndef RTE_FOC_CTRL_H_
#define RTE_FOC_CTRL_H_
#include "Platform_Types.h"
#include "FocDrv.h"

extern void Runnable_1ms(void);
#define Rte_Call_CS_PhaseCur_Operation Rte_Call_SP_PhaseCur_Operation

#define Rte_Call_CS_ElecRad_Operation Rte_Call_SP_ElecRad_Operation 

#define Rte_Call_CS_MachRadVelocity_Operation Rte_Call_SP_MachRadVelocity_Operation

#define Rte_Call_CS_MachRad_Operation Rte_Call_SP_MachRad_Operation

#define Rte_Write_PP_PhaseDutySet_Element Rte_Write_RP_PhaseDutySet_Element

#define Rte_Write_PP_FocDrvEbl_Element Rte_Write_RP_FocDrvEbl_Element

#endif // !RTE_FOC_CTRL
