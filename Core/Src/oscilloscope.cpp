#include "main.h"
using namespace std;
[[noreturn]]void run_oscilloscope()
{
    while (true)
    {
        constexpr auto my_data = R"(
[start]
sampling.freq=100
shift.a=0
gain.a=1
data.a=abbcf100
        )";
        printf(my_data);
        HAL_Delay(500);
    }
}
