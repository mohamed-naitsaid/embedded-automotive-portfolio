/*
 * File: SWC_LedControl.c
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

#include "SWC_LedControl.h"

/* PublicStructure Variables for Internal Data */
ARID_DEF_SWC_LedControl_T SWC_LedControl_ARID_DEF;/* '<Root>/Unit Delay' */

/* Model step function */
void SWC_LedControl_Step(void)
{
  /* Outport: '<Root>/LedCommand' incorporates:
   *  UnitDelay: '<Root>/Unit Delay'
   */
  Rte_IWrite_SWC_LedControl_Step_LedCommand_LedCommand
    (SWC_LedControl_ARID_DEF.UnitDelay_DSTATE);

  /* Logic: '<Root>/Logical Operator' incorporates:
   *  UnitDelay: '<Root>/Unit Delay'
   */
  SWC_LedControl_ARID_DEF.UnitDelay_DSTATE =
    !SWC_LedControl_ARID_DEF.UnitDelay_DSTATE;
}

/* Model initialize function */
void SWC_LedControl_Init(void)
{
  /* (no initialization code required) */
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
