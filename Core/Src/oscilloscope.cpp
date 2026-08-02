#include "main.h"
#include "adc.h"
#include "tim.h"
#include "dma.h"
#include "dac.h"
#include "cmsis_os2.h"
#include "array"
#include "string"
#include "span"
using namespace std;

constexpr size_t data_frame_size = 200;

alignas(uint32_t) static array<uint16_t, data_frame_size * 2> adcBuffer{};

constexpr auto adc1stHalf = span(adcBuffer).first<data_frame_size>();
constexpr auto adc2ndHalf = span(adcBuffer).last<data_frame_size>();

alignas(uint32_t) static array<uint16_t, data_frame_size> transmitBuffer{};

extern osSemaphoreId_t transmitBufferBusyHandle;
extern osSemaphoreId_t readyToTransmitHandle;


void dmaMemToMemCallback(DMA_HandleTypeDef* dma_handle_type_def);

void initialize_test_signal() //todo remove  together with hdac2 triangle wave generation
{
    HAL_DAC_Start(&hdac2, DAC_CHANNEL_1);
    HAL_TIM_Base_Start(&htim2);
}

[[noreturn]] void run_oscilloscope()
{
    BSP_COM_SelectLogPort(COM1);

    initialize_test_signal();

    HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_channel2, HAL_DMA_XFER_CPLT_CB_ID, dmaMemToMemCallback);
    HAL_ADCEx_MultiModeStart_DMA(&hadc1, reinterpret_cast<uint32_t*>(adcBuffer.data()), adcBuffer.size() / 2);
    HAL_TIM_Base_Start(&htim3);

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


static constexpr char BASE64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Converts a 12-bit value (stored in uint16_t) to a 2-character Base64 string.
 * @param value The 12-bit integer (0-4095)
 * @return A string of length 2
 */
std::string uint12_to_base64(uint16_t value)
{
    // Ensure we only look at the lower 12 bits
    value &= 0x0FFF;

    // Split the 12 bits into two 6-bit chunks
    // Chunk 1: Bits 0-5 (Lower 6 bits)
    // Chunk 2: Bits 6-11 (Upper 6 bits)
    uint8_t high6 = (value >> 6) & 0x3F;
    uint8_t low6 = value & 0x3F;

    // Note: In standard Base64, the "high" part of a byte comes first.
    // Depending on your specific protocol, you might need to swap these.
    // Usually, for a single 12-bit word, it maps as follows:
    return {BASE64_CHARS[high6], BASE64_CHARS[low6]};
}

extern "C" [[noreturn]] void startTransmitTask([[maybe_unused]] void* argument)
{
    static array<char, data_frame_size * 2 + 1> dataBuffer;

    while (true)
    {
        osSemaphoreAcquire(readyToTransmitHandle, osWaitForever/*todo add fail detection*/);
        BSP_LED_Toggle(LED_GREEN);
        auto textPtr = dataBuffer.begin();
        for (auto val : transmitBuffer)
        {
            val &= 0x0FFF;

            // Split the 12 bits into two 6-bit chunks
            // Chunk 1: Bits 0-5 (Lower 6 bits)
            // Chunk 2: Bits 6-11 (Upper 6 bits)
            const uint8_t high6 = (val >> 6) & 0x3F;
            const uint8_t low6 = val & 0x3F;

            // Note: In standard Base64, the "high" part of a byte comes first.
            // Depending on your specific protocol, you might need to swap these.
            // Usually, for a single 12-bit word, it maps as follows:
            *(textPtr++) = BASE64_CHARS[high6];
            *(textPtr++) = BASE64_CHARS[low6];
        }
        *textPtr = 0;
        constexpr auto dataHeader = R"(
[start]
sampling.freq=100
shift.a=0
gain.a=1
data.a=)";
        fputs(dataHeader, stdout);
        puts(dataBuffer.data());
        puts("[stop]");
        osSemaphoreRelease(transmitBufferBusyHandle);
    }
}
