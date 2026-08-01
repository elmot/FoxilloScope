#ifndef G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
#define G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H

#include <algorithm>

#include "main.h"
#include "cmsis_os2.h"
#include <array>
#include <atomic>
#include <string>
#include <charconv>

#define ADC_MAX_VALUE 4095UL
#define ADC_STEPS_STR "4096"
#define DAC_MAX_VALUE 4095UL

void writeUart(const std::string_view& str);

struct Command_t
{
    constexpr Command_t(char const* name_v,
                      const long value_v, const long min_v, const long max_v,
                      const bool requires_restart_v = false) :
        name{name_v},
        requires_restart{requires_restart_v},
        min{min_v},
        max{max_v},
        value(value_v)
    {
    }

    virtual ~Command_t() = default;

    const std::string_view name;
    const bool requires_restart{false};

    virtual void useNewValue() const {};

    long getValue() const { return value; }

    bool setValue(long aValue) const
    {
        aValue = adjustValue(aValue);
        if (aValue == value) return false;
        value = aValue;
        useNewValue();
        return true;
    }

    static void do_write_value(const std::string_view& valueName, const long v)
    {
        writeUart(valueName);
        std::array<char,32> buffer{'='};
        auto [ptr, err] = std::to_chars(&buffer[1], buffer.end() - 1, v);
        // Always verify to_chars succeeded before writing '\n'
        if (err == std::errc{})
        {
            *ptr++ = '\n';
            writeUart(std::string_view(buffer.data(), ptr - buffer.data()));
        }
    }

    virtual void write() const
    {
        do_write_value(name, value);
    }

    const long min;
    const long max;
protected:
    /** Mutable part*/
    mutable long value;
    /**End of Mutable part*/
    virtual long adjustValue(const long aValue) const { return std::clamp(aValue, min, max); }
};

struct CommandBaseLevelUv_t;
struct CommandGainChannel_t : Command_t
{
    constexpr CommandGainChannel_t(const Command_t* bias_command, const char* name, OPAMP_HandleTypeDef * opamp) : Command_t(name, 1, 1, 63), opamp(opamp), bias_command(bias_command){}
    void useNewValue() const override;
protected:
    OPAMP_HandleTypeDef * const opamp;
    const Command_t* bias_command;
    long adjustValue(long value) const override;
};

struct CommandBaseLevelUv_t : Command_t
{
    constexpr CommandBaseLevelUv_t(const char* name, DAC_HandleTypeDef* dac, uint32_t dac_channel,
        const char* gain_name, OPAMP_HandleTypeDef * gain_opamp)
        : Command_t(name, 0, -1'000L * static_cast<long>(VDD_VALUE) / 2L, 1'000L * VDD_VALUE / 2),
          dac{dac}, dac_channel{dac_channel}, gain_cmd{CommandGainChannel_t(this, gain_name, gain_opamp)}
    {

    }

    DAC_HandleTypeDef* dac;
    const uint32_t dac_channel;
    const CommandGainChannel_t gain_cmd;

    void useNewValue() const override
    {
        const long gain = gain_cmd.getValue();
        if (gain > 1) {
            constexpr long long dac_max_ll = DAC_MAX_VALUE;
            // PGA mode: V_dac = VDD/2 + bias × G/(G-1)
            // dac_code = (max*(G-1) + value*G) * DAC_MAX / ((max-min)*(G-1))
            const long long dac_code = (static_cast<long long>(max) * (gain - 1) + static_cast<long long>(value) * gain)
                       * dac_max_ll / (static_cast<long long>(max - min) * (gain - 1));
            const uint16_t dac_bias = static_cast<uint16_t>(std::ranges::clamp(dac_code, 0LL, dac_max_ll));
            HAL_DAC_SetValue(dac, dac_channel, DAC_ALIGN_12B_R, dac_bias);
        }
    }
};

constexpr uint32_t THREAD_FLAG_READY_TO_TRANSMIT = 0x20;
constexpr uint32_t THREAD_FLAG_KEY_FRAME_DETECTED = 0x40;

constexpr size_t data_frame_size = 200;

struct TransmitBuffer_t
{   constexpr TransmitBuffer_t(osSemaphoreId_t& a_semaphore): semaphore(a_semaphore){}
    alignas(uint32_t) std::array<uint16_t, data_frame_size> samplesA{};
    alignas(uint32_t) std::array<uint16_t, data_frame_size> samplesB{};
    std::atomic<size_t> length{};
    std::atomic<bool> head{};
    const osSemaphoreId_t& semaphore;
};

extern TransmitBuffer_t transmitBuffer; // NOLINT(*-dynamic-static-initializers)
extern TransmitBuffer_t transmitKeyBuffer; // NOLINT(*-dynamic-static-initializers)
extern std::atomic<bool> transmitKeyBufferReady; // NOLINT(*-dynamic-static-initializers)

extern osMessageQueueId_t cmdRxQueueHandle; // NOLINT(*-dynamic-static-initializers)

extern osTimerId_t partialFrameTimerHandle; // NOLINT(*-dynamic-static-initializers)

extern uint16_t analog_supply_voltage_mV; // NOLINT(*-dynamic-static-initializers)


void startUartInput();

extern "C" void adcCalibration();
extern "C" void startMainAdcs(bool interleaveSampling, uint16_t* bufferA, uint16_t* bufferB, size_t bufferLength);
extern "C" size_t adcSamplesLeft();
extern "C" void startSysBootloader(void);

[[maybe_unused]]static uint32_t msec_to_ticks(const uint32_t msec) {
    const uint32_t ticks_per_sec = osKernelGetTickFreq();
    return (msec * ticks_per_sec) / 1000U;
}
void writeCommands();

enum class TriggerState
{
    DISARMED,
    ARMED,
    TRIGGERED,
    PROCESSING
};
#endif //G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
