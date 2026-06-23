#include "dac.h"
#include "opamp.h"
#include "oscilloscope.hpp"

struct CommandGainChannel_t : Command
{
    constexpr CommandGainChannel_t(const char* name, OPAMP_HandleTypeDef * opamp) : Command(name, 16, 2, 64), opamp(opamp){}
    OPAMP_HandleTypeDef  * opamp;
    void useNewValue() const override
    {
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
        HAL_OPAMP_Stop(opamp);
        HAL_OPAMP_Init(opamp);
        HAL_OPAMP_Start(opamp);
    }
protected:
    long adjustValue(const long value) const override
    {
        for (const long i : {2, 4, 8, 16, 32})
        {
            if (value <= i) return i;
        }
        return 64L;
    }
};

CommandGainChannel_t CommandGainChannelA{"gain.a", &hopamp3};
const Command& CommandGainChannelA_ref =CommandGainChannelA;

CommandGainChannel_t CommandGainChannelB{"gain.b", &hopamp4};
const Command& CommandGainChannelB_ref =CommandGainChannelB;

struct CommandBiasChannel_t : Command
{
    constexpr CommandBiasChannel_t(const char* name, DAC_HandleTypeDef* dac, uint32_t dac_channel)
    : Command(name, 0, -1'000'000, 1'000'000), dac{dac}, dac_channel{dac_channel}{}
    DAC_HandleTypeDef  * dac;
    const uint32_t dac_channel;

    void useNewValue() const override
    {
        const uint16_t dac_bias = std::ranges::clamp(
            (max - value) * 4'095LL / (max - min), 0LL, 4095LL);
        HAL_DAC_SetValue(dac, dac_channel,DAC_ALIGN_12B_R, dac_bias);
    }
};

CommandBiasChannel_t CommandBiasChannelA{"vbias.a", &hdac1,DAC1_CHANNEL_1};
const Command& CommandBiasChannelA_ref = CommandBiasChannelA;

CommandBiasChannel_t CommandBiasChannelB{"vbias.b", &hdac2,DAC2_CHANNEL_1};
const Command& CommandBiasChannelB_ref =CommandBiasChannelB;
