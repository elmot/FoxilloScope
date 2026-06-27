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
COMP_HandleTypeDef hcomp6;

/* COMP5 init function */
void MX_COMP5_Init(void)
{

  /* USER CODE BEGIN COMP5_Init 0 */

  /* USER CODE END COMP5_Init 0 */

  /* USER CODE BEGIN COMP5_Init 1 */

  /* USER CODE END COMP5_Init 1 */
  hcomp5.Instance = COMP5;
  hcomp5.Init.InputPlus = COMP_INPUT_PLUS_IO1;
  hcomp5.Init.InputMinus = COMP_INPUT_MINUS_DAC4_CH1;
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
/* COMP6 init function */
void MX_COMP6_Init(void)
{

  /* USER CODE BEGIN COMP6_Init 0 */

  /* USER CODE END COMP6_Init 0 */

  /* USER CODE BEGIN COMP6_Init 1 */

  /* USER CODE END COMP6_Init 1 */
  hcomp6.Instance = COMP6;
  hcomp6.Init.InputPlus = COMP_INPUT_PLUS_IO1;
  hcomp6.Init.InputMinus = COMP_INPUT_MINUS_DAC4_CH2;
  hcomp6.Init.OutputPol = COMP_OUTPUTPOL_NONINVERTED;
  hcomp6.Init.Hysteresis = COMP_HYSTERESIS_10MV;
  hcomp6.Init.BlankingSrce = COMP_BLANKINGSRC_NONE;
  hcomp6.Init.TriggerMode = COMP_TRIGGERMODE_IT_RISING_FALLING;
  if (HAL_COMP_Init(&hcomp6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN COMP6_Init 2 */

  /* USER CODE END COMP6_Init 2 */

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
    GPIO_InitStruct.Pin = SHIFTED_BB13_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SHIFTED_BB13_GPIO_Port, &GPIO_InitStruct);

    /* COMP5 interrupt Init */
    HAL_NVIC_SetPriority(COMP4_5_6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(COMP4_5_6_IRQn);
  /* USER CODE BEGIN COMP5_MspInit 1 */

  /* USER CODE END COMP5_MspInit 1 */
  }
  else if(compHandle->Instance==COMP6)
  {
  /* USER CODE BEGIN COMP6_MspInit 0 */

  /* USER CODE END COMP6_MspInit 0 */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**COMP6 GPIO Configuration
    PB11     ------> COMP6_INP
    */
    GPIO_InitStruct.Pin = SHIFTED_AB11_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SHIFTED_AB11_GPIO_Port, &GPIO_InitStruct);

    /* COMP6 interrupt Init */
    HAL_NVIC_SetPriority(COMP4_5_6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(COMP4_5_6_IRQn);
  /* USER CODE BEGIN COMP6_MspInit 1 */

  /* USER CODE END COMP6_MspInit 1 */
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
    HAL_GPIO_DeInit(SHIFTED_BB13_GPIO_Port, SHIFTED_BB13_Pin);

    /* COMP5 interrupt Deinit */
  /* USER CODE BEGIN COMP5:COMP4_5_6_IRQn disable */
    /**
    * Uncomment the line below to disable the "COMP4_5_6_IRQn" interrupt
    * Be aware, disabling shared interrupt may affect other IPs
    */
    /* HAL_NVIC_DisableIRQ(COMP4_5_6_IRQn); */
  /* USER CODE END COMP5:COMP4_5_6_IRQn disable */

  /* USER CODE BEGIN COMP5_MspDeInit 1 */

  /* USER CODE END COMP5_MspDeInit 1 */
  }
  else if(compHandle->Instance==COMP6)
  {
  /* USER CODE BEGIN COMP6_MspDeInit 0 */

  /* USER CODE END COMP6_MspDeInit 0 */

    /**COMP6 GPIO Configuration
    PB11     ------> COMP6_INP
    */
    HAL_GPIO_DeInit(SHIFTED_AB11_GPIO_Port, SHIFTED_AB11_Pin);

    /* COMP6 interrupt Deinit */
  /* USER CODE BEGIN COMP6:COMP4_5_6_IRQn disable */
    /**
    * Uncomment the line below to disable the "COMP4_5_6_IRQn" interrupt
    * Be aware, disabling shared interrupt may affect other IPs
    */
    /* HAL_NVIC_DisableIRQ(COMP4_5_6_IRQn); */
  /* USER CODE END COMP6:COMP4_5_6_IRQn disable */

  /* USER CODE BEGIN COMP6_MspDeInit 1 */

  /* USER CODE END COMP6_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

