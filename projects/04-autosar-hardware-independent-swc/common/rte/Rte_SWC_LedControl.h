/* This file contains stub implementations of the AUTOSAR RTE functions.
   The stub implementations can be used for testing the generated code in
   Simulink, for example, in SIL/PIL simulations of the component under
   test. Note that this file should be replaced with an appropriate RTE
   file when deploying the generated code outside of Simulink.

   This file is generated for:
   Atomic software component:  "SWC_LedControl"
   ARXML schema: "R22-11"
   File generated on: "Mon Oct 05 21:30:13 2026"  */

#ifndef Rte_SWC_LedControl_h
#define Rte_SWC_LedControl_h
#include "Rte_Type.h"
#include "Compiler.h"

/* Data access functions */
#define Rte_IWrite_SWC_LedControl_Step_LedCommand_LedCommand Rte_IWrite_SWC_LedControl_SWC_LedControl_Step_LedCommand_LedCommand

void Rte_IWrite_SWC_LedControl_Step_LedCommand_LedCommand(boolean u);

#define Rte_IWriteRef_SWC_LedControl_Step_LedCommand_LedCommand Rte_IWriteRef_SWC_LedControl_SWC_LedControl_Step_LedCommand_LedCommand

boolean* Rte_IWriteRef_SWC_LedControl_Step_LedCommand_LedCommand(void);

/* Entry point functions */
extern FUNC(void, SWC_LedControl_CODE) SWC_LedControl_Init(void);
extern FUNC(void, SWC_LedControl_CODE) SWC_LedControl_Step(void);

#endif
