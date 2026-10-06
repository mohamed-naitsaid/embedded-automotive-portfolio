#include <time.h>

#include "SWC_LedControl.h"
#include "Led_Hw.h"

static void delay_ms(unsigned int ms)
{
    struct timespec delay;

    delay.tv_sec = ms / 1000U;
    delay.tv_nsec = (long)(ms % 1000U) * 1000000L;

    nanosleep(&delay, NULL);
}

int main(void)
{
    Led_Hw_Init();
    SWC_LedControl_Init();

    while (1)
    {
        SWC_LedControl_Step();
        delay_ms(500);
    }

    return 0;
}