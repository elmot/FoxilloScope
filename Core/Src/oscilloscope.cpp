#include "main.h"
#include "adc.h"
#include "tim.h"
using namespace std;
constexpr size_t data_buffer_size = 2000;
uint16_t buffer[data_buffer_size];
[[noreturn]]void run_oscilloscope()
{
    HAL_ADC_Start_DMA(&hadc2,reinterpret_cast<uint32_t*>(buffer),data_buffer_size);
    HAL_TIM_Base_Start(&htim3);

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
