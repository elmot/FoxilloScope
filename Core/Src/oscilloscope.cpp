#include "adc.h"
#include "tim.h"
#include "dma.h"
#include "dac.h"
#include "array"
#include "string"
#include "span"
#include "oscilloscope.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>

#include "opamp.h"

using namespace std;

alignas(uint32_t) static array<uint16_t, data_frame_size * 2> adcBuffer{};

constexpr auto adc1stHalf = span(adcBuffer).first<data_frame_size>();
constexpr auto adc2ndHalf = span(adcBuffer).last<data_frame_size>();

void dmaMemToMemCallback(DMA_HandleTypeDef* dma_handle_type_def);

void initialize_test_signal() //todo remove together with tim2 & hdac2 wave generation
{
    extern const unsigned short fake_signal[];
    HAL_DAC_Start_DMA(&hdac2, DAC_CHANNEL_1, reinterpret_cast<const uint32_t*>(fake_signal), 140, DAC_ALIGN_12B_R);
    HAL_TIM_Base_Start(&htim2);
}

constexpr array<Command, 1> commands{
    {
        {
            .name = "vbias.a",
            .requiresRestart = false,
            .value = 0L,
            .useNewValue = [](long long value)
            {
                const uint16_t dac_bias = clamp((value + 1'000'000LL) * 4'095LL / 2'000'000LL, 0LL, 4095LL);
                HAL_DAC_SetValue(&hdac1, DAC1_CHANNEL_1,DAC_ALIGN_12B_R, dac_bias);
            }
        }
    }
};

static char* skipWhiteSpace(char* & ptr)
{
    while (isspace(static_cast<unsigned char>(*ptr)))
    {
        ptr++;
    }
    return ptr;
}

static void executeIncomingCommand()
{
    static char cmdBuffer[121];
    cmdBuffer[0] = 0;
    fgets(cmdBuffer, sizeof(cmdBuffer),stdin);
    char* ptr = cmdBuffer;
    skipWhiteSpace(ptr);
    bool updated = false;
    bool requiresRestart = false;
    for (const auto command : commands)
    {
        if (strncmp(ptr, command.name.data(), command.name.size()) != 0) continue;
        ptr+=command.name.size();
        skipWhiteSpace(ptr);
        if (*ptr++ != '=') continue;
        long long newValue = atoll(ptr);
        newValue = command.adjustValue(newValue);
        if (newValue == command.value) continue;
        command.useNewValue(newValue);
        updated = true;
        command.value = newValue;
        requiresRestart |= command.requiresRestart;
    }
}

[[noreturn]] void run_oscilloscope()
{
    initialize_test_signal();
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);
    HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_channel2, HAL_DMA_XFER_CPLT_CB_ID, dmaMemToMemCallback);
    HAL_ADC_Start(&hadc4);
    //todo apply all stored parameters
    HAL_ADCEx_MultiModeStart_DMA(&hadc3, reinterpret_cast<uint32_t*>(adcBuffer.data()), adcBuffer.size() / 2);
    HAL_OPAMP_Start(&hopamp3);
    HAL_DAC_Start(&hdac1, DAC1_CHANNEL_1);
    for (const auto command : commands)
    {
        command.useNewValue(command.value);
    }
    startUartInput();
    while (true)
    {
        executeIncomingCommand();
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
    osSemaphoreRelease(notReadyToTransmitHandle);
}
