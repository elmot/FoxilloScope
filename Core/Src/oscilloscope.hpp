//
// Created by elmot on 14/06/2026.
//

#ifndef G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
#define G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H

#include "main.h"
#include "cmsis_os2.h"
#include "array"
#include "string"

struct Command
{
    std::string_view name;
    bool requiresRestart{false};
    mutable long long value;
    void (*useNewValue)(long long value) = [](long long value){};
    long long (*adjustValue)(long long value) = [](const long long value) { return value; };
};

constexpr uint32_t THREAD_FLAG_READY_TO_TRANSMIT = 0x20;
constexpr uint32_t THREAD_FLAG_KEY_FRAME_DETECTED = 0x40;

constexpr size_t data_frame_size = 200;

extern osSemaphoreId_t transmitBufferBusyHandle;

struct transmitBuffer_t
{
    std::array<uint16_t, data_frame_size> samples;
    bool keyFrame;
};

extern transmitBuffer_t transmitBuffer;

extern osMessageQueueId_t cmdRxQueueHandle;


void startUartInput();

extern "C" void adcCalibration();
extern "C" void startMainAdc(bool interleaveSampling, uint16_t* buffer, size_t bufferLength);
extern "C" size_t adcSamplesLeft();


#endif //G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
