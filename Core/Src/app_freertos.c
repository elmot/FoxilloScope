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
typedef StaticTimer_t osStaticTimerDef_t;
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
/* Definitions for transmitTask */
osThreadId_t transmitTaskHandle;
uint32_t transmitTaskBuffer[ 200 ];
osStaticThreadDef_t transmitTaskControlBlock;
const osThreadAttr_t transmitTask_attributes = {
  .name = "transmitTask",
  .stack_mem = &transmitTaskBuffer[0],
  .stack_size = sizeof(transmitTaskBuffer),
  .cb_mem = &transmitTaskControlBlock,
  .cb_size = sizeof(transmitTaskControlBlock),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for keyFrameTask */
osThreadId_t keyFrameTaskHandle;
uint32_t keyFrameTaskBuffer[ 200 ];
osStaticThreadDef_t keyFrameTaskControlBlock;
const osThreadAttr_t keyFrameTask_attributes = {
  .name = "keyFrameTask",
  .stack_mem = &keyFrameTaskBuffer[0],
  .stack_size = sizeof(keyFrameTaskBuffer),
  .cb_mem = &keyFrameTaskControlBlock,
  .cb_size = sizeof(keyFrameTaskControlBlock),
  .priority = (osPriority_t) osPriorityHigh,
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
/* Definitions for partialFrameTimer */
osTimerId_t partialFrameTimerHandle;
osStaticTimerDef_t partialFrameTimerControlBlock;
const osTimerAttr_t partialFrameTimer_attributes = {
  .name = "partialFrameTimer",
  .cb_mem = &partialFrameTimerControlBlock,
  .cb_size = sizeof(partialFrameTimerControlBlock),
};
/* Definitions for transmitBufferBusy */
osSemaphoreId_t transmitBufferBusyHandle;
osStaticSemaphoreDef_t transmitBufferBusyControlBlock;
const osSemaphoreAttr_t transmitBufferBusy_attributes = {
  .name = "transmitBufferBusy",
  .cb_mem = &transmitBufferBusyControlBlock,
  .cb_size = sizeof(transmitBufferBusyControlBlock),
};
/* Definitions for transmitKeyBufferBusy */
osSemaphoreId_t transmitKeyBufferBusyHandle;
osStaticSemaphoreDef_t transmitKeyBufferBusyControlBlock;
const osSemaphoreAttr_t transmitKeyBufferBusy_attributes = {
  .name = "transmitKeyBufferBusy",
  .cb_mem = &transmitKeyBufferBusyControlBlock,
  .cb_size = sizeof(transmitKeyBufferBusyControlBlock),
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void initialize_test_signal(void);

/* USER CODE END FunctionPrototypes */

void startDefaultTask(void *argument);
extern void startTransmitTask(void *argument);
extern void keyFramesProcessing(void *argument);
extern void partialFrameSend(void *argument);

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

  /* creation of transmitKeyBufferBusy */
  transmitKeyBufferBusyHandle = osSemaphoreNew(1, 1, &transmitKeyBufferBusy_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* creation of partialFrameTimer */
  partialFrameTimerHandle = osTimerNew(partialFrameSend, osTimerPeriodic, NULL, &partialFrameTimer_attributes);

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

  /* creation of transmitTask */
  transmitTaskHandle = osThreadNew(startTransmitTask, NULL, &transmitTask_attributes);

  /* creation of keyFrameTask */
  keyFrameTaskHandle = osThreadNew(keyFramesProcessing, NULL, &keyFrameTask_attributes);

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

