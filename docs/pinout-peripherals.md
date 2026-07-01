Configuration g4-oscilloscope-b
Date 06/16/2026

**NB** OPAMP6, OPAMP3 work in inverted mode, so bias, trigger, and trigger type are also inverted

| PERIPHERALS | FUNCTIONS                                    | DESCRIPTION                                                        | PINS | MODES                                           |
|:------------|:---------------------------------------------|:-------------------------------------------------------------------|:-----|:------------------------------------------------|
|             | **Test Signal**                              |                                                                    |      |                                                 |
| DAC1        | DAC1_OUT1                                    | *TODO* remove - test signal 1                                      | PA4  |                                                 |
| DAC1        | DAC1_OUT2                                    | *TODO* remove - test signal 2                                      | -    | Triangle generation                             |
| TIM15       | Internal clock                               | *TODO* remove - test signal clock                                  | -    | Internal Clock                                  |
|             | **Virtual Ground**                           |                                                                    |      |                                                 |
| DAC2        | DAC2_OUT1                                    | Virtual ground                                                     | PA6  | Connected to external pin only                  |
|             | **Triggers **                                |                                                                    |      |                                                 |
| COMP3       | COMP3_INP                                    | CH1 Trigger                                                        | PA0  |                                                 |
| COMP1       | COMP1_INP                                    | CH2 Trigger                                                        | PB1  |                                                 |
| DAC3        | *(int)* DAC3_OUT1                            | Trigger level, internally connected                                | -    |                                                 |
|             | **Channel A**                                |                                                                    |      |                                                 |
| OPAMP5      | Follower                                     | *CH1 Input* (connect to **PA4**)                                   | P14  | OPAMP5_VINP                                     |           
| OPAMP5      | Follower                                     | CH1 pre-amplifier output, connect to **PB10**                      | PA8  | OPAMP5_VOUT                                     | 
| OPAMP4      | OPAMP4_VINM0                                 | CH1 signal, connect to **PA8**                                     | PB10 | Connected-INVERTINGINPUT_IO0_BIAS-DAC4_OUT1-INP |
| OPAMP4      | OPAMP4_VOUT                                  | CH1 amplified/shifted signal output, connect to **PA0**            | PB11 | Connected-INVERTINGINPUT_IO0_BIAS-DAC4_OUT1-INP |
| DAC4        | *(int)* DAC4_OUT1                            | CH1 Zero level bias, internally connected                          | -    | Connected to external pin only                  |
| ADC1,ADC2   | ADCx_IN1                                     | CH2 amplified/shifted signal input, connect to **PB12**            | PA0  | IN1 Single-ended                                |
|             | **Channel B**                                |                                                                    |      |                                                 |
| OPAMP6      | Follower                                     | *CH2 Input* (connect to **PA5**)                                   | PB13 | OPAMP6_VINP                                     |           
| OPAMP6      | Follower                                     | CH2 pre-amplifier output, connect to **PB2**                       | PB11 | OPAMP6_VOUT                                     | 
| OPAMP3      | OPAMP3_VINM0                                 | CH3 signal, connect to **PB11**                                    | PB2  | Connected-INVERTINGINPUT_IO0_BIAS-DAC3_OUT2-INP |
| OPAMP3      | OPAMP3_VOUT                                  | CH3 amplified/shifted signal output, connect to **PB13**, **PB15** | PB1  | Connected-INVERTINGINPUT_IO0_BIAS-DAC3_OUT2-INP |
| DAC3        | *(int)* DAC3_OUT2                            | CH2 Zero level bias, internally connected                          | -    | Connected to external pin only                  |
| ADC3        | ADC3_IN1                                     | CH3 amplified/shifted signal input, connect to **PB15**            | PB1  | IN1 Single-ended                                |
| ADC4        | ADC4_IN5                                     | CH3 amplified/shifted signal input, connect to **PB1**             | PB15 | IN5 Single-ended                                |
|             | **Clocking**                                 |                                                                    |      |                                                 |
| TIM2        | *(int)* TRGO->ADC3 trigger                   | Triggers ADC3/4 measurements                                       | -    | Gated by ITR0(TIM1)                             |
| TIM1        | *(int)* CH A PWM1 -> TRGO -> TIM3 clock gate | Stops TIM2 when keyframe ended                                     | -    | Clocked by ITR1(TIM2)                           |
|             | **Communication**                            |                                                                    |      |                                                 |
| LPUART1     | Asynchronous TX                              |                                                                    | PA2  |                                                 |
| LPUART1     | Asynchronous RX                              |                                                                    | PA3  |                                                 |
|             | **Calibration**                              |                                                                    |      |                                                 |
| ADC1        |                                              | VRef measured as injected rank 1                                   | -    |                                                 |

