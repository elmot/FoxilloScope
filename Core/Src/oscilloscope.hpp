#ifndef G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
#define G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H

#include <algorithm>

#include "main.h"
#include "cmsis_os2.h"
#include <array>
#include <string>
#include <charconv>

void writeUart(const std::string_view& str);

struct Command
{
    constexpr Command(char const* name_v,
                      const long value_v, const long min_v, const long max_v,
                      const bool requires_restart_v = false) :
        name{name_v},
        requires_restart{requires_restart_v},
        value(value_v),
        min{min_v},
        max{max_v}
    {
    }

    virtual ~Command() = default;

    std::string_view name;
    const bool requires_restart{false};

    virtual void useNewValue() const = 0;

    long getValue() const { return value; }

    virtual bool setValue(long aValue, const unsigned long aStateNumber) const
    {
        if (aStateNumber <= stateNumber) return false;
        stateNumber = aStateNumber;
        aValue = adjustValue(aValue);
        if (aValue == value) return false;
        value = aValue;
        useNewValue();
        return true;
    }

    void write() const
    {
        writeUart(name);
        std::array<char,32> buffer{'='};
        auto [ptr, _] = std::to_chars(&buffer[1], &buffer.back(),value);
        *ptr++ ='\n';
        writeUart(std::string_view(buffer.data(), ptr - buffer.data()));
    }

protected:
    /** Mutable part*/
    mutable long value;
    mutable unsigned long stateNumber = 0;
    /**End of Mutable part*/
    const long min;
    const long max;
    virtual long adjustValue(const long aValue) const { return std::clamp(aValue, min, max); }
};

struct CommandGainChannel_t : Command
{
    constexpr CommandGainChannel_t(const char* name, OPAMP_HandleTypeDef * opamp) : Command(name, 16, 2, 64), opamp(opamp){}
    void useNewValue() const override;
protected:
    OPAMP_HandleTypeDef  * opamp;
    long adjustValue(long value) const override;
};

struct CommandBiasChannel_t : Command
{
    constexpr CommandBiasChannel_t(const char* name, DAC_HandleTypeDef* dac, uint32_t dac_channel)
        : Command(name, 0, -1'000'000, 1'000'000), dac{dac}, dac_channel{dac_channel}
    {
    }

    DAC_HandleTypeDef* dac;
    const uint32_t dac_channel;

    void useNewValue() const override
    {
        const uint16_t dac_bias = std::ranges::clamp(
            (max - value) * 4'095LL / (max - min), 0LL, 4095LL);
        HAL_DAC_SetValue(dac, dac_channel,DAC_ALIGN_12B_R, dac_bias);
    }
};

constexpr uint32_t THREAD_FLAG_READY_TO_TRANSMIT = 0x20;
constexpr uint32_t THREAD_FLAG_KEY_FRAME_DETECTED = 0x40;

constexpr size_t data_frame_size = 200;

extern osSemaphoreId_t transmitBufferBusyHandle; // NOLINT(*-dynamic-static-initializers)

struct transmitBuffer_t
{
    alignas(uint32_t) std::array<uint16_t, data_frame_size> samplesA;
    alignas(uint32_t) std::array<uint16_t, data_frame_size> samplesB;
    std::atomic<bool> keyFrame;
};

extern transmitBuffer_t transmitBuffer; // NOLINT(*-dynamic-static-initializers)

extern osMessageQueueId_t cmdRxQueueHandle; // NOLINT(*-dynamic-static-initializers)


void startUartInput();

extern "C" void adcCalibration();
extern "C" void startMainAdcs(bool interleaveSampling, uint16_t* bufferA, uint16_t* bufferB, size_t bufferLength);
extern "C" size_t adcSamplesLeft();

static uint32_t msec_to_ticks(uint32_t msec) {
    uint32_t ticks_per_sec = osKernelGetTickFreq(); // Usually 1000 Hz
    return (msec * ticks_per_sec) / 1000U;
}
void writeCommands();

#endif //G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
