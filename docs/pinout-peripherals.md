Configuration g4-oscilloscope-b
Date 06/16/2026
MCU STM32G474RETx

| PERIPHERALS | FUNCTIONS                                    | DESCRIPTION                                     | PINS     | MODES                                 |
|:------------|:---------------------------------------------|:------------------------------------------------|:---------|:--------------------------------------|
|             | **Test Signal**                              |                                                 |          |                                       |
| DAC4        | *(int)* DAC4_OUT2                            | *TODO* remove - test signal                     | -        |                                       |
| OPAMP5      | OPAMP5_VOUT                                  | *TODO* remove - test signal, connect to **PB0** | PA8      | Connected to external pin only        |
| TIM15       | Internal clock                               | *TODO* remove - test signal clock               | -        | Internal Clock                        |
|             | **Trigger**                                  |                                                 |          |                                       |
| COMP1       | COMP1_INP                                    | Trigger                                         | PB13/PA1 |                                       |
| COMP1       | *(int)* COMP1_VS_DAC3OUT1                    | Internal DAC to adjust trigger level            | -        | VP_COMP1_VS_DAC3OUT1                  |
| DAC3        | *(int)* DAC3_OUT1                            | Trigger level, internally connected             | -        |                                       |
|             | **Channel A**                                |                                                 |          |                                       |
| OPAMP3      | OPAMP3_VINP                                  | *CH1 Input* (connect to **PA8**)                | PB0      | PGA Connected-INVERTINGINPUT_IO0_BIAS |
| ADC3        | ADC3_IN5                                     | CH1 amplified signal input, connect to **PB1**  | PB13     | IN5 Single-ended                      |
| ADC4        | ADC4_IN5                                     | CH1 amplified signal input, connect to **PB1**  | PB15     | IN5 Single-ended                      |
| DAC1        | DAC1_OUT1                                    | Zero level bias, connect to **PB2**             | PA4      | Connected to external pin only        |
| OPAMP3      | OPAMP3_VINM0                                 | Zero level bias, connect to **PA4**             | PB2      | PGA Connected-INVERTINGINPUT_IO0_BIAS |
| OPAMP3      | OPAMP3_VOUT                                  | CH1 amplified signal output                     | PB1      | PGA Connected-INVERTINGINPUT_IO0_BIAS |
|             | **Channel B**                                |                                                 |          |                                       |
| OPAMP4      | OPAMP4_VINP                                  | *CH2 Input* (connect to **PA8**)                | PB11     | PGA Connected-INVERTINGINPUT_IO0_BIAS |
| ADC1,ADC2   | ADCx_IN2                                     | CH2 amplified signal input, connect to **PB12** | PA1      | IN2 Single-ended                      |
| DAC2        | DAC1_OUT2                                    | CH2 Zero level bias, connect to **PB10**        | PA6      | Connected to external pin only        |
| OPAMP4      | OPAMP4_VINM0                                 | CH2 Zero level bias, connect to **PA6**         | PB10     | PGA Connected-INVERTINGINPUT_IO0_BIAS |
| OPAMP4      | OPAMP4_VOUT                                  | CH2 amplified signal output                     | PB12     | PGA Connected-INVERTINGINPUT_IO0_BIAS |
|             | **Clocking**                                 |                                                 |          |                                       |
| TIM2        | *(int)* TRGO->ADC3 trigger                   | Triggers ADC3/4 measurements                    | -        | Gated by ITR0(TIM1)                   |
| TIM1        | *(int)* CH A PWM1 -> TRGO -> TIM3 clock gate | Stops TIM2 when keyframe ended                  | -        | Clocked by ITR1(TIM2)                 |
|             | **Communication**                            |                                                 |          |                                       |
| LPUART1     | Asynchronous TX                              |                                                 | PA2      |                                       |
| LPUART1     | Asynchronous RX                              |                                                 | PA3      |                                       |

