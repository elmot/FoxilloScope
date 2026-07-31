FoxilloScope
===

| PERIPHERALS | FUNCTIONS                                    | DESCRIPTION                                     | PINS | MODES                                           |
|:------------|:---------------------------------------------|:------------------------------------------------|:-----|:------------------------------------------------|
|             | **Test Signal**                              |                                                 |      |                                                 |
| DAC4        | *(int)*DAC1_OUT1                             | Test signal 1                                   | -    | Connected to internal pin                       |
| DAC3        | *(int)*DAC1_OUT1                             | Test signal 2                                   | -    | Triangle generation, connected to internal pin  |
| OPAMP4      | DAC Follower                                 | Test signal 2                                   | PB12 | DAC4.1 Follower                                 |
| OPAMP6      | DAC Follower                                 | Test signal 2                                   | PB11 | DAC3.1 Follower                                 |
| TIM15       |                                              | Test signal clock                               | -    | Internal Clock                                  |
|             | **Virtual Ground**                           |                                                 |      |                                                 |
| DAC4        | *(int)*DAC4_OUT2                             | Virtual ground                                  | -    | Connected to internal pin only                  |
| OPAMP5      | DAC Follower                                 | Virtual ground                                  | PA8  | DAC4.2 Follower                                 |
|             | **Triggers**                                 |                                                 |      |                                                 |
| DAC3        | *(int)* DAC3_OUT2                            | CH1 Trigger level, internally connected         | -    |                                                 |
| DAC2        | *(int)* DAC2_OUT1                            | CH2 Trigger level, internally connected         | -    |                                                 |
| COMP2       | COMP2_INP                                    | CH1 Trigger                                     | PA7  |                                                 |
| COMP7       | COMP7_INP                                    | CH2 Trigger                                     | PB14 |                                                 |
|             | **Channel A**                                |                                                 |      |                                                 |
| OPAMP2      | OPAMP2_VINP                                  | *CH1 Input* (connect to **PB12**)               | PB0  | OPAMP2_VINP                                     |           
| OPAMP2      | OPAMP2_VINM0                                 | CH1 bias                                        | PA5  | Connected-INVERTINGINPUT_IO0_BIAS               |
| OPAMP2      | OPAMP2_VOUT                                  | CH1 normalized, connect to **PA0**, **PA7**     | PA8  | OPAMP2_VOUT                                     | 
| DAC1        | DAC1_OUT2                                    | CH1 bias                                        | PA5  | Connected to external pin only                  |
| ADC1        | ADC1_IN1                                     | CH1 normalized, connect to **PA7**, **PA8**     | PA0  | IN1 Single-ended                                |
| ADC2        | ADC2_IN4                                     | CH1 normalized, connect to **PA0**, **PA8**     | PA7  | IN4 Single-ended                                |
|             | **Channel B**                                |                                                 |      |                                                 |
| OPAMP3      | OPAMP3_VINP                                  | *CH2 Input* (connect to **PB11**)               | PA1  | OPAMP3_VINP                                     |           
| OPAMP3      | OPAMP3_VINM0                                 | CH2 bias, connect to **PA4**,                   | PB2  | Connected-INVERTINGINPUT_IO0_BIAS               |
| OPAMP3      | OPAMP3_VOUT                                  | CH2 normalized, connect to **PB14**             | PB1  | OPAMP3_VOUT                                     | 
| DAC1        | DAC1_OUT1                                    | CH2 bias                                        | PA4  | Connected to external pin only                  |
| ADC3        | ADC3_IN1                                     | CH2 normalized, connect to **PB14**             | PB1  | IN1 Single-ended                                |
| ADC4        | ADC4_IN4                                     | CH2 normalized, connect to **PB1**              | PB14 | IN4 Single-ended                                |
|             | **Clocking**                                 |                                                 |      |                                                 |
| TIM2        | *(int)* TRGO->ADC3 trigger                   | Triggers ADC3/4 measurements                    | -    | Gated by ITR0(TIM1)                             |
| TIM1        | *(int)* CH A PWM1 -> TRGO -> TIM3 clock gate | Stops TIM2 when keyframe ended                  | -    | Clocked by ITR1(TIM2)                           |
|             | **Communication**                            |                                                 |      |                                                 |
| LPUART1     | Asynchronous TX                              | Data to PC (USB)                                | PA2  |                                                 |
| LPUART1     | Asynchronous RX                              | Data from PC (USB)                              | PA3  |                                                 |
| UART4       | Asynchronous TX                              | Data to ESP32 gateway (parallel with LPUART1)   | PC10 |                                                 |
| UART4       | Asynchronous RX                              | Data from ESP32 gateway (parallel with LPUART1) | PC11 |                                                 |
|             | **Calibration**                              |                                                 |      |                                                 |
| ADC1        |                                              | VRef measured as injected rank 1                | -    |                                                 |

