Configuration g4-oscilloscope-b
Date 06/16/2026

**NB** OPAMP6, OPAMP3 work in inverted mode, so bias, trigger, and trigger type are also inverted

| PERIPHERALS | FUNCTIONS                                    | DESCRIPTION                                                        | PINS | MODES                                           |
|:------------|:---------------------------------------------|:-------------------------------------------------------------------|:-----|:------------------------------------------------|
|             | **Test Signal**                              |                                                                    |      |                                                 |
| DAC1        | DAC1_OUT1                                    | *TODO* remove - test signal 1                                      | PA4  |                                                 |
| DAC4        | *(int)*DAC4_OUT1                             | *TODO* remove - test signal 2                                      | -    | Triangle generation                             |
| OPAMP4      | OPAMP4_VOUT                                  | *TODO* remove - test signal 2 repeater                             | PB12 | Follower-DAC4_OUT1-INP                          |	
| TIM15       | Internal clock                               | *TODO* remove - test signal clock                                  | -    | Internal Clock                                  |
|             | **Trigger A**                                |                                                                    |      |                                                 |
| COMP6       | COMP6_INP                                    | Trigger                                                            | PB11 |                                                 |
| COMP6       | *(int)* COMP6_VS_DAC4OUT2                    | Internal DAC to adjust trigger level                               | -    | COMP6_VS_DAC4OUT2                               |
| DAC4        | *(int)* DAC4_OUT2                            | Trigger level, internally connected                                | -    |                                                 |
|             | **Trigger B**                                |                                                                    |      |                                                 |
| COMP5       | COMP6_INP                                    | Trigger, connect to **PA1**, **PA15**                              | PB13 |                                                 |
| COMP5       | *(int)* COMP5_VS_DAC4OUT1                    | Internal DAC to adjust trigger level                               | -    | COMP5_VS_DAC4OUT1                               |
| DAC1        | *(int)* DAC1_OUT2                            | Trigger level, internally connected                                | -    |                                                 |
|             | **Channel A**                                |                                                                    |      |                                                 |
| OPAMP5      | Follower                                     | *CH1 Input* (connect to **PA4**)                                   | PC3  | OPAMP5_VINP                                     |           
| OPAMP5      | Follower                                     | CH1 pre-amplifier output, connect to **PB2**                       | PA8  | OPAMP5_VOUT                                     | 
| OPAMP6      | OPAMP6_VINM0                                 | CH1 signal, connect to **PA8**                                     | PA1  | Connected-INVERTINGINPUT_IO0_BIAS-DAC3_OUT1-INP |
| OPAMP6      | OPAMP6_VOUT                                  | CH1 amplified/shifted signal output, connect to **PA0**            | PB11 | Connected-INVERTINGINPUT_IO0_BIAS-DAC3_OUT1-INP |
| DAC3        | *(int)* DAC3_OUT1                            | CH1 Zero level bias, internally connected                          | -    | Connected to external pin only                  |
| ADC1,ADC2   | ADCx_IN1                                     | CH2 amplified signal input, connect to **PB11**                    | PA0  | IN1 Single-ended                                |
|             | **Channel B**                                |                                                                    |      |                                                 |
| OPAMP2      | Follower                                     | *CH2 Input* (connect to **PB12**)                                  | PA7  | OPAMP2_VINP                                     |           
| OPAMP2      | Follower                                     | CH2 pre-amplifier output, connect to **PB2**                       | PB1  | OPAMP2_VOUT                                     | 
| OPAMP3      | OPAMP3_VINM0                                 | CH3 signal, connect to **PB1**                                     | PB2  | Connected-INVERTINGINPUT_IO0_BIAS-DAC3_OUT2-INP |
| OPAMP3      | OPAMP3_VOUT                                  | CH3 amplified/shifted signal output, connect to **PB13**, **PB15** | PB1  | Connected-INVERTINGINPUT_IO0_BIAS-DAC3_OUT2-INP |
| DAC3        | *(int)* DAC3_OUT2                            | CH2 Zero level bias, internally connected                          | -    | Connected to external pin only                  |
| ADC3        | ADC3_IN5                                     | CH3 amplified signal input, connect to **PB1**, **PB15**           | PB13 | IN5 Single-ended                                |
| ADC4        | ADC4_IN5                                     | CH3 amplified signal input, connect to **PB1**, **PB13**           | PB15 | IN5 Single-ended                                |
|             | **Channel B**                                |                                                                    |      |                                                 |
| DAC2        | DAC1_OUT2                                    | CH2 Zero level bias, connect to **PB10**                           | PA6  | Connected to external pin only                  |
| OPAMP4      | OPAMP4_VINM0                                 | CH2 Zero level bias, connect to **PA6**                            | PB10 | PGA Connected-INVERTINGINPUT_IO0_BIAS           |
| OPAMP4      | OPAMP4_VOUT                                  | CH2 amplified signal output                                        | PB12 | PGA Connected-INVERTINGINPUT_IO0_BIAS           |
|             | **Clocking**                                 |                                                                    |      |                                                 |
| TIM2        | *(int)* TRGO->ADC3 trigger                   | Triggers ADC3/4 measurements                                       | -    | Gated by ITR0(TIM1)                             |
| TIM1        | *(int)* CH A PWM1 -> TRGO -> TIM3 clock gate | Stops TIM2 when keyframe ended                                     | -    | Clocked by ITR1(TIM2)                           |
|             | **Communication**                            |                                                                    |      |                                                 |
| LPUART1     | Asynchronous TX                              |                                                                    | PC1  |                                                 |
| LPUART1     | Asynchronous RX                              |                                                                    | PC0  |                                                 |

