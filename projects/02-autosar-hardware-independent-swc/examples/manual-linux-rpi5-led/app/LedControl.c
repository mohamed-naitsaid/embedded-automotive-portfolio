#include "LedControl.h"
#include "Rte_LedControl.h"

typedef struct
{
    bool UnitDelay_DSTATE;
} LedControl_Data;

static LedControl_Data ledData;

void LedControl_Init(void)
{
    ledData.UnitDelay_DSTATE = false;
}

void LedControl_Step(void)
{
    bool currentState;

    currentState = ledData.UnitDelay_DSTATE;

    Rte_Write_LedCommand(currentState);

    ledData.UnitDelay_DSTATE = !currentState;
}
