#ifndef __LIBCSP2_HANDLERS_H__
#define __LIBCSP2_HANDLERS_H__

#ifdef USE_CSP_OVER_CANARD

#include <stdint.h>
#include <csp/csp_interface.h>
#include <csp/interfaces/csp_if_can.h>
#include "semphr.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#define LOCAL_NODE_ID 10
#define BCAST_PORT   10

#define RX_THREAD_TASK_DEPTH (1024)
#define CSP_QUEUE_LENGTH (256)
#define CSP_NETMASK (0xfff0)
#define CSP_NETMASK_MAX_NUMBER_OF_BITS (-1)
#define CSP_NO_VIA (0)

/* CAN payload length and DLC definitions according to ISO 11898-1 */
#define CAN_MAX_DLC (8)

/* special address description flags for the CAN_ID */
#define CAN_EFF_FLAG (0x80000000U) /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG (0x40000000U) /* remote transmission request */
#define CAN_ERR_FLAG (0x20000000U) /* error message frame */

/* valid bits in CAN ID for frame formats */
#define CAN_SFF_MASK (0x000007FFU) /* standard frame format (SFF) */
#define CAN_EFF_MASK (0x1FFFFFFFU) /* extended frame format (EFF) */
#define CAN_ERR_MASK (0x1FFFFFFFU) /* omit EFF, RTR, ERR flags */

typedef struct{
    csp_iface_t *iface;
    csp_can_interface_data_t ifdata;
    xSemaphoreHandle tx_sem;
    QueueHandle_t rx_queue;
    uint32_t can_err_frames_tracker;
    uint32_t can_rtr_frames_tracker;
} csp_can_s;


typedef struct {
    uint32_t id;
    uint8_t data[CAN_MAX_DLC];
    uint8_t dlc;
} csp_can_msg_s;

int can_add_interface(uint16_t node_id, uint16_t netmask);
void task_csp_router(void *data);
void task_csp_server(void *data);

#define LIBCSP_LOOP_DELAY 100

void libcspv2_task_start(void *arg);

#endif

#endif // __LIBCSP2_HANDLERS_H__
