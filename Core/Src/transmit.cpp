#include "cmsis_os.h"
#include "oscilloscope.hpp"
#include "string"
#include "usart.h"

using namespace std;

transmitBuffer_t transmitBuffer{};

static constexpr char BASE64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void writeUart(const string_view& str);

extern "C" [[noreturn]] void startTransmitTask([[maybe_unused]] void* argument)
{
    static array<char, data_frame_size * 2 + 1> dataBuffer;

    while (true)
    {
        osThreadFlagsWait(THREAD_FLAG_READY_TO_TRANSMIT, osFlagsWaitAny, osWaitForever);
        auto textPtr = dataBuffer.begin();
        for (auto val : transmitBuffer.samples)
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
        writeUart("[frame]\n");
        writeCommands(writeUart);
        writeUart("data.a=");
        writeUart(dataBuffer.data());
        if (transmitBuffer.keyFrame)
        {
            writeUart("\nkeyframe=1");
        }
        writeUart("\n");
        osSemaphoreRelease(transmitBufferBusyHandle);
    }
}

/**  STDOUT substitution
 *
 */

volatile static osThreadId_t transmittingTaskHandle = nullptr;
constexpr uint32_t UART_TX_BUSY = 0x1;
void writeUart(const string_view& str)
{
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
