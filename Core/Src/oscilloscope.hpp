//
// Created by elmot on 14/06/2026.
//

#ifndef G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
#define G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H

#include <algorithm>

#include "main.h"
#include "cmsis_os2.h"
#include <array>
#include <string>
#include <charconv>

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

    std::string name;
    const bool requires_restart{false};

    virtual void useNewValue() const = 0;

    long getValue() const { return value; }

    bool setValue(long aValue) const
    {
        aValue = adjustValue(aValue);
        if (aValue == value) return false;
        value = aValue;
        useNewValue();
        return true;
    }

    void write(void (*writeText)(const std::string_view&)) const
    {
        writeText(name);
        std::array<char,32> buffer{'='};
        auto [ptr, _] = std::to_chars(&buffer[1], &buffer.back(),value);
        *ptr++ ='\n';
        writeText(std::string_view(buffer.data(), ptr - buffer.data()));
    }

protected:
    mutable long value;
    const long min;
    const long max;
    virtual long adjustValue(const long aValue) const { return std::clamp(aValue, min, max); }
};

constexpr uint32_t THREAD_FLAG_READY_TO_TRANSMIT = 0x20;
constexpr uint32_t THREAD_FLAG_KEY_FRAME_DETECTED = 0x40;

constexpr size_t data_frame_size = 200;

extern osSemaphoreId_t transmitBufferBusyHandle; // NOLINT(*-dynamic-static-initializers)

struct transmitBuffer_t
{
    alignas(uint32_t) std::array<uint16_t, data_frame_size> samples;
    bool keyFrame;
};

extern transmitBuffer_t transmitBuffer; // NOLINT(*-dynamic-static-initializers)

extern osMessageQueueId_t cmdRxQueueHandle; // NOLINT(*-dynamic-static-initializers)


void startUartInput();

extern "C" void adcCalibration();
extern "C" void startMainAdc(bool interleaveSampling, uint16_t* buffer, size_t bufferLength);
extern "C" size_t adcSamplesLeft();

void writeCommands(void (*write_uart)(const std::string_view& str));

#endif //G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
