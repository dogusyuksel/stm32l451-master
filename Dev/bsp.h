#ifndef __BSP_
#define __BSP_

#include "FreeRTOS.h"
#include "cmsis_os.h"


void board_init(void);
void board_periodic_task(void *argument);

#endif // __BSP_
