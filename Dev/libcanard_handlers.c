#ifndef USE_CSP_OVER_CANARD

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
#include "canard.h"
#include "canard_stm32.h"
#include "nodes/node1/StatusShare.h"
#include "nodes/node2/NodePing.h"
#include "nodes/node2/NodePong.h"

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
  uavcanInit();

  for(;;) {
    sendCanard();
    receiveCanard();
    spinCanard();
    osDelay(CANARD_SPIN_PERIOD);
  }
}

void libcanard_task_start(void *argument) {
    libcanardTaskHandle = osThreadNew(StartlibcanardTask, NULL, &libcanardTask_attributes);
}

static CanardInstance g_canard;            // The library instance
static uint8_t g_canard_memory_pool[1024]; // Area for memory allocation, used
                                           // by the library

static char *node_id_to_str(uint32_t id) {
  switch (id) {
  case NODES_NODE2_NODEPONG_ID:
    return NODES_NODE2_NODEPONG_NAME;
  case NODES_NODE2_NODEPING_ID:
    return NODES_NODE2_NODEPING_NAME;
  case NODES_NODE1_STATUSSHARE_ID:
    return NODES_NODE1_STATUSSHARE_NAME;
  default:
    return "UNHANDLED";
  }
}

static char *transfer_type_to_str(CanardTransferType type) {
  switch (type) {
  case CanardTransferTypeResponse:
    return "Response";
  case CanardTransferTypeRequest:
    return "Request";
  case CanardTransferTypeBroadcast:
    return "Broadcast";
  default:
    return "unknown";
    break;
  }

  return "Unknown_Type";
}

bool shouldAcceptTransfer(const CanardInstance *ins,
                          uint64_t *out_data_type_signature,
                          uint16_t data_type_id,
                          CanardTransferType transfer_type,
                          uint8_t source_node_id) {
  log_debug("ins->node_id: %d source_node_id: %d data_type_id: %s(%d) "
           "transfer_type: %s\n",
           ins->node_id, source_node_id, node_id_to_str(data_type_id),
           data_type_id, transfer_type_to_str(transfer_type));

  if (transfer_type == CanardTransferTypeRequest) {
    switch (data_type_id) {
    case NODES_NODE1_STATUSSHARE_ID:
      *out_data_type_signature = NODES_NODE1_STATUSSHARE_SIGNATURE;
      return 1;
    default:
      return 0;
    }
  } else if (transfer_type == CanardTransferTypeResponse) {
    // drop response messages. Do nothing
  } else if (transfer_type == CanardTransferTypeBroadcast) {
    switch (data_type_id) {
    case NODES_NODE2_NODEPING_ID:
      *out_data_type_signature = NODES_NODE2_NODEPING_SIGNATURE;
      return 1;
    case NODES_NODE2_NODEPONG_ID:
      *out_data_type_signature = NODES_NODE2_NODEPONG_SIGNATURE;
      return 1;
    default:
      return 0;
    }
  }

  return 0;
}

static void handler_status_share(CanardInstance *ins,
                                 CanardRxTransfer *transfer) {
  uint8_t buf[NODES_NODE1_STATUSSHARE_REQUEST_MAX_SIZE];
  uint8_t *buf_ptr = buf;
  nodes_node1_StatusShareRequest msg;
  nodes_node1_StatusShareResponse rsp;

  int32_t res = nodes_node1_StatusShareRequest_decode(
      transfer, transfer->payload_len, &msg, &buf_ptr);
  if (res < 0) {
    log_debug("nodes_node1_StatusShareRequest_decode() error\n");
    return;
  }

  // here we have the request struct. parse it!!
  log_debug("duration_sec: %lu\nboard_name.len: %u  board_name.len: %s\n",
           msg.duration_sec, msg.board_name.len, (char *)(msg.board_name.data));

  // prepare a response here
  rsp.status = 0; // OK
  memcpy(rsp.reason.data, (uint8_t *)"OK\0", 3);
  rsp.reason.len = strlen((char *)(rsp.reason.data));

  // send the response here
  uint8_t buf_rsp[NODES_NODE1_STATUSSHARE_RESPONSE_MAX_SIZE];
  uint32_t len = nodes_node1_StatusShareResponse_encode(&rsp, buf_rsp);

  int16_t ret_val = canardRequestOrRespond(
      ins, transfer->source_node_id, NODES_NODE1_STATUSSHARE_SIGNATURE,
      NODES_NODE1_STATUSSHARE_ID, &(transfer->transfer_id), transfer->priority,
      CanardResponse, buf_rsp, len);
  if (ret_val < 0) {
    log_debug("error %u\n", __LINE__);
  }
}

static void handler_ping(CanardInstance *ins, CanardRxTransfer *transfer) {
  uint8_t buf[NODES_NODE2_NODEPING_MAX_SIZE];
  uint8_t *buf_ptr = buf;
  nodes_node2_NodePing msg;
  nodes_node2_NodePong pong;

  nodes_node2_NodePing_decode(transfer, transfer->payload_len, &msg, &buf_ptr);

  // here we decoded the coming message, print it!
  log_debug("msg.pinger_id: %u\n", msg.pinger_id);

  // prepare a pong message
  pong.pinger_id = msg.pinger_id;
  pong.ponger_id = LOCAL_NODE_ID;

  // send the response
  uint8_t buf_pong[NODES_NODE2_NODEPONG_MAX_SIZE];
  uint32_t len = nodes_node2_NodePong_encode(&pong, buf_pong);

  static uint8_t transfer_id = 0; // This variable MUST BE STATIC; refer to the
                                  // libcanard documentation for the background

  int16_t bc_res = canardBroadcast(ins, NODES_NODE2_NODEPONG_SIGNATURE,
                                   NODES_NODE2_NODEPONG_ID, &transfer_id,
                                   transfer->priority, buf_pong, len);
  if (bc_res < 0) {
    log_debug("broadcast failed\n");
  }
}

void onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer) {
  if (transfer->transfer_type == CanardTransferTypeRequest) {
    switch (transfer->data_type_id) {
    case NODES_NODE1_STATUSSHARE_ID:
      handler_status_share(ins, transfer);
      break;
    default:
      log_debug("unhandled data type\n");
      break;
    }

    return;
  }

  if (transfer->transfer_type == CanardTransferTypeBroadcast) {
    switch (transfer->data_type_id) {
    case NODES_NODE2_NODEPING_ID:
      handler_ping(ins, transfer);
      break;
    case NODES_NODE2_NODEPONG_ID:
      log_debug("PONG RECEIVED, no need to send anything\n");
      break;
    default:
      log_debug("unknown type\n");
      break;
    }

    return;
  }

  log_debug("Unhandled transfer type\n");
}

void uavcanInit(void) {
  CanardSTM32CANTimings timings;
  int result =
      canardSTM32ComputeCANTimings(HAL_RCC_GetPCLK1Freq(), 1000000, &timings);
  if (result) {
    log_debug("canardSTM32ComputeCANTimings() failed\n");
  }

  result = canardSTM32Init(&timings, CanardSTM32IfaceModeNormal);
  if (result) {
    log_debug("canardSTM32Init() failed\n");
  }

  canardInit(
      &g_canard,            // Uninitialized library instance
      g_canard_memory_pool, // Raw memory chunk used for dynamic allocation
      sizeof(g_canard_memory_pool), // Size of the above, in bytes
      onTransferReceived,           // Callback, see CanardOnTransferReception
      shouldAcceptTransfer,         // Callback, see CanardShouldAcceptTransfer
      NULL);

  canardSetLocalNodeID(&g_canard, LOCAL_NODE_ID);
}

void sendCanard(void) {
  const CanardCANFrame *txf = canardPeekTxQueue(&g_canard);
  while (txf) {
    const int tx_res = canardSTM32Transmit(txf);
    if (tx_res < 0) {
      log_debug("canardSTM32Transmit() failed\n)");
    }
    if (tx_res > 0) {
      canardPopTxQueue(&g_canard);
    }
    txf = canardPeekTxQueue(&g_canard);
  }
}

void receiveCanard(void) {
  CanardCANFrame rx_frame;
  int res = canardSTM32Receive(&rx_frame);
  if (res) {
    canardHandleRxFrame(&g_canard, &rx_frame, osKernelGetTickCount() * 1000);
  }
}

void spinCanard(void) {
  static uint32_t spin_time = 0;
  if (osKernelGetTickCount() < (spin_time + CANARD_SPIN_PERIOD)) {
    return; // rate limiting
  }
  spin_time = osKernelGetTickCount();

  // send the response
  nodes_node2_NodePing ping;

  ping.pinger_id = LOCAL_NODE_ID;

  uint8_t buf_ping[NODES_NODE2_NODEPING_MAX_SIZE];
  uint32_t len = nodes_node2_NodePing_encode(&ping, buf_ping);

  static uint8_t transfer_id = 0; // This variable MUST BE STATIC; refer to the
                                  // libcanard documentation for the background

  int16_t bc_res = canardBroadcast(&g_canard, NODES_NODE2_NODEPING_SIGNATURE,
                                   NODES_NODE2_NODEPING_ID, &transfer_id,
                                   CANARD_TRANSFER_PRIORITY_LOW, buf_ping, len);
  if (bc_res < 0) {
    log_debug("broadcast failed: %d\n\r", bc_res);
  }
#ifdef RENODE_BUILD
  else {
    log_debug("broadcast sent\n\r");
  }
#endif
}

#endif
