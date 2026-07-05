#ifdef USE_CSP_OVER_CANARD

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcsp2_handlers.h"
#include "main.h"
#include "can.h"
#include "logging.h"
#include "stm32l4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "cmsis_os.h"
#include <csp/csp.h>
#include <csp/csp_interface.h>
#include <csp/csp_error.h>
#include <csp/interfaces/csp_if_can.h>


static void csp_can_tx_frame_cb(void);
static void csp_can_rx_frame_cb(uint32_t addr, uint8_t *data, uint8_t len);
static void csp_can_rx_thread(void* data);
static int csp_can_tx_frame(void *driver_data, uint32_t id, const uint8_t * data, uint8_t dlc);
uint8_t hal_can_write(CAN_HandleTypeDef *can, uint32_t addr, uint8_t *data, uint8_t len);


typedef StaticTask_t osStaticThreadDef_t;

osThreadId_t libcsp2TaskHandle;
uint32_t libcsp2TaskBuffer[ 2048 ];
osStaticThreadDef_t libcsp2TaskControlBlock;
const osThreadAttr_t libcsp2Task_attributes = {
  .name = "libcsp2Task",
  .cb_mem = &libcsp2TaskControlBlock,
  .cb_size = sizeof(libcsp2TaskControlBlock),
  .stack_mem = &libcsp2TaskBuffer[0],
  .stack_size = sizeof(libcsp2TaskBuffer),
  .priority = (osPriority_t) osPriorityAboveNormal,
};

osThreadId_t libcsp2RouteHandle;
uint32_t routerTaskBuffer[ 512 ];
osStaticThreadDef_t routerTaskControlBlock;
const osThreadAttr_t routerTask_attributes = {
  .name = "routerTask",
  .cb_mem = &routerTaskControlBlock,
  .cb_size = sizeof(routerTaskControlBlock),
  .stack_mem = &routerTaskBuffer[0],
  .stack_size = sizeof(routerTaskBuffer),
  .priority = (osPriority_t) osPriorityHigh,
};

csp_iface_t csp_if_can1 = {
    .name = "CAN1"
};

static csp_can_s csp_can_ctx = {
    .iface = &csp_if_can1
};

// interrupt callback functions
// void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) {
//     csp_can_tx_frame_cb();
// }

// void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) {
//     csp_can_tx_frame_cb();
// }

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, data) == HAL_OK) {
        // call our CSP related new callback here
        csp_can_rx_frame_cb(header.ExtId, data, header.DLC);
    }
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) {
    // call our CSP related new callback here
    // assuming only one can here as well
    (void)hcan;
    csp_can_tx_frame_cb();
}

int can_add_interface(uint16_t node_id, uint16_t netmask)
{
    csp_can_s * csp_can = &csp_can_ctx;

    csp_can->tx_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(csp_can->tx_sem);

    csp_can->iface->interface_data = &csp_can->ifdata;
    csp_can->iface->addr = node_id;
    csp_can->iface->netmask = netmask;
    csp_can->iface->driver_data = csp_can;
    csp_can->ifdata.tx_func = csp_can_tx_frame;
    csp_can->ifdata.pbufs = NULL;

    if (csp_can_add_interface(csp_can->iface) != CSP_ERR_NONE) {
        return 1;
    }
    csp_rtable_set(node_id, CSP_NETMASK_MAX_NUMBER_OF_BITS, csp_can->iface, CSP_NO_VIA);

    csp_can->rx_queue = xQueueCreate(CSP_QUEUE_LENGTH, sizeof(csp_can_msg_s));
    xTaskCreate(csp_can_rx_thread, "csp_rx_thread", RX_THREAD_TASK_DEPTH, &csp_can_ctx, 3, NULL);

    return 0;
}

static void csp_can_rx_frame_cb(uint32_t addr, uint8_t *data, uint8_t len) {
    int task_woken = pdTRUE;
    if (len > 8) {
        return;
    }

    csp_can_msg_s msg = {
        .id = addr,
        .dlc = len,
    };

    for (uint8_t i = 0; i<len; i++) {
        msg.data[i] = *(data + i);
    }

    csp_queue_enqueue_isr(csp_can_ctx.rx_queue, &msg, &task_woken);

    portYIELD_FROM_ISR(task_woken);
}

static void csp_can_tx_frame_cb(void) {

    BaseType_t task_woken = 1;
    xSemaphoreGiveFromISR(csp_can_ctx.tx_sem, &task_woken);
    portYIELD_FROM_ISR (task_woken);
}

static int csp_can_tx_frame(void *driver_data, uint32_t id, const uint8_t * data, uint8_t dlc) {
    if (driver_data == NULL) {
        return 1;
    }

    csp_can_s * csp_can = (csp_can_s *) driver_data;
    if (csp_can->tx_sem == NULL) {
        return 1;
    }

    uint8_t local_data[CAN_MAX_DLC];
    if (dlc > CAN_MAX_DLC) {
        return 1;
    }
    for(uint8_t i=0; i<dlc; i++) {
        local_data[i] = data[i];
    }

    uint8_t result = 1;
    if (xSemaphoreTake(csp_can->tx_sem, portMAX_DELAY) == pdTRUE) {
        result = hal_can_write(&hcan1, id, local_data, dlc);
        if (result != 0) {
            log_debug("can_write error %x\n\r", result);
        }
        xSemaphoreGive(csp_can->tx_sem);
    } else {
        log_debug("Failed to take CSP TX Semaphore\n\r");
    }

    return result;
}

uint8_t hal_can_write(CAN_HandleTypeDef *can, uint32_t addr, uint8_t *data, uint8_t len) {
    uint8_t rc;
    CAN_TxHeaderTypeDef header = {0};
    uint32_t mailbox;

    if (!can) {
        return 1;
    }

    header.ExtId = addr;
    header.IDE = CAN_ID_EXT;
    header.RTR = CAN_RTR_DATA;
    header.DLC = len;
    header.TransmitGlobalTime = DISABLE;

#ifndef RENODE_BUILD
    uint32_t err;
    // Check if CAN is in error state
    err = HAL_CAN_GetError(can);
    if (err != 0) {
        HAL_CAN_ResetError(can);
        // There are CAN errors
        return 2;
    }
// #else
//     HAL_CAN_ResetError(can);
#endif

    // Handle all TX mailboxes being full (in this case, probably by blocking)
    rc = HAL_CAN_AddTxMessage(can, &header, data, &mailbox);
    if (rc || HAL_CAN_GetError(can)) {
        log_debug("HAL state=%lu free=%lu addTx=%d mailbox=%lu TSR=%08lx err=%08lx\r\n",
            HAL_CAN_GetState(can),
            HAL_CAN_GetTxMailboxesFreeLevel(can),
            rc,
            mailbox,
            can->Instance->TSR,
            HAL_CAN_GetError(can));
    }

#ifndef RENODE_BUILD
    if (rc != HAL_OK) {
        err = HAL_CAN_GetError(can);
        if (err != 0) {
            HAL_CAN_ResetError(can);
            // There are CAN errors
            return 3;
        } else {
            return 4;
        }
    }
// #else
//     HAL_CAN_ResetError(can);
#endif

    return 0;
}

static void csp_can_rx_thread(void* data) {
    csp_can_s * csp_can = data;

    while (1) {
        csp_can_msg_s msg;
        if (csp_queue_dequeue(csp_can->rx_queue, &msg, portMAX_DELAY) == CSP_QUEUE_OK){
            if (msg.dlc > CAN_MAX_DLC) {
                /*Too long*/
                log_debug("\n[CSP ERROR] CAN frame Longer than MAX Length\n\r");
            }

            else if(msg.id & (CAN_ERR_FLAG)) {
                /*Error Frame*/
                log_debug("\n[CSP ERROR] Error CAN Frame Received\n\r");
                csp_can->can_err_frames_tracker++;
            }

            else if(msg.id & (CAN_RTR_FLAG)) {
                /*RTR Frame*/
                log_debug("\n[CSP ERROR] Remote Transmission Request (RTR) CAN Frame Received\n\r");
                csp_can->can_rtr_frames_tracker++;
            }

            else {
                /* Frames that can be processed */
                msg.id &= CAN_EFF_MASK;
                csp_can_rx(csp_can->iface, msg.id, msg.data, msg.dlc, NULL);
            }
        }
    }
}

static void broadcast_csp_packet(uint8_t *data, uint32_t len)
{
    if (!data) {
        return;
    }
    csp_transaction(CSP_PRIO_NORM,
                    0x3FFF,
                    BCAST_PORT,
                    1000,
                    (void *)data,
                    len,
                    NULL,
                    0);
}

void task_csp_router(void *data) {
    log_debug("%s(%u)\n\r", __func__, __LINE__);
    (void)data;
    while (1) {
        csp_route_work();
        // osDelay(10);
    }
}

void task_csp_server(void *data) {
    (void)data;
    log_debug("%s(%u)\n\r", __func__, __LINE__);

    char *bcast_data = "hello\n\0";

    /* Create socket with no specific socket options, e.g. accepts CRC32, HMAC, etc. if enabled during
     * compilation */
    csp_socket_t sock = {0};

    /* Bind socket to all ports, e.g. all incoming connections will be handled here */
    csp_bind(&sock, CSP_ANY);

    /* Create a backlog of 10 connections, i.e. up to 10 new connections can be queued */
    csp_listen(&sock, 10);

    while (1) {
        /* Wait for a new connection, 1000 mS timeout */
        csp_conn_t *conn;
        if ((conn = csp_accept(&sock, 1000)) == NULL) {
            /* timeout */
            broadcast_csp_packet((uint8_t *)bcast_data, strlen(bcast_data)); // send hi
            log_debug("hi sent!\n\r");
            continue;
        }

        /* Read packets on connection, timout is 100 mS */
        csp_packet_t *packet;
        while ((packet = csp_read(conn, 1000)) != NULL) {
            if (csp_conn_dport(conn) > CSP_UPTIME) {
                log_debug("CSP Packet Received\n Incoming Port: %d\tSenderPort: %d\tSender ID: %u\tPacket Length: %d\n\r",
                        csp_conn_dport(conn), csp_conn_sport(conn), packet->id.src, packet->length);
                csp_buffer_free(packet);
            } else {
                /* Call the default CSP service handler, handle pings, buffer use, etc. */
                csp_service_handler(packet);
            }
        }

        /* Close current connection */
        csp_close(conn);
    }
}

void packet_dump(uint8_t *data, uint16_t len) {
    if (!data || !len) {
        return;
    }

    const uint16_t break_nochar = 8;

    for (uint16_t start = 0; start < len; start += break_nochar) {
        uint16_t end = start + break_nochar;
        if (end > len) end = len;

        /* Hex column: print actual bytes, pad missing with spaces so column width is stable */
        for (uint16_t i = start; i < start + break_nochar; i++) {
            if (i < end) {
                log_debug("0x%.02X ", data[i]);
            } else {
                /* "0x%.02X " is 5 characters wide, so pad with 5 spaces for missing bytes */
                log_debug("     ");
            }
        }

        /* separator between hex and ascii columns */
        log_debug("   ");

        /* ASCII/string column: only print existing bytes in this block */
        for (uint16_t i = start; i < end; i++) {
            uint8_t c = data[i];
            /* printable range 32..126, otherwise show dot */
            log_debug("%c", (c >= 32 && c <= 126) ? c : '.');
        }

        log_debug("\n\r");
    }
}

void csp_output_hook(csp_id_t *idout, csp_packet_t *packet, csp_iface_t *iface, uint16_t via, int from_me) {
    (void)from_me;
    log_debug("OUT: S %u, D %u(0x%X), Dp %u, Sp %u, Pr %u, Fl 0x%02X, Sz %u VIA: %s (%u)\n\r",
              idout->src,
              idout->dst,
              idout->dst,
              idout->dport,
              idout->sport,
              idout->pri,
              idout->flags,
              packet->length,
              iface->name,
              (via != CSP_NO_VIA_ADDRESS) ? via : idout->dst);
    packet_dump(packet->data, packet->length);
    return;
}

void csp_input_hook(csp_iface_t *iface, csp_packet_t *packet) {
    // debug
    log_debug("INP: S %u, D %u, Dp %u, Sp %u, Pr %u, Fl 0x%02X, Sz %" PRIu16 " VIA: %s\n\r",
              packet->id.src,
              packet->id.dst,
              packet->id.dport,
              packet->id.sport,
              packet->id.pri,
              packet->id.flags,
              packet->length,
              iface->name);
    packet_dump(packet->data, packet->length);
}

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize) {
    /* If the buffers to be provided to the Idle task are declared inside this
    function then they must be declared static – otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task’s
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task’s stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
    /* If the buffers to be provided to the Timer task are declared inside this
    function then they must be declared static – otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task’s state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task’s stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}


void libcspv2_task_start(void *arg) {
  csp_conf.hostname = "stm32l4-master";
  csp_conf.model = "stm32l4 master";
  csp_conf.version = 2;
//   csp_conf.conn_dfl_so = CSP_O_NOCRC32;
  csp_init();
  log_debug("CSP Initialized!\n\r");
  libcsp2RouteHandle = osThreadNew(task_csp_router, NULL, &routerTask_attributes);
  can_add_interface(LOCAL_NODE_ID, CSP_NETMASK);
  libcsp2TaskHandle = osThreadNew(task_csp_server, NULL, &libcsp2Task_attributes);
}

#endif
