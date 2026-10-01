/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_cortex.h"
#include "stm32g4xx_ll_system.h"
#include "stm32g4xx_ll_utils.h"
#include "stm32g4xx_ll_pwr.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_dma.h"

#include "stm32g4xx_ll_exti.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
[[noreturn]] void run_oscilloscope(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define VGND_DAC hdac3
#define VGND_OPAMP hopamp6
#define TEST_SIGNAL_DAC hdac2
#define TEST_SIGNAL_DAC_CHANNEL DAC_CHANNEL_1
#define VGND_DAC_CHANNEL_1 DAC_CHANNEL_1
#define VGND_DAC_CHANNEL_2 DAC_CHANNEL_2
#define STAGE_A2_OPAMP hopamp1
#define BIAS_DAC hdac1
#define BIAS_DAC_CHANNEL_A DAC_CHANNEL_1
#define BIAS_DAC_CHANNEL_B DAC_CHANNEL_2
#define STAGE_A1_OPAMP hopamp4
#define STAGE_B2_OPAMP hopamp3
#define STAGE_B1_OPAMP hopamp5
#define OPAMP5_IN__Pin GPIO_PIN_3
#define OPAMP5_IN__GPIO_Port GPIOC
#define ADC2_1_Pin GPIO_PIN_0
#define ADC2_1_GPIO_Port GPIOA
#define ADC1_3__OPAMP1_OUT_Pin GPIO_PIN_2
#define ADC1_3__OPAMP1_OUT_GPIO_Port GPIOA
#define OPAMP1_IN__Pin GPIO_PIN_3
#define OPAMP1_IN__GPIO_Port GPIOA
#define DAC1_1_Pin GPIO_PIN_4
#define DAC1_1_GPIO_Port GPIOA
#define DAC1_2_Pin GPIO_PIN_5
#define DAC1_2_GPIO_Port GPIOA
#define DAC2_1_Pin GPIO_PIN_6
#define DAC2_1_GPIO_Port GPIOA
#define OPAMP3_OUT_ADC3_1_Pin GPIO_PIN_1
#define OPAMP3_OUT_ADC3_1_GPIO_Port GPIOB
#define OPAMP3_IN__Pin GPIO_PIN_2
#define OPAMP3_IN__GPIO_Port GPIOB
#define OPAMP4_IN__Pin GPIO_PIN_10
#define OPAMP4_IN__GPIO_Port GPIOB
#define OPAMP6_OUT_Pin GPIO_PIN_11
#define OPAMP6_OUT_GPIO_Port GPIOB
#define OPAMP4_OUT_Pin GPIO_PIN_12
#define OPAMP4_OUT_GPIO_Port GPIOB
#define OPAMP4_IN_B13_Pin GPIO_PIN_13
#define OPAMP4_IN_B13_GPIO_Port GPIOB
#define ADC4_4_Pin GPIO_PIN_14
#define ADC4_4_GPIO_Port GPIOB
#define OPAMP5_IN_B15_Pin GPIO_PIN_15
#define OPAMP5_IN_B15_GPIO_Port GPIOB
#define OPAMP5_OUT_Pin GPIO_PIN_8
#define OPAMP5_OUT_GPIO_Port GPIOA
#define UART4_TX_Pin GPIO_PIN_10
#define UART4_TX_GPIO_Port GPIOC
#define UART4_RX_Pin GPIO_PIN_11
#define UART4_RX_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
