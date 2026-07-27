#include "dac.h"
#include "opamp.h"
#include "oscilloscope.hpp"

void CommandGainChannel_t::useNewValue() const
{
    if (value == 1)
    {
        opamp->Init.Mode = OPAMP_FOLLOWER_MODE;
    } else
    {
        opamp->Init.Mode = OPAMP_PGA_MODE;
        uint32_t opamp_gain_bits;
        switch (value)
        {
        case 2: opamp_gain_bits = OPAMP_PGA_GAIN_2_OR_MINUS_1;
            break;
        case 4: opamp_gain_bits = OPAMP_PGA_GAIN_4_OR_MINUS_3;
            break;
        case 8: opamp_gain_bits = OPAMP_PGA_GAIN_8_OR_MINUS_7;
            break;
        case 16: opamp_gain_bits = OPAMP_PGA_GAIN_16_OR_MINUS_15;
            break;
        case 32: opamp_gain_bits = OPAMP_PGA_GAIN_32_OR_MINUS_31;
            break;
        default: opamp_gain_bits = OPAMP_PGA_GAIN_64_OR_MINUS_63;
        }
        opamp->Init.PgaGain = opamp_gain_bits;
    }
    HAL_OPAMP_Stop(opamp);
    HAL_OPAMP_Init(opamp);
    HAL_OPAMP_Start(opamp);

    bias_command->useNewValue();
}

long CommandGainChannel_t::adjustValue(const long value) const
{
    for (const long i : {1, 2, 4, 8, 16, 32})
    {
        if (value <= i) return i;
    }
    return 64L;
}
