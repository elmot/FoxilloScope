#include "dac.h"
#include "opamp.h"
#include "oscilloscope.hpp"

void CommandGainChannel_t::useNewValue() const
{
    uint32_t opamp_gain_bits;
    switch (value)
    {
    case 1: opamp_gain_bits = OPAMP_PGA_GAIN_2_OR_MINUS_1;
        break;
    case 3: opamp_gain_bits = OPAMP_PGA_GAIN_4_OR_MINUS_3;
        break;
    case 7: opamp_gain_bits = OPAMP_PGA_GAIN_8_OR_MINUS_7;
        break;
    case 15: opamp_gain_bits = OPAMP_PGA_GAIN_16_OR_MINUS_15;
        break;
    case 31: opamp_gain_bits = OPAMP_PGA_GAIN_32_OR_MINUS_31;
        break;
    default: opamp_gain_bits = OPAMP_PGA_GAIN_64_OR_MINUS_63;
    }
    opamp->Init.PgaGain = opamp_gain_bits;
    HAL_OPAMP_Stop(opamp);
    HAL_OPAMP_Init(opamp);
    HAL_OPAMP_Start(opamp);
}

long CommandGainChannel_t::adjustValue(const long value) const
{
    for (const long i : {1, 3, 7, 15, 31})
    {
        if (value <= i) return i;
    }
    return 64L;
}
