#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcanard_handlers.h"
#include "main.h"
#include "logging.h"
#include "stm32l4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

typedef StaticTask_t osStaticThreadDef_t;

osThreadId_t libcanardTaskHandle;
uint32_t libcanardTaskBuffer[ 2048 ];
osStaticThreadDef_t libcanardTaskControlBlock;
const osThreadAttr_t libcanardTask_attributes = {
  .name = "libcanardTask",
  .cb_mem = &libcanardTaskControlBlock,
  .cb_size = sizeof(libcanardTaskControlBlock),
  .stack_mem = &libcanardTaskBuffer[0],
  .stack_size = sizeof(libcanardTaskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};

void StartlibcanardTask(void *argument)
{
  for(;;) {
    log_debug("libcanard task running...\n\r");
    osDelay(LIBCANARD_LOOP_TIMEOUT_MS);
  }
}

void libcanard_task_start(void *argument) {
    libcanardTaskHandle = osThreadNew(StartlibcanardTask, NULL, &libcanardTask_attributes);
}
