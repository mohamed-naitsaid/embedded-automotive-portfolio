#include <stdbool.h>
#include "stm32f1xx.h"

#include "Led_Hw.h"


void Led_Hw_Init(void)
{
    RCC->APB2ENR |= (1U << 4);

    GPIOC->CRH &= ~(0xFU << 20);
    GPIOC->CRH |=  (0x2U << 20);

    GPIOC->BSRR = (1U << 13);
}


void Led_Hw_Write(bool state)
{
    if (state == true)
    {
        GPIOC->BRR = (1U << 13);
    }
    else
    {
        GPIOC->BSRR = (1U << 13);
    }
}