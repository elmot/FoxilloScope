#include "tim.h"
#include "dma.h"
#include "dac.h"
#include "array"
#include "string"
#include "span"
#include "oscilloscope.hpp"
#include <cstring>
#include <algorithm>

#include "cmsis_os2.h"
#include "comp.h"
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
    HAL_TIM_Base_Start(&htim15);
}

/** Oscilloscope commands
 *
 */

constexpr Command CommandBiasChannelA{
            .name = "vbias.a",
    .value = -503'000LL, //todo 0
            .useNewValue = [](long long value)
            {
                const uint16_t dac_bias = clamp((value + 1'000'000LL) * 4'095LL / 2'000'000LL, 0LL, 4095LL);
                HAL_DAC_SetValue(&hdac1, DAC1_CHANNEL_1,DAC_ALIGN_12B_R, dac_bias);
            }
};

constexpr Command CommandTriggerLevel{
            .name = "trg_level",
    .value = 1200'000L, //todo 0
    .useNewValue = [](const long long value)
            {
                const uint16_t dac_bias = clamp((value + 1'000'000LL) * 4'095LL / 2'000'000LL, 0LL, 4095LL);
                HAL_DAC_SetValue(&hdac1, DAC1_CHANNEL_2,DAC_ALIGN_12B_R, dac_bias);
            }
};

namespace sampling
{
    constexpr auto minAdcTime = 1'000'000'000LL / 4'000'000; // 4 MHz ADC max sampling
    constexpr auto minSamplingTimeNs = 1'000'000'000LL / 8'000'000; // 8 MHz max sampling
    constexpr auto maxSamplingTimeNs = 1'000'000'000LL / 20; // 20 Hz max sampling
    constexpr Command CommandTimeResolution{
        .name = "sampling.ns",
        .requiresRestart = true,
        .value = 250,
        .adjustValue = [](const long long value) { return clamp(value, minSamplingTimeNs, maxSamplingTimeNs); }
    };

    bool isInterleaveSampling()
    {
        return CommandTimeResolution.value < minAdcTime;
    }

    uint32_t clockDivider()
    {
        //Hopefully APB1 divider is 1
        // twice slower if interleaved sampling
        unsigned long long fullDivider = (isInterleaveSampling()) ? 2 : 1;
        fullDivider *= HAL_RCC_GetHCLKFreq() * static_cast<unsigned long long>(CommandTimeResolution.value) /
            1'000'000'000ULL;
        return static_cast<uint32_t>(fullDivider - 1);
        }
    }

constexpr array<const Command*, 3> commands{
    {
        &CommandBiasChannelA,
        &CommandTriggerLevel,
        &sampling::CommandTimeResolution
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

static volatile bool triggerArmed = false;

static void startSampling()
{
    startMainAdc(sampling::isInterleaveSampling(), adcBuffer.data(), adcBuffer.size());
    __HAL_TIM_SET_PRESCALER(&htim2, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim2, sampling::clockDivider());
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    //todo set trigger
    HAL_TIM_Base_Start(&htim2);
    HAL_NVIC_ClearPendingIRQ(COMP4_5_6_IRQn);
    triggerArmed = false;

}
static void executeIncomingCommand()
{
    static char cmdBuffer[121];
    cmdBuffer[0] = 0;
    for (size_t i = 0; true; i = (i + 1) % sizeof(cmdBuffer))
    {
        osMessageQueueGet(cmdRxQueueHandle, &cmdBuffer[i], 0,osWaitForever);
        if (cmdBuffer[i] == '\n' || cmdBuffer[i] == '\r')
        {
            cmdBuffer[i] = 0;
            break;
        }
    }

    char* ptr = cmdBuffer;
    skipWhiteSpace(ptr);
    bool requiresRestart = false;
    for (const auto command : commands)
    {
        if (strncmp(ptr, command->name.data(), command->name.size()) != 0) continue;
        ptr += command->name.size();
        skipWhiteSpace(ptr);
        if (*ptr++ != '=') continue;
        long long newValue = atoll(ptr);
        newValue = command->adjustValue(newValue);
        if (newValue == command->value) continue;
        command->useNewValue(newValue);
        command->value = newValue;
        requiresRestart |= command->requiresRestart;
        break;
    }
    if (requiresRestart)
    {
        startSampling();
}
}

[[noreturn]] void run_oscilloscope()
{
    initialize_test_signal();
    adcCalibration();
    HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_channel2, HAL_DMA_XFER_CPLT_CB_ID, dmaMemToMemCallback);
    HAL_OPAMP_Start(&hopamp3);
    HAL_DAC_Start(&hdac1, DAC1_CHANNEL_1);
    HAL_DAC_Start(&hdac1, DAC1_CHANNEL_2);
    TIM_CCxChannelCmd(htim1.Instance, TIM_CHANNEL_1, TIM_CCx_ENABLE);
    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIM_Base_Start(&htim1);
    startSampling();
    HAL_COMP_Start(&hcomp5);
    for (const auto command : commands)
    {
        command->useNewValue(command->value);
    }
    startUartInput();
    while (true)
    {
        executeIncomingCommand();
    }
}

extern "C" void initFrameTransfer(const int subBufferIndex)
{
    const span<uint16_t, data_frame_size>& from = subBufferIndex ? adc1stHalf : adc2ndHalf;
    if (osSemaphoreAcquire(transmitBufferBusyHandle, 0) != osOK)
    {
        //transmit buffer busy, skip the frame
        return;
    }
    transmitBuffer.keyFrame = false;
    HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel2,
                     reinterpret_cast<uint32_t>(from.data()),
                     reinterpret_cast<uint32_t>(transmitBuffer.samples.data()),
                     transmitBuffer.samples.size() / 2);
}

void transmitBufferReady()
{
    extern osThreadId_t transmitTaskHandle;
    osThreadFlagsSet(transmitTaskHandle, THREAD_FLAG_READY_TO_TRANSMIT);
}

void dmaMemToMemCallback(DMA_HandleTypeDef* dma_handle_type_def)
{
    UNUSED(dma_handle_type_def);
    transmitBufferReady();
}

void HAL_COMP_TriggerCallback(COMP_HandleTypeDef* hcomp)
{
    if (HAL_COMP_GetOutputLevel(hcomp))
    {
        if (triggerArmed)
        {
            __HAL_TIM_ENABLE(&htim1);
            HAL_NVIC_DisableIRQ(COMP4_5_6_IRQn);
        }
    }
    else
    {
        triggerArmed = true;
    }
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
}

extern "C" void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef* htim)
{
    UNUSED(htim);
    HAL_TIM_Base_Stop_IT(&htim1);
    HAL_TIM_Base_Stop(&htim2);
    extern osThreadId_t keyFrameTaskHandle;
    osThreadFlagsSet(keyFrameTaskHandle, THREAD_FLAG_KEY_FRAME_DETECTED);
}

extern "C" [[noreturn]] void keyFramesProcessing()
{
    while (true)
    {
        osThreadFlagsWait(THREAD_FLAG_KEY_FRAME_DETECTED,osFlagsNoClear,osWaitForever);
        osSemaphoreAcquire(transmitBufferBusyHandle, osWaitForever);
        transmitBuffer.keyFrame = true;

        const auto dma_samples_left = adcSamplesLeft();
        if (dma_samples_left <= data_frame_size)
        {
            const auto frame_start_position = (adcBuffer.size() - dma_samples_left) - data_frame_size;
            memcpy(&transmitBuffer.samples[0], &adcBuffer[frame_start_position],
                   data_frame_size * sizeof (adcBuffer[0]));
        }
        else
        {
            const auto first_chunk_len = dma_samples_left - data_frame_size;
            memcpy(&transmitBuffer.samples[0], &adcBuffer[adcBuffer.size() - first_chunk_len],
                   first_chunk_len * sizeof (adcBuffer[0]));
            memcpy(&transmitBuffer.samples[first_chunk_len], &adcBuffer[0],
                   (data_frame_size - first_chunk_len) * sizeof (adcBuffer[0]));
        }
        osThreadFlagsClear(THREAD_FLAG_KEY_FRAME_DETECTED);
        transmitBufferReady();
        startSampling();
        HAL_NVIC_EnableIRQ(COMP4_5_6_IRQn);
    }
}
