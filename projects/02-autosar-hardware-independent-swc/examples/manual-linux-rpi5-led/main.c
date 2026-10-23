#define _POSIX_C_SOURCE 199309L

#include <time.h>

#include "LedControl.h"
#include "Led_Hw.h"

static void Delay500ms(void)
{
    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = 500000000L;

    nanosleep(&delay, NULL);
}

int main(void)
{
    Led_Hw_Init();
    LedControl_Init();

    while (1)
    {
        LedControl_Step();
        Delay500ms();
    }

    return 0;
}
