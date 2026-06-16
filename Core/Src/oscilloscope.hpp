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
    bool requiresRestart;
    mutable long long value;
    void (*useNewValue)(long long value);
    long long (*adjustValue)(long long value) = [](const long long value) { return value; };
};

constexpr size_t data_frame_size = 200;

extern osSemaphoreId_t transmitBufferBusyHandle;
extern osSemaphoreId_t notReadyToTransmitHandle;

extern std::array<uint16_t, data_frame_size> transmitBuffer;

void startUartInput();

#endif //G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
