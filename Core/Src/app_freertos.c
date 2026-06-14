/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tim.h"
#include "dac.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticSemaphore_t osStaticSemaphoreDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
uint32_t defaultTaskBuffer[ 128 ];
osStaticThreadDef_t defaultTaskControlBlock;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_mem = &defaultTaskBuffer[0],
  .stack_size = sizeof(defaultTaskBuffer),
  .cb_mem = &defaultTaskControlBlock,
  .cb_size = sizeof(defaultTaskControlBlock),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TransmitTask */
osThreadId_t TransmitTaskHandle;
uint32_t TransmitTaskBuffer[ 128 ];
osStaticThreadDef_t TransmitTaskControlBlock;
const osThreadAttr_t TransmitTask_attributes = {
  .name = "TransmitTask",
  .stack_mem = &TransmitTaskBuffer[0],
  .stack_size = sizeof(TransmitTaskBuffer),
  .cb_mem = &TransmitTaskControlBlock,
  .cb_size = sizeof(TransmitTaskControlBlock),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for transmit_buffer_ready */
osSemaphoreId_t transmit_buffer_readyHandle;
osStaticSemaphoreDef_t transmit_buffer_readyControlBlock;
const osSemaphoreAttr_t transmit_buffer_ready_attributes = {
  .name = "transmit_buffer_ready",
  .cb_mem = &transmit_buffer_readyControlBlock,
  .cb_size = sizeof(transmit_buffer_readyControlBlock),
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void initialize_test_signal(void);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartTransmitTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
return 0;
}
/* USER CODE END 1 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of transmit_buffer_ready */
  transmit_buffer_readyHandle = osSemaphoreNew(1, 1, &transmit_buffer_ready_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of TransmitTask */
  TransmitTaskHandle = osThreadNew(StartTransmitTask, NULL, &TransmitTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  BSP_COM_SelectLogPort(COM1);

  initialize_test_signal();

  run_oscilloscope();
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTransmitTask */
/**
* @brief Function implementing the TransmitTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTransmitTask */
void StartTransmitTask(void *argument)
{
  /* USER CODE BEGIN StartTransmitTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTransmitTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void initialize_test_signal(void)
{
  HAL_DAC_Start(&hdac2, DAC_CHANNEL_1);
  HAL_TIM_Base_Start(&htim2);
}

/* USER CODE END Application */

