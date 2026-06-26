#include <cstring>
#include <span>

#include "cmsis_os.h"
#include "oscilloscope.hpp"
#include "string"
#include "usart.h"

using namespace std;

transmitBuffer_t transmitBuffer{};

static constexpr char BASE64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static constexpr size_t ascii_buffer_size = data_frame_size * 2 + 2;

static std::span<char> encode_bin_buffer(
    const std::span<uint16_t>& samples,
    array<char, ascii_buffer_size>& asciiBuffer)
{
    auto textPtr = asciiBuffer.begin();
    for (auto val : samples)
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
    *textPtr++ = '\n';
    *textPtr = 0;
    return std::span{asciiBuffer.begin(), textPtr};
}


template <unsigned long N>
constexpr auto to_constexpr_string_cr()
{
    constexpr auto count_digits = []() constexpr -> size_t
    {
        char buf[100];
        return to_chars(buf, buf + sizeof(buf), N).ptr - buf;
    };
    constexpr size_t len = count_digits(); // (Use the count_digits func from earlier)
    array<char, len + 1> arr{};
    to_chars(arr.begin(), arr.end() - 1, N).ptr[0] = '\n';
    return arr;
}

static_assert(string_view(to_constexpr_string_cr<182>()) == string_view("182\n"));

extern "C" [[noreturn]] void startTransmitTask([[maybe_unused]] void* argument)
{
    static array<char, ascii_buffer_size> asciiBufferA;
    static array<char, ascii_buffer_size> asciiBufferB;

    while (true)
    {
        osThreadFlagsWait(THREAD_FLAG_READY_TO_TRANSMIT, osFlagsWaitAny, osWaitForever);
        writeUart("[frame]\nframe.size=");
        writeUart(string_view(to_constexpr_string_cr<data_frame_size>()));
        if (transmitBuffer.keyFrame)
        {
            writeUart("keyframe=1\n");
        }
        writeCommands();
        const auto& encodedA = encode_bin_buffer(span{transmitBuffer.samplesA.begin(),transmitBuffer.samplesA.size()}, asciiBufferA);
        writeUart("data.a=");
        writeUart(string_view{encodedA});
        const auto& encodedB = encode_bin_buffer(span{transmitBuffer.samplesB.begin(),transmitBuffer.samplesB.size()}, asciiBufferB);
        writeUart("data.b=");
        writeUart(string_view{encodedB});
        osSemaphoreRelease(transmitBufferBusyHandle);
    }
}

static atomic<osThreadId_t> transmittingTaskHandle = nullptr;
constexpr uint32_t UART_TX_BUSY = 0x1;

void writeUart(const string_view& str)
{
    if(str.empty()) return;
    transmittingTaskHandle = osThreadGetId();
    // Disable the DMA channel to allow configuration
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4); // Use your specific DMA and Channel/Stream

    // Clear any prior transfer complete or error flags
    LL_DMA_ClearFlag_TC4(DMA1);
    LL_DMA_ClearFlag_TE4(DMA1);

    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_4, reinterpret_cast<uint32_t>(str.data()));
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_4, str.size());

    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_4, reinterpret_cast<uint32_t>(&LPUART1->TDR));

    // Clear any pending notifications before starting the hardware
    osThreadFlagsClear(UART_TX_BUSY);
    // Enable the DMA Channel
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_4);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_4);
    LL_LPUART_EnableDMAReq_TX(LPUART1);

    // Wait for the specific thread flag to be set by the ISR
    // This is immune to the race condition because even if the flag is set 1 microsecond
    // BEFORE this line executes, osThreadFlagsWait reads the already-set flag and moves on.
    osThreadFlagsWait(UART_TX_BUSY, osFlagsWaitAny, osWaitForever);
}

extern "C" void lpuart1TransferComplete()
{
    if (LL_DMA_IsActiveFlag_TC4(DMA1))
    {
        // Clear the DMA interrupt flag
        LL_DMA_ClearFlag_TC4(DMA1);

        // Disable the UART DMA TX Request bit
        LL_LPUART_DisableDMAReq_TX(LPUART1);

        // Disable the DMA Channel (required before re-configuring NDTR for the next block)
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);

        if (transmittingTaskHandle != nullptr)
        {
            // Signal the waiting task directly
            osThreadFlagsSet(transmittingTaskHandle, UART_TX_BUSY);
        }
    }
}


void startUartInput()
{
    LL_LPUART_EnableIT_RXNE(LPUART1);
}

extern "C" void lpuart1ReadByte(const uint8_t rxByte)
{
    osMessageQueuePut(cmdRxQueueHandle, &rxByte, 0, 0);
}
