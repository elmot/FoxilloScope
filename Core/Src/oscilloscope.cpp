#include "tim.h"
#include "dma.h"
#include "dac.h"
#include <array>
#include <atomic>
#include <span>
#include "oscilloscope.hpp"
#include <cstring>
#include <algorithm>

#include "cmsis_os2.h"
#include "comp.h"
#include "opamp.h"

alignas(uint32_t) static std::array<uint16_t, data_frame_size * 2> adcBufferA{};

alignas(uint32_t) static std::array<uint16_t, data_frame_size * 2> adcBufferB{};

constexpr auto bufferHalves = std::array{
    std::pair(std::span(adcBufferA).first<data_frame_size>(),
              std::span(adcBufferB).first<data_frame_size>()),

    std::pair(std::span(adcBufferA).last<data_frame_size>(),
              std::span(adcBufferB).last<data_frame_size>())
};

void dmaMemToMemCallback(DMA_HandleTypeDef* dma_handle_type_def);

void initialize_test_signal() //todo remove together with tim2 & hdac1 wave generation
{
    extern const unsigned short fake_signal[];
    HAL_DAC_Start_DMA(&hdac1, DAC1_CHANNEL_1, reinterpret_cast<const uint32_t*>(fake_signal), 164, DAC_ALIGN_12B_R);
    HAL_DAC_Start(&hdac1, DAC1_CHANNEL_2);
    //__HAL_TIM_SET_PRESCALER(&htim15, 30000);
    HAL_TIM_Base_Start(&htim15);
}

/** Oscilloscope commands
 *
 */

constexpr struct CommandTimeResolution_t : Command
{
    static constexpr long minAdcTime = 1'000'000'000LL / 4'000'000; // 4 MHz ADC max sampling

    constexpr CommandTimeResolution_t() : Command("sampling.ns", 250,
                                                  1'000'000'000LL / 8'000'000, // 8 MHz max sampling freq in nsec
                                                  1'000'000'000LL / 20, // 20 Hz min sampling freq in nsec
                                                  true) {}

    bool isInterleaveSampling() const
    {
        return value < minAdcTime;
    }

    uint32_t clockDivider() const
    {
        if (LL_RCC_GetAPB1Prescaler() != LL_RCC_APB1_DIV_1)
        {
            //APB1 divider must be 1
            Error_Handler();
        }
        // twice slower if interleaved sampling
        unsigned long long fullDivider = (isInterleaveSampling()) ? 2 : 1;
        fullDivider *= HAL_RCC_GetPCLK1Freq() * static_cast<unsigned long long>(value) /
            1'000'000'000ULL;
        return static_cast<uint32_t>(fullDivider - 1);
    }

    void useNewValue() const override
    {
    }
} CommandTimeResolution{};


static void startSampling();

namespace trigger
{
    std::atomic<TriggerState> state  = TriggerState::DISARMED;
    std::atomic<int> pre_arming  = 0;

    constexpr struct CommandTriggerLevel_t : Command
    {
        constexpr CommandTriggerLevel_t() : Command("trg.level", 200'000L/*todo 0*/, -1'000'000, 1'000'000)
        {
        }

        void useNewValue() const override
        {
            constexpr long long dac_max_long_long = DAC_MAX_VALUE;
            const uint16_t dac_bias = std::ranges::clamp((max - value) * dac_max_long_long / (max - min), 0LL, dac_max_long_long);
            HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1,DAC_ALIGN_12B_R, dac_bias);
        }
    } CommandTriggerLevel{};


    constexpr struct CommandTriggerType_t : Command
    {
        constexpr CommandTriggerType_t() : Command("trg.type", 0, -1, 1)
        {
        }

        void useNewValue() const override { startSampling(); }
    } CommandTriggerType{};

    constexpr struct CommandTriggerOffset_t : Command
    {
        constexpr CommandTriggerOffset_t() : Command("trg.time.offset", 0, -1'000'000, 1'000'000)
        {
        }

        void useNewValue() const override {}

        int timerShiftSamples() const
        {
            constexpr long range = data_frame_size;
            return static_cast<int>(range * value / min);
        }
    } CommandTriggerOffset{};

    constexpr struct CommandTriggerChannel_t : Command
    {
        constexpr CommandTriggerChannel_t() : Command("trg.chan", 0, 0, 1)
        {
        }

        void useNewValue() const override
        {
            startSampling();
        }
    } CommandTriggerChannel{};

    void enableTrigger()
    {
        if (CommandTriggerType.getValue() == 0)
        {
            HAL_NVIC_DisableIRQ(COMP1_2_3_IRQn);
        }
        else
        {
            trigger::state = TriggerState::DISARMED;
            HAL_NVIC_EnableIRQ(COMP1_2_3_IRQn);
        }
    }

    uint32_t comparatorValue()
    {
        return CommandTriggerType.getValue() == -1 ? COMP_OUTPUT_LEVEL_HIGH : COMP_OUTPUT_LEVEL_LOW;
    }

}

constexpr CommandGainChannel_t CommandGainChannelA{"gain.a", &hopamp4};

constexpr CommandGainChannel_t CommandGainChannelB{"gain.b", &hopamp3};

constexpr CommandBiasChannel_t CommandBiasChannelA{"vbias.a", &hdac4,DAC_CHANNEL_1};

constexpr CommandBiasChannel_t CommandBiasChannelB{"vbias.b", &hdac3,DAC_CHANNEL_2};

constexpr std::array<const Command*, 9> commands{
    &CommandBiasChannelA,
    &CommandBiasChannelB,
    &CommandGainChannelA,
    &CommandGainChannelB,
    &CommandTimeResolution,
    &trigger::CommandTriggerLevel,
    &trigger::CommandTriggerType,
    &trigger::CommandTriggerOffset,
    &trigger::CommandTriggerChannel
};

void skipWhiteSpace(char* & ptr)
{
    while (isspace(static_cast<unsigned char>(*ptr)))
    {
        ptr++;
    }
}

static void startSampling()
{
    if (trigger::CommandTriggerChannel.getValue() == 0)
    {
        __HAL_COMP_COMP3_EXTI_ENABLE_IT();
        __HAL_COMP_COMP1_EXTI_DISABLE_IT();
    }
    else
    {
        __HAL_COMP_COMP1_EXTI_ENABLE_IT();
        __HAL_COMP_COMP3_EXTI_DISABLE_IT();
    }
    int arr = trigger::CommandTriggerOffset.timerShiftSamples() + static_cast<int>(data_frame_size);
    if (arr < 0) arr = 0;
    if (CommandTimeResolution.isInterleaveSampling()) arr /= 2;
    arr = std::ranges::clamp(arr, 1, 1000);
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, arr);
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    startMainAdcs(CommandTimeResolution.isInterleaveSampling(), adcBufferA.data(), adcBufferB.data(),
                  adcBufferA.size());
    __HAL_TIM_SET_PRESCALER(&htim2, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim2, CommandTimeResolution.clockDivider());
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    HAL_TIM_Base_Start(&htim2);
    HAL_NVIC_ClearPendingIRQ(COMP1_2_3_IRQn);
    trigger::pre_arming = trigger::CommandTriggerOffset.timerShiftSamples() < 0 ? 2 : 0;
    trigger::enableTrigger();
}

static void executeIncomingCommand()
{
    static char cmdBuffer[2064];
    cmdBuffer[0] = 0;
    for (size_t i = 0; true; i = (i + 1) % sizeof(cmdBuffer))
    {
        osMessageQueueGet(cmdRxQueueHandle, &cmdBuffer[i], nullptr,osWaitForever);
        if (cmdBuffer[i] == '\n' || cmdBuffer[i] == '\r')
        {
            cmdBuffer[i] = 0;
            break;
        }
    }

    char* noSpacePtr = cmdBuffer;
    skipWhiteSpace(noSpacePtr);
    bool requiresRestart = false;
    for (const auto& command : commands)
    {
        char* ptr = noSpacePtr;
        if (strncmp(ptr, command->name.data(), command->name.size()) != 0) continue;
        ptr += command->name.size();
        skipWhiteSpace(ptr);
        if (*ptr++ != '=') continue;

        long newValue;
        auto [cookie_ptr,errc] = std::from_chars(ptr, ptr + strlen(ptr), newValue);
        if (errc != std::errc{}) break;
        if (command->setValue(newValue))
        {
            requiresRestart |= command->requires_restart;
            break;
        }
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

    for (const auto opamp : {&hopamp3,&hopamp4,&hopamp5,&hopamp6})
    {
        HAL_OPAMP_Start(opamp);
        HAL_OPAMP_SelfCalibrate(opamp);
    }

    HAL_DAC_Start(&hdac3, DAC_CHANNEL_1);
    HAL_DAC_Start(&hdac3, DAC_CHANNEL_2);
    HAL_DAC_Start(&hdac4, DAC_CHANNEL_1);
    HAL_DAC_Start(&hdac2, DAC2_CHANNEL_1);
    HAL_DAC_SetValue(&hdac2, DAC2_CHANNEL_1, DAC_ALIGN_12B_R, (DAC_MAX_VALUE + 1) / 2);
    TIM_CCxChannelCmd(htim1.Instance, TIM_CHANNEL_1, TIM_CCx_ENABLE);
    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIM_Base_Start(&htim1);
    startSampling();
    HAL_COMP_Start(&hcomp1);
    HAL_COMP_Start(&hcomp3);
    for (const auto& command : commands)
    {
        command->useNewValue();
    }
    startUartInput();
    osTimerStart(partialFrameTimerHandle, msec_to_ticks(50));
    while (true)
    {
        executeIncomingCommand();
    }
}

//std::atomic<bool> fullFrameSent = false;
std::atomic<int> partialSamplesSent = false;

void initPartialFrameTransfer(const int subBufferIndex,const int start_index, const size_t length)
{
    --trigger::pre_arming;
    const auto& [fromA, fromB] = bufferHalves[subBufferIndex];
    if (osSemaphoreAcquire(transmitBuffer.semaphore, 0) != osOK)
    {
        //transmit buffer busy, skip the frame
        return;
    }
    transmitBuffer.length = length;
    transmitBuffer.head = start_index == 0;
    HAL_DMA_Start(&hdma_memtomem_dma1_channel6,
                  reinterpret_cast<uint32_t>(fromA.data() + start_index),
                  reinterpret_cast<uint32_t>(transmitBuffer.samplesA.data()),
                  (length + 1) / 2);
    HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel2,
                     reinterpret_cast<uint32_t>(fromB.data() + start_index),
                     reinterpret_cast<uint32_t>(transmitBuffer.samplesB.data()),
                     (length + 1) / 2);
}


extern "C" void initFrameTransfer(const int subBufferIndex)
{
    initPartialFrameTransfer(subBufferIndex,0, data_frame_size);
    partialSamplesSent = -1;
}

void signalTransmit()
{
    extern osThreadId_t transmitTaskHandle;
    osThreadFlagsSet(transmitTaskHandle, THREAD_FLAG_READY_TO_TRANSMIT);
}

void dmaMemToMemCallback([[maybe_unused]] DMA_HandleTypeDef* dma_handle_type_def)
{
    HAL_DMA_PollForTransfer(&hdma_memtomem_dma1_channel6, HAL_DMA_FULL_TRANSFER, 10000);
    signalTransmit();
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void HAL_COMP_TriggerCallback(COMP_HandleTypeDef* hcomp)
{
    if (trigger::pre_arming > 0) return;
    if (HAL_COMP_GetOutputLevel(hcomp) == trigger::comparatorValue())
    {
        if (trigger::state == TriggerState::ARMED)
        {
            trigger::state = TriggerState::TRIGGERED;
            __HAL_TIM_ENABLE(&htim1);
            HAL_NVIC_DisableIRQ(COMP1_2_3_IRQn);
        }
    }
    else
    {
        trigger::state = TriggerState::ARMED;
    }
}

extern "C" void HAL_TIM_PWM_PulseFinishedCallback([[maybe_unused]] TIM_HandleTypeDef* htim)
{
    HAL_TIM_Base_Stop_IT(&htim1);
    HAL_TIM_Base_Stop(&htim2);
    extern osThreadId_t keyFrameTaskHandle;
    osThreadFlagsSet(keyFrameTaskHandle, THREAD_FLAG_KEY_FRAME_DETECTED);
}

extern "C" [[noreturn]] void keyFramesProcessing([[maybe_unused]] void*)
{
    while (true)
    {
        osThreadFlagsWait(THREAD_FLAG_KEY_FRAME_DETECTED,osFlagsNoClear,osWaitForever);
        osSemaphoreAcquire(transmitKeyBuffer.semaphore, osWaitForever);
        transmitKeyBuffer.length = data_frame_size;
        transmitKeyBuffer.head = true;

        const auto dma_samples_left = adcSamplesLeft();
        if (dma_samples_left <= data_frame_size)
        {
            const auto frame_start_position = (adcBufferA.size() - dma_samples_left) - data_frame_size;
            memcpy(&transmitKeyBuffer.samplesA[0], &adcBufferA[frame_start_position],
                   data_frame_size * sizeof (adcBufferA[0]));
            memcpy(&transmitKeyBuffer.samplesB[0], &adcBufferB[frame_start_position],
                   data_frame_size * sizeof (adcBufferB[0]));
        }
        else
        {
            const auto first_chunk_len = dma_samples_left - data_frame_size;
            memcpy(&transmitKeyBuffer.samplesA[0], &adcBufferA[adcBufferA.size() - first_chunk_len],
                   first_chunk_len * sizeof (adcBufferA[0]));
            memcpy(&transmitKeyBuffer.samplesB[0], &adcBufferB[adcBufferB.size() - first_chunk_len],
                   first_chunk_len * sizeof (adcBufferB[0]));

            memcpy(&transmitKeyBuffer.samplesA[first_chunk_len], &adcBufferA[0],
                   (data_frame_size - first_chunk_len) * sizeof (adcBufferA[0]));
            memcpy(&transmitKeyBuffer.samplesB[first_chunk_len], &adcBufferB[0],
                   (data_frame_size - first_chunk_len) * sizeof (adcBufferB[0]));
        }
        osThreadFlagsClear(THREAD_FLAG_KEY_FRAME_DETECTED);
        transmitKeyBufferReady = true;
        signalTransmit();
        startSampling();
        trigger::enableTrigger();
        partialSamplesSent = -1;
    }
}

static std::pair<long, long> calculate_min_max_uV(const CommandGainChannel_t& gain, const CommandBiasChannel_t& bias)
{
    const long long D = bias.get_12bit_bias();
    const long long G = gain.getValue();
    const long long v_ref = analog_supply_voltage_mV * 1000LL;
    const long long amplitude_uV = analog_supply_voltage_mV * 1000LL / gain.getValue();
    const long long vmax_uV = (1LL + G) * D * v_ref / G / DAC_MAX_VALUE - v_ref / 2;
    const long long vmin_uV = vmax_uV - amplitude_uV;

    return {vmin_uV, vmax_uV};
}

void writeCommands()
{
    for (auto& command : commands)
    {
        command->write();
    }
    writeUart("vltg.steps=" ADC_MAX_VALUE_STR "\n");//todo correct +1
    const auto [minA, maxA] = calculate_min_max_uV(CommandGainChannelA, CommandBiasChannelA);
    const auto [minB, maxB] = calculate_min_max_uV(CommandGainChannelB, CommandBiasChannelB);
    Command::do_write_value("vltg.min.uv.a", minA);
    Command::do_write_value("vltg.max.uv.a", maxA);
    Command::do_write_value("vltg.min.uv.b", minB);
    Command::do_write_value("vltg.max.uv.b", maxB);
}

extern "C" void partialFrameSend([[maybe_unused]] void*)
{
    if (partialSamplesSent < 0)
    {
        partialSamplesSent = 0;
        return;
    }
    unsigned int dma_samples_left = adcSamplesLeft();
    const int subBufferIndex = dma_samples_left > data_frame_size ? 0 : 1;
    dma_samples_left %= data_frame_size;

    constexpr int lastSubFrameThresholdLow = data_frame_size * 5 / 100;
    constexpr int lastSubFrameThresholdHigh = data_frame_size * 95 / 100;
    constexpr int garbageDmaTail = 1;

    const int measuredSamples = static_cast<int>(data_frame_size) - dma_samples_left - garbageDmaTail;

    if (measuredSamples > lastSubFrameThresholdLow
        && measuredSamples < lastSubFrameThresholdHigh
        && partialSamplesSent >= 0
        && partialSamplesSent < measuredSamples)
    {
        initPartialFrameTransfer(subBufferIndex, partialSamplesSent, measuredSamples - partialSamplesSent);
        partialSamplesSent = measuredSamples;
    }
}
