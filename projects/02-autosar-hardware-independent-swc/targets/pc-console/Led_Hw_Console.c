#include <stdio.h>

#include "Led_Hw.h"

void Led_Hw_Init(void)
{
    printf("LED hardware initialized\n");
}

void Led_Hw_Write(boolean state)
{
    if (state == TRUE)
    {
        printf("LED ON\n");
    }
    else
    {
        printf("LED OFF\n");
    }
}