//
// Created by elmot on 14/06/2026.
//

#ifndef G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
#define G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H

#include "main.h"
#include "cmsis_os2.h"
#include "array"

constexpr size_t data_frame_size = 200;

extern osSemaphoreId_t transmitBufferBusyHandle;
extern osSemaphoreId_t readyToTransmitHandle;

extern std::array<uint16_t, data_frame_size> transmitBuffer;

#endif //G4_OSCILLOSCOPE_B_OSCILLOSCOPE_H
