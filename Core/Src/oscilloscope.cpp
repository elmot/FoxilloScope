#include "tim.h"
#include "dma.h"
#include "dac.h"
#include <array>
#include <atomic>
#include <string>
#include <span>
#include "oscilloscope.hpp"
#include <cstring>
#include <algorithm>

#include "cmsis_os2.h"
#include "comp.h"
#include "opamp.h"

alignas(uint32_t) static std::array<uint16_t, data_frame_size * 2> adcBufferA{};

alignas(uint32_t) static std::array<uint16_t, data_frame_size * 2> adcBufferB{100, 1000, 2000, 3000, 4000};

constexpr auto adcA1stHalf = std::span(adcBufferA).first<data_frame_size>();
constexpr auto adcA2ndHalf = std::span(adcBufferA).last<data_frame_size>();

constexpr auto adcB1stHalf = std::span(adcBufferB).first<data_frame_size>();
constexpr auto adcB2ndHalf = std::span(adcBufferB).last<data_frame_size>();

void dmaMemToMemCallback(DMA_HandleTypeDef* dma_handle_type_def);

void initialize_test_signal() //todo remove together with tim2 & hdac2 wave generation
{
    extern const unsigned short fake_signal[];
    HAL_OPAMP_Start(&hopamp5);
    HAL_DAC_Start_DMA(&hdac4, DAC_CHANNEL_2, reinterpret_cast<const uint32_t*>(fake_signal), 164, DAC_ALIGN_12B_R);
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
                                                  true)
    {
    }

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
    constexpr struct CommandTriggerLevel_t : Command
    {
        constexpr CommandTriggerLevel_t() : Command("trg.level", 200'000L/*todo 0*/, -1'000'000, 1'000'000)
        {
        }

        void useNewValue() const override
        {
            const uint16_t dac_bias = std::ranges::clamp((value - min) * 4'095LL / (max - min), 0LL, 4095LL);
            HAL_DAC_SetValue(&hdac3, DAC1_CHANNEL_1,DAC_ALIGN_12B_R, dac_bias);
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

        void useNewValue() const override
        {
        }


        int timerShiftSamples() const
        {
            return static_cast<int>(data_frame_size * (value - min)* 2 / (max - min)) ;
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
            HAL_NVIC_EnableIRQ(COMP1_2_3_IRQn);
        }
    }

    uint32_t comparatorValue()
    {
        return CommandTriggerType.getValue() == -1 ? COMP_OUTPUT_LEVEL_LOW : COMP_OUTPUT_LEVEL_HIGH;
    }
}

constexpr struct CommandStateNo_t : Command
{
    constexpr CommandStateNo_t() : Command("state.no", 0, 0, 0x7FFF'FFFF)
    {
    }

    void useNewValue() const override
    {
    }

    bool setValue(const long aValue, [[maybe_unused]] const unsigned long aStateNumber) const override
    {
        value = aValue;
        return true;
    }
} CommandStateNo{};

const std::array<const Command*, 10> commands{
    &CommandStateNo,
    &CommandBiasChannelA_ref,
    &CommandBiasChannelB_ref,
    &CommandGainChannelA_ref,
    &CommandGainChannelB_ref,
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

static volatile std::atomic<bool> triggerArmed = false;
static volatile std::atomic<int> triggerCounter;

static void startSampling()
{
    /* Select COMP1 input pin: PB1 or PA1*/
    MODIFY_REG(hcomp1.Instance->CSR, COMP_CSR_INPSEL,
               trigger::CommandTriggerChannel.getValue() == 0 ? COMP_INPUT_PLUS_IO2 : COMP_INPUT_PLUS_IO1);
    int arr = trigger::CommandTriggerOffset.timerShiftSamples() + data_frame_size / 2;
    triggerCounter = 2; //todo proper value
    if (arr < 0) arr = 0;
    if (CommandTimeResolution.isInterleaveSampling()) arr /= 2;
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, arr);
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    startMainAdcs(CommandTimeResolution.isInterleaveSampling(), adcBufferA.data(), adcBufferB.data(),
                  adcBufferA.size());
    __HAL_TIM_SET_PRESCALER(&htim2, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim2, CommandTimeResolution.clockDivider());
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    HAL_TIM_Base_Start(&htim2);
    HAL_NVIC_ClearPendingIRQ(COMP1_2_3_IRQn);
    triggerArmed = false;
    trigger::enableTrigger();
}

static void executeIncomingCommand()
{
    static char cmdBuffer[121];
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
        std::from_chars(ptr, ptr + strlen(ptr), newValue); // NOLINT(*-err34-c)
        if (command->setValue(newValue, CommandStateNo.getValue()))
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
    HAL_OPAMP_Start(&hopamp3);
    HAL_OPAMP_Start(&hopamp4);
    HAL_DAC_Start(&hdac1, DAC1_CHANNEL_1);
    HAL_DAC_Start(&hdac2, DAC2_CHANNEL_1);
    HAL_DAC_Start(&hdac3, DAC_CHANNEL_1);
    TIM_CCxChannelCmd(htim1.Instance, TIM_CHANNEL_1, TIM_CCx_ENABLE);
    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIM_Base_Start(&htim1);
    startSampling();
    HAL_COMP_Start(&hcomp1);
    for (const auto& command : commands)
    {
        command->useNewValue();
    }
    startUartInput();
    while (true)
    {
        executeIncomingCommand();
    }
}

extern "C" void initFrameTransfer(const int subBufferIndex)
{
    const std::span<uint16_t, data_frame_size>& fromA = subBufferIndex ? adcA1stHalf : adcA2ndHalf;
    const std::span<uint16_t, data_frame_size>& fromB = subBufferIndex ? adcB1stHalf : adcB2ndHalf;
    if (osSemaphoreAcquire(transmitBufferBusyHandle, 0) != osOK)
    {
        //transmit buffer busy, skip the frame
        return;
    }
    transmitBuffer.keyFrame = false;
    HAL_DMA_Start(&hdma_memtomem_dma1_channel6,
                  reinterpret_cast<uint32_t>(fromA.data()),
                  reinterpret_cast<uint32_t>(transmitBuffer.samplesA.data()),
                  transmitBuffer.samplesA.size() / 2);
    HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel2,
                     reinterpret_cast<uint32_t>(fromB.data()),
                     reinterpret_cast<uint32_t>(transmitBuffer.samplesB.data()),
                     transmitBuffer.samplesB.size() / 2);
}

void transmitBufferReady()
{
    extern osThreadId_t transmitTaskHandle;
    osThreadFlagsSet(transmitTaskHandle, THREAD_FLAG_READY_TO_TRANSMIT);
}

void dmaMemToMemCallback([[maybe_unused]] DMA_HandleTypeDef* dma_handle_type_def)
{
    HAL_DMA_PollForTransfer(&hdma_memtomem_dma1_channel6, HAL_DMA_FULL_TRANSFER, 10000);
    transmitBufferReady();
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void HAL_COMP_TriggerCallback(COMP_HandleTypeDef* hcomp)
{
    if (HAL_COMP_GetOutputLevel(hcomp) == trigger::comparatorValue())
    {
        if (triggerArmed)
        {
            triggerArmed = false;
            if (triggerCounter.fetch_sub(1) == 0)
            {
                __HAL_TIM_ENABLE(&htim1);
                HAL_NVIC_DisableIRQ(COMP1_2_3_IRQn);
            }

        }
    }
    else
    {
        triggerArmed = true;
    }
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
}

extern "C" void HAL_TIM_PWM_PulseFinishedCallback([[maybe_unused]] TIM_HandleTypeDef* htim)
{
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
            const auto frame_start_position = (adcBufferA.size() - dma_samples_left) - data_frame_size;
            memcpy(&transmitBuffer.samplesA[0], &adcBufferA[frame_start_position],
                   data_frame_size * sizeof (adcBufferA[0]));
            memcpy(&transmitBuffer.samplesB[0], &adcBufferB[frame_start_position],
                   data_frame_size * sizeof (adcBufferB[0]));
        }
        else
        {
            const auto first_chunk_len = dma_samples_left - data_frame_size;
            memcpy(&transmitBuffer.samplesA[0], &adcBufferA[adcBufferA.size() - first_chunk_len],
                   first_chunk_len * sizeof (adcBufferA[0]));
            memcpy(&transmitBuffer.samplesB[0], &adcBufferB[adcBufferB.size() - first_chunk_len],
                   first_chunk_len * sizeof (adcBufferB[0]));

            memcpy(&transmitBuffer.samplesA[first_chunk_len], &adcBufferA[0],
                   (data_frame_size - first_chunk_len) * sizeof (adcBufferA[0]));
            memcpy(&transmitBuffer.samplesB[first_chunk_len], &adcBufferB[0],
                   (data_frame_size - first_chunk_len) * sizeof (adcBufferB[0]));
        }
        osThreadFlagsClear(THREAD_FLAG_KEY_FRAME_DETECTED);
        transmitBufferReady();
        startSampling();
        trigger::enableTrigger();
    }
}

void writeCommands(void (*write_uart)(const std::string_view& str))
{
    for (auto& command : commands)
    {
        command->write(write_uart);
    }
}
