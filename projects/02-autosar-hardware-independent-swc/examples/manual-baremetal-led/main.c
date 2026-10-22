#include <stdint.h>

#include "LedControl.h"
#include "Led_Hw.h"


static void Delay(volatile uint32_t count)
{
    while (count > 0U)
    {
        __asm volatile ("nop");
        count--;
    }
}


int main(void)
{
    Led_Hw_Init();
    LedControl_Init();

    while (1)
{
    LedControl_Step();
    Delay(50000U);

    LedControl_Step();
    Delay(50000U);

    LedControl_Step();
    Delay(50000U);

    LedControl_Step();
    Delay(500000U);
}
}