Configuration	g4-oscilloscope-b
Date	06/16/2026
MCU	STM32G474RETx

| PERIPHERALS | FUNCTIONS              | DESCRIPTION                                   | PINS | MODES                                 |
|:------------|:-----------------------|:----------------------------------------------|:-----|:--------------------------------------|
| ADC3        | ADC3_IN1               | CH1 amplified signal input                    | PB1  | IN1 Single-ended                      |
| COMP1       | COMP1_INP              | CH1 trigger                                   | PB1  | INP                                   |
| COMP1       | COMP1_VS_VREFINT14     | TODO internal DAC to adjust trigger level     | -    | 1/4 Internal VRef                     |
| DAC1        | DAC1_OUT1              | Zero level bias, connect to **PB2**           | PA4  | Connected to external pin only        |
| DAC2        | DAC2_OUT1              | TODO remove - test signal, connect to **PB0** | PA6  | Connected to external pin only        |
| OPAMP3      | OPAMP3_VINM0           | Zero level bias, connect to **PA4**           | PB2  | PGA Connected-INVERTINGINPUT_IO0_BIAS |
| OPAMP3      | OPAMP3_VINP            | *CH1 Input*                                   | PB0  | PGA Connected-INVERTINGINPUT_IO0_BIAS |
| OPAMP3      | OPAMP3_VOUT            | CH1 amplified signal output                   | PB1  | PGA Connected-INVERTINGINPUT_IO0_BIAS |
| TIM2        | TIM2_VS_ClockSourceINT | TODO remove - test signal clock               | -    | Internal Clock                        |
| TIM3        | TIM3_VS_ClockSourceINT | ADC clock                                     | -    | Internal Clock                        |