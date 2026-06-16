#include "adc.h"
#include "tim.h"
#include "dma.h"
#include "dac.h"
#include "array"
#include "string"
#include "span"
#include "oscilloscope.hpp"

#include "opamp.h"

using namespace std;

alignas(uint32_t) static array<uint16_t, data_frame_size * 2> adcBuffer{};

constexpr auto adc1stHalf = span(adcBuffer).first<data_frame_size>();
constexpr auto adc2ndHalf = span(adcBuffer).last<data_frame_size>();

void dmaMemToMemCallback(DMA_HandleTypeDef* dma_handle_type_def);

void initialize_test_signal() //todo remove together with tim2 & hdac2 wave generation
{
    extern const unsigned short fake_signal [];
    HAL_DAC_Start_DMA(&hdac2, DAC_CHANNEL_1, reinterpret_cast<const uint32_t*>(fake_signal), 140, DAC_ALIGN_12B_R);
    HAL_TIM_Base_Start(&htim2);
}

[[noreturn]] void run_oscilloscope()
{
    BSP_COM_SelectLogPort(COM1);

    initialize_test_signal();
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);
    HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_channel2, HAL_DMA_XFER_CPLT_CB_ID, dmaMemToMemCallback);
    HAL_ADC_Start(&hadc4);
    HAL_ADCEx_MultiModeStart_DMA(&hadc3, reinterpret_cast<uint32_t*>(adcBuffer.data()), adcBuffer.size() / 2);
    HAL_OPAMP_Start(&hopamp3);
    HAL_DAC_Start(&hdac1, DAC1_CHANNEL_1);
    HAL_DAC_SetValue(&hdac1, DAC1_CHANNEL_1,DAC_ALIGN_12B_R, 1020);

    while (true)
    {
        osDelay(10000); //todo what to do here?
    }
}

static void initFrameTransfer(const span<uint16_t, data_frame_size>& from)
{
    if (osSemaphoreAcquire(transmitBufferBusyHandle, 0) != osOK)
    {
        //transmit buffer busy
        return;
    }
    HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel2,
                     reinterpret_cast<uint32_t>(from.data()),
                     reinterpret_cast<uint32_t>(transmitBuffer.data()),
                     transmitBuffer.size() / 2);
}

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    initFrameTransfer(adc2ndHalf);
}

extern "C" void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    initFrameTransfer(adc1stHalf);
}

void dmaMemToMemCallback(DMA_HandleTypeDef* dma_handle_type_def)
{
    osSemaphoreRelease(readyToTransmitHandle);
}

