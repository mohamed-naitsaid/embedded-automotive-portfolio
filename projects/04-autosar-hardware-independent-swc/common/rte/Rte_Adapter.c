#include "Rte_SWC_LedControl.h"
#include "Led_Hw.h"

static boolean ledCommand = FALSE;

void Rte_IWrite_SWC_LedControl_Step_LedCommand_LedCommand(boolean u)
{
    ledCommand = u;
    Led_Hw_Write(ledCommand);
}

boolean* Rte_IWriteRef_SWC_LedControl_Step_LedCommand_LedCommand(void)
{
    return &ledCommand;
}