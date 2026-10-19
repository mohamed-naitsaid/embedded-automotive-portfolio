/*
 * File: SWC_LedControl.h
 *
 * Code generated for Simulink model 'SWC_LedControl'.
 *
 * Model version                  : 1.3
 * Simulink Coder version         : 24.1 (R2024a) 19-Nov-2023
 * C/C++ source code generated on : Mon Oct  5 21:29:54 2026
 *
 * Target selection: autosar.tlc
 * Embedded hardware selection: Intel->x86-64 (Windows64)
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */

#ifndef SWC_LedControl_h_
#define SWC_LedControl_h_
#ifndef SWC_LedControl_COMMON_INCLUDES_
#define SWC_LedControl_COMMON_INCLUDES_
#include "Platform_Types.h"
#include "Rte_SWC_LedControl.h"
#endif                                 /* SWC_LedControl_COMMON_INCLUDES_ */

#include "SWC_LedControl_types.h"

/* PublicStructure Variables for Internal Data, for system '<Root>' */
typedef struct {
  boolean UnitDelay_DSTATE;            /* '<Root>/Unit Delay' */
} ARID_DEF_SWC_LedControl_T;

/* PublicStructure Variables for Internal Data */
extern ARID_DEF_SWC_LedControl_T SWC_LedControl_ARID_DEF;/* '<Root>/Unit Delay' */

/*-
 * The generated code includes comments that allow you to trace directly
 * back to the appropriate location in the model.  The basic format
 * is <system>/block_name, where system is the system number (uniquely
 * assigned by Simulink) and block_name is the name of the block.
 *
 * Use the MATLAB hilite_system command to trace the generated code back
 * to the model.  For example,
 *
 * hilite_system('<S3>')    - opens system 3
 * hilite_system('<S3>/Kp') - opens and selects block Kp which resides in S3
 *
 * Here is the system hierarchy for this model
 *
 * '<Root>' : 'SWC_LedControl'
 */
#endif                                 /* SWC_LedControl_h_ */

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
