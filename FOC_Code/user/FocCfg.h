#ifndef FOC_CFG_H
#define FOC_CFG_H

/* 算法层二选一：手写 SWC / Simulink 生成代码（FocCtrl.c 不改） */
#define FOC_ALGO_HANDWRITTEN  0
#define FOC_ALGO_MATLAB       1

#ifndef FOC_ALGO
#define FOC_ALGO              FOC_ALGO_HANDWRITTEN
#endif

#endif
