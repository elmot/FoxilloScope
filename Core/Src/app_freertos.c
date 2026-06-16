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
typedef StaticQueue_t osStaticMessageQDef_t;
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
/* Definitions for cmdRxQueue */
osMessageQueueId_t cmdRxQueueHandle;
uint8_t cmdRxQueueBuffer[ 128 * sizeof( uint8_t ) ];
osStaticMessageQDef_t cmdRxQueueControlBlock;
const osMessageQueueAttr_t cmdRxQueue_attributes = {
  .name = "cmdRxQueue",
  .cb_mem = &cmdRxQueueControlBlock,
  .cb_size = sizeof(cmdRxQueueControlBlock),
  .mq_mem = &cmdRxQueueBuffer,
  .mq_size = sizeof(cmdRxQueueBuffer)
};
/* Definitions for transmitBufferBusy */
osSemaphoreId_t transmitBufferBusyHandle;
osStaticSemaphoreDef_t transmitBufferBusyControlBlock;
const osSemaphoreAttr_t transmitBufferBusy_attributes = {
  .name = "transmitBufferBusy",
  .cb_mem = &transmitBufferBusyControlBlock,
  .cb_size = sizeof(transmitBufferBusyControlBlock),
};
/* Definitions for notReadyToTransmit */
osSemaphoreId_t notReadyToTransmitHandle;
osStaticSemaphoreDef_t transmitReadyControlBlock;
const osSemaphoreAttr_t notReadyToTransmit_attributes = {
  .name = "notReadyToTransmit",
  .cb_mem = &transmitReadyControlBlock,
  .cb_size = sizeof(transmitReadyControlBlock),
};
/* Definitions for dataUartTaken */
osSemaphoreId_t dataUartTakenHandle;
osStaticSemaphoreDef_t myBinarySem03ControlBlock;
const osSemaphoreAttr_t dataUartTaken_attributes = {
  .name = "dataUartTaken",
  .cb_mem = &myBinarySem03ControlBlock,
  .cb_size = sizeof(myBinarySem03ControlBlock),
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void initialize_test_signal(void);

/* USER CODE END FunctionPrototypes */

void startDefaultTask(void *argument);
extern void startTransmitTask(void *argument);

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
  /* creation of transmitBufferBusy */
  transmitBufferBusyHandle = osSemaphoreNew(1, 1, &transmitBufferBusy_attributes);

  /* creation of notReadyToTransmit */
  notReadyToTransmitHandle = osSemaphoreNew(1, 0, &notReadyToTransmit_attributes);

  /* creation of dataUartTaken */
  dataUartTakenHandle = osSemaphoreNew(1, 1, &dataUartTaken_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of cmdRxQueue */
  cmdRxQueueHandle = osMessageQueueNew (128, sizeof(uint8_t), &cmdRxQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(startDefaultTask, NULL, &defaultTask_attributes);

  /* creation of TransmitTask */
  TransmitTaskHandle = osThreadNew(startTransmitTask, NULL, &TransmitTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_startDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_startDefaultTask */
void startDefaultTask(void *argument)
{
  /* USER CODE BEGIN startDefaultTask */
  run_oscilloscope();
  /* USER CODE END startDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

