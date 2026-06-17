/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    comp.c
  * @brief   This file provides code for the configuration
  *          of the COMP instances.
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
#include "comp.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

COMP_HandleTypeDef hcomp5;

/* COMP5 init function */
void MX_COMP5_Init(void)
{

  /* USER CODE BEGIN COMP5_Init 0 */

  /* USER CODE END COMP5_Init 0 */

  /* USER CODE BEGIN COMP5_Init 1 */

  /* USER CODE END COMP5_Init 1 */
  hcomp5.Instance = COMP5;
  hcomp5.Init.InputPlus = COMP_INPUT_PLUS_IO1;
  hcomp5.Init.InputMinus = COMP_INPUT_MINUS_DAC1_CH2;
  hcomp5.Init.OutputPol = COMP_OUTPUTPOL_NONINVERTED;
  hcomp5.Init.Hysteresis = COMP_HYSTERESIS_10MV;
  hcomp5.Init.BlankingSrce = COMP_BLANKINGSRC_NONE;
  hcomp5.Init.TriggerMode = COMP_TRIGGERMODE_IT_RISING_FALLING;
  if (HAL_COMP_Init(&hcomp5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN COMP5_Init 2 */

  /* USER CODE END COMP5_Init 2 */

}

void HAL_COMP_MspInit(COMP_HandleTypeDef* compHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(compHandle->Instance==COMP5)
  {
  /* USER CODE BEGIN COMP5_MspInit 0 */

  /* USER CODE END COMP5_MspInit 0 */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**COMP5 GPIO Configuration
    PB13     ------> COMP5_INP
    */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* COMP5 interrupt Init */
    HAL_NVIC_SetPriority(COMP4_5_6_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(COMP4_5_6_IRQn);
  /* USER CODE BEGIN COMP5_MspInit 1 */

  /* USER CODE END COMP5_MspInit 1 */
  }
}

void HAL_COMP_MspDeInit(COMP_HandleTypeDef* compHandle)
{

  if(compHandle->Instance==COMP5)
  {
  /* USER CODE BEGIN COMP5_MspDeInit 0 */

  /* USER CODE END COMP5_MspDeInit 0 */

    /**COMP5 GPIO Configuration
    PB13     ------> COMP5_INP
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_13);

    /* COMP5 interrupt Deinit */
    HAL_NVIC_DisableIRQ(COMP4_5_6_IRQn);
  /* USER CODE BEGIN COMP5_MspDeInit 1 */

  /* USER CODE END COMP5_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

