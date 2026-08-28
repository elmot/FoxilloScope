#include "dac.h"
#include "opamp.h"
#include "oscilloscope.hpp"

constexpr std::array<std::pair<unsigned int, unsigned long>,5> GAINS1 = {
    {
        {1, OPAMP_PGA_GAIN_2_OR_MINUS_1},
        {2, OPAMP_PGA_GAIN_2_OR_MINUS_1},
        {4, OPAMP_PGA_GAIN_4_OR_MINUS_3},
        {8, OPAMP_PGA_GAIN_8_OR_MINUS_7},
        {16, OPAMP_PGA_GAIN_16_OR_MINUS_15}}
};

constexpr std::array<std::pair<unsigned int, unsigned long>,4> GAINS2 = {
    {
        {1, OPAMP_PGA_GAIN_2_OR_MINUS_1},
        {3, OPAMP_PGA_GAIN_4_OR_MINUS_3},
        {7, OPAMP_PGA_GAIN_8_OR_MINUS_7},
        {15, OPAMP_PGA_GAIN_16_OR_MINUS_15}}
};

static constexpr std::tuple<unsigned,uint32_t,uint32_t,uint32_t> get_gain_mode1_gain1bits_gain2_bits(const unsigned long requested_gain)
{
    using namespace std;
    std::array<std::tuple<unsigned,uint32_t,uint32_t,uint32_t>, GAINS1.size() * GAINS2.size()> result{};

    size_t k = 0;
    for (const auto& [gain1, gain1bits] : GAINS1)
        for (const auto& [gain2, gain2bits] : GAINS2)
        {
            const auto mode1 = gain1 == 1 ? OPAMP_FOLLOWER_MODE : OPAMP_PGA_MODE;
            result[k++] = {gain1 * gain2, mode1,  gain1bits, gain2bits};
        }


    std::ranges::sort(result, [](const auto& a, const auto& b)
    {
        return std::get<0>(a) < std::get<0>(b);
    });

    for (const auto& [gain, mode1, gain1bits, gain2bits] : result)
    {
        if (requested_gain <= gain)
            return {gain, mode1, gain1bits, gain2bits};
    }
    return result.back();

}

long CommandGainChannel_t::adjustValue(const long value) const
{
    return std::get<0>(get_gain_mode1_gain1bits_gain2_bits(value));
}

void CommandGainChannel_t::useNewValue() const
{
    static_assert(get_gain_mode1_gain1bits_gain2_bits(29) ==
        std::tuple{30,OPAMP_PGA_MODE, OPAMP_PGA_GAIN_2_OR_MINUS_1,OPAMP_PGA_GAIN_16_OR_MINUS_15});
    static_assert(get_gain_mode1_gain1bits_gain2_bits(15) ==
        std::tuple{15,OPAMP_FOLLOWER_MODE, OPAMP_PGA_GAIN_2_OR_MINUS_1,OPAMP_PGA_GAIN_16_OR_MINUS_15});

    static_assert(get_gain_mode1_gain1bits_gain2_bits(53) ==
        std::tuple{56,OPAMP_PGA_MODE, OPAMP_PGA_GAIN_8_OR_MINUS_7,OPAMP_PGA_GAIN_8_OR_MINUS_7});

    static_assert(get_gain_mode1_gain1bits_gain2_bits(10000) ==
        std::tuple{15*16,OPAMP_PGA_MODE, OPAMP_PGA_GAIN_16_OR_MINUS_15, OPAMP_PGA_GAIN_16_OR_MINUS_15});

    const auto [gain, mode1, gain1bits, gain2bits]
    = get_gain_mode1_gain1bits_gain2_bits(value);

    opamp1->Init.Mode = mode1;
    opamp1->Init.PgaGain = gain1bits;
    opamp2->Init.PgaGain = gain2bits;

    HAL_OPAMP_Stop(opamp1);
    HAL_OPAMP_Init(opamp1);
    HAL_OPAMP_Start(opamp1);
    HAL_OPAMP_Stop(opamp2);
    HAL_OPAMP_Init(opamp2);
    HAL_OPAMP_Start(opamp2);

    bias_command->useNewValue();
}

