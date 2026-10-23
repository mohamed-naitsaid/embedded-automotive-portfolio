#include "Rte_LedControl.h"
#include "Led_Hw.h"

void Rte_Write_LedCommand(bool value)
{
    Led_Hw_Write(value);
}


