#include "oscilloscope.hpp"
#include "string"

using namespace std;

 array<uint16_t, data_frame_size> transmitBuffer{};

static constexpr char BASE64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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
