/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    opamp.c
  * @brief   This file provides code for the configuration
  *          of the OPAMP instances.
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
/* Includes ------------------------------------------------------------------*/
#include "opamp.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

OPAMP_HandleTypeDef hopamp2;
OPAMP_HandleTypeDef hopamp3;
OPAMP_HandleTypeDef hopamp5;
OPAMP_HandleTypeDef hopamp6;

/* OPAMP2 init function */
void MX_OPAMP2_Init(void)
{

  /* USER CODE BEGIN OPAMP2_Init 0 */

  /* USER CODE END OPAMP2_Init 0 */

  /* USER CODE BEGIN OPAMP2_Init 1 */

  /* USER CODE END OPAMP2_Init 1 */
  hopamp2.Instance = OPAMP2;
  hopamp2.Init.PowerMode = OPAMP_POWERMODE_HIGHSPEED;
  hopamp2.Init.Mode = OPAMP_FOLLOWER_MODE;
  hopamp2.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO0;
  hopamp2.Init.InternalOutput = DISABLE;
  hopamp2.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp2.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP2_Init 2 */

  /* USER CODE END OPAMP2_Init 2 */

}
/* OPAMP3 init function */
void MX_OPAMP3_Init(void)
{

  /* USER CODE BEGIN OPAMP3_Init 0 */

  /* USER CODE END OPAMP3_Init 0 */

  /* USER CODE BEGIN OPAMP3_Init 1 */

  /* USER CODE END OPAMP3_Init 1 */
  hopamp3.Instance = OPAMP3;
  hopamp3.Init.PowerMode = OPAMP_POWERMODE_HIGHSPEED;
  hopamp3.Init.Mode = OPAMP_PGA_MODE;
  hopamp3.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_DAC;
  hopamp3.Init.InternalOutput = DISABLE;
  hopamp3.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp3.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_IO0_BIAS;
  hopamp3.Init.PgaGain = OPAMP_PGA_GAIN_16_OR_MINUS_15;
  hopamp3.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP3_Init 2 */

  /* USER CODE END OPAMP3_Init 2 */

}
/* OPAMP5 init function */
void MX_OPAMP5_Init(void)
{

  /* USER CODE BEGIN OPAMP5_Init 0 */

  /* USER CODE END OPAMP5_Init 0 */

  /* USER CODE BEGIN OPAMP5_Init 1 */

  /* USER CODE END OPAMP5_Init 1 */
  hopamp5.Instance = OPAMP5;
  hopamp5.Init.PowerMode = OPAMP_POWERMODE_HIGHSPEED;
  hopamp5.Init.Mode = OPAMP_FOLLOWER_MODE;
  hopamp5.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO2;
  hopamp5.Init.InternalOutput = DISABLE;
  hopamp5.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp5.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP5_Init 2 */

  /* USER CODE END OPAMP5_Init 2 */

}
/* OPAMP6 init function */
void MX_OPAMP6_Init(void)
{

  /* USER CODE BEGIN OPAMP6_Init 0 */

  /* USER CODE END OPAMP6_Init 0 */

  /* USER CODE BEGIN OPAMP6_Init 1 */

  /* USER CODE END OPAMP6_Init 1 */
  hopamp6.Instance = OPAMP6;
  hopamp6.Init.PowerMode = OPAMP_POWERMODE_HIGHSPEED;
  hopamp6.Init.Mode = OPAMP_PGA_MODE;
  hopamp6.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_DAC;
  hopamp6.Init.InternalOutput = DISABLE;
  hopamp6.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp6.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_IO0_BIAS;
  hopamp6.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
  hopamp6.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP6_Init 2 */

  /* USER CODE END OPAMP6_Init 2 */

}

void HAL_OPAMP_MspInit(OPAMP_HandleTypeDef* opampHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(opampHandle->Instance==OPAMP2)
  {
  /* USER CODE BEGIN OPAMP2_MspInit 0 */

  /* USER CODE END OPAMP2_MspInit 0 */

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**OPAMP2 GPIO Configuration
    PA6     ------> OPAMP2_VOUT
    PA7     ------> OPAMP2_VINP
    */
    GPIO_InitStruct.Pin = AMPLIFIED_B_Pin|INPUT_B_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN OPAMP2_MspInit 1 */

  /* USER CODE END OPAMP2_MspInit 1 */
  }
  else if(opampHandle->Instance==OPAMP3)
  {
  /* USER CODE BEGIN OPAMP3_MspInit 0 */

  /* USER CODE END OPAMP3_MspInit 0 */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**OPAMP3 GPIO Configuration
    PB1     ------> OPAMP3_VOUT
    PB2     ------> OPAMP3_VINM0
    */
    GPIO_InitStruct.Pin = SHIFTED_B_Pin|AMPLIFIED_BB2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN OPAMP3_MspInit 1 */

  /* USER CODE END OPAMP3_MspInit 1 */
  }
  else if(opampHandle->Instance==OPAMP5)
  {
  /* USER CODE BEGIN OPAMP5_MspInit 0 */

  /* USER CODE END OPAMP5_MspInit 0 */

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**OPAMP5 GPIO Configuration
    PC3     ------> OPAMP5_VINP
    PA8     ------> OPAMP5_VOUT
    */
    GPIO_InitStruct.Pin = INPUT_A_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(INPUT_A_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = AMPLIFIED_AA8_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(AMPLIFIED_AA8_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN OPAMP5_MspInit 1 */

  /* USER CODE END OPAMP5_MspInit 1 */
  }
  else if(opampHandle->Instance==OPAMP6)
  {
  /* USER CODE BEGIN OPAMP6_MspInit 0 */

  /* USER CODE END OPAMP6_MspInit 0 */

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**OPAMP6 GPIO Configuration
    PA1     ------> OPAMP6_VINM0
    PB11     ------> OPAMP6_VOUT
    */
    GPIO_InitStruct.Pin = AMPLIFIED_A_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(AMPLIFIED_A_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SHIFTED_AB11_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SHIFTED_AB11_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN OPAMP6_MspInit 1 */

  /* USER CODE END OPAMP6_MspInit 1 */
  }
}

void HAL_OPAMP_MspDeInit(OPAMP_HandleTypeDef* opampHandle)
{

  if(opampHandle->Instance==OPAMP2)
  {
  /* USER CODE BEGIN OPAMP2_MspDeInit 0 */

  /* USER CODE END OPAMP2_MspDeInit 0 */

    /**OPAMP2 GPIO Configuration
    PA6     ------> OPAMP2_VOUT
    PA7     ------> OPAMP2_VINP
    */
    HAL_GPIO_DeInit(GPIOA, AMPLIFIED_B_Pin|INPUT_B_Pin);

  /* USER CODE BEGIN OPAMP2_MspDeInit 1 */

  /* USER CODE END OPAMP2_MspDeInit 1 */
  }
  else if(opampHandle->Instance==OPAMP3)
  {
  /* USER CODE BEGIN OPAMP3_MspDeInit 0 */

  /* USER CODE END OPAMP3_MspDeInit 0 */

    /**OPAMP3 GPIO Configuration
    PB1     ------> OPAMP3_VOUT
    PB2     ------> OPAMP3_VINM0
    */
    HAL_GPIO_DeInit(GPIOB, SHIFTED_B_Pin|AMPLIFIED_BB2_Pin);

  /* USER CODE BEGIN OPAMP3_MspDeInit 1 */

  /* USER CODE END OPAMP3_MspDeInit 1 */
  }
  else if(opampHandle->Instance==OPAMP5)
  {
  /* USER CODE BEGIN OPAMP5_MspDeInit 0 */

  /* USER CODE END OPAMP5_MspDeInit 0 */

    /**OPAMP5 GPIO Configuration
    PC3     ------> OPAMP5_VINP
    PA8     ------> OPAMP5_VOUT
    */
    HAL_GPIO_DeInit(INPUT_A_GPIO_Port, INPUT_A_Pin);

    HAL_GPIO_DeInit(AMPLIFIED_AA8_GPIO_Port, AMPLIFIED_AA8_Pin);

  /* USER CODE BEGIN OPAMP5_MspDeInit 1 */

  /* USER CODE END OPAMP5_MspDeInit 1 */
  }
  else if(opampHandle->Instance==OPAMP6)
  {
  /* USER CODE BEGIN OPAMP6_MspDeInit 0 */

  /* USER CODE END OPAMP6_MspDeInit 0 */

    /**OPAMP6 GPIO Configuration
    PA1     ------> OPAMP6_VINM0
    PB11     ------> OPAMP6_VOUT
    */
    HAL_GPIO_DeInit(AMPLIFIED_A_GPIO_Port, AMPLIFIED_A_Pin);

    HAL_GPIO_DeInit(SHIFTED_AB11_GPIO_Port, SHIFTED_AB11_Pin);

  /* USER CODE BEGIN OPAMP6_MspDeInit 1 */

  /* USER CODE END OPAMP6_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

