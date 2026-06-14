#include "main.h"

[[noreturn]]void run_oscilloscope()
{
    while (true)
    {
        printf("Hello World!\n\r");
        HAL_Delay(500);
    }
}
