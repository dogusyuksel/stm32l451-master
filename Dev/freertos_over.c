
#include "FreeRTOS.h"
#include "cmsis_os.h"

struct tskTaskControlBlock;
typedef struct tskTaskControlBlock TCB_t;

struct tskTaskControlBlock
{
    volatile StackType_t *pxTopOfStack;
    ListItem_t xStateListItem;
    ListItem_t xEventListItem;
    UBaseType_t uxPriority;
    StackType_t *pxStack;
    char pcTaskName[ configMAX_TASK_NAME_LEN ];
#if ( portSTACK_GROWTH > 0 )
    StackType_t *pxEndOfStack;
#endif
    uint32_t uxTCBNumber;
    UBaseType_t uxStackDepth;
};

void *vTaskStackAddr(void)
{
    TCB_t *tcb = (TCB_t *)xTaskGetCurrentTaskHandle();

    return (void *)tcb->pxStack;
}

size_t vTaskStackSize(void)
{
    TCB_t *tcb = (TCB_t *)xTaskGetCurrentTaskHandle();

    return tcb->uxStackDepth * sizeof(StackType_t);
}

char *vTaskName(void)
{
    return (char *)pcTaskGetName(NULL);
}
