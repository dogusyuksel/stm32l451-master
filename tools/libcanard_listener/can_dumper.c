// This is needed to enable necessary declarations in sys/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <canard.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <socketcan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ussp/generic/ResetInfo.h>

static uint8_t LOCAL_NODE_ID = 5;
static uint8_t SERVER_NODE_ID = 5;
static char INTERFACE_NAME[128] = "can0";
static bool TEST_MODE = false;

#define VERSION "00.01"
#define CLIENT_DEFAULT_NODE_ID 6U
#define DECODE_SCRATCH_SIZE 4096U
#define FAKE_CAUSE_COUNT 3U
#define FAKE_WATCHDOG_TASK_COUNT 3U
#define FAKE_BACKTRACE_COUNT 5U
#define FAKE_RESET_TASK_COUNT 4U

static CanardInstance canard;
static uint8_t canard_memory_pool[8192];

static ussp_generic_ResetReason fake_causes[FAKE_CAUSE_COUNT];
static uint8_t fake_cause_name_0[USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH];
static uint8_t fake_cause_name_1[USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH];
static uint8_t fake_cause_name_2[USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH];
static uint8_t *const fake_cause_names[FAKE_CAUSE_COUNT] = {
    fake_cause_name_0,
    fake_cause_name_1,
    fake_cause_name_2,
};

static ussp_generic_TaskInfo fake_watchdog_tasks[FAKE_WATCHDOG_TASK_COUNT];
static uint8_t fake_watchdog_task_name_0[USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH];
static uint8_t fake_watchdog_task_name_1[USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH];
static uint8_t fake_watchdog_task_name_2[USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH];
static uint8_t *const fake_watchdog_task_names[FAKE_WATCHDOG_TASK_COUNT] = {
    fake_watchdog_task_name_0,
    fake_watchdog_task_name_1,
    fake_watchdog_task_name_2,
};

static uint32_t fake_backtrace[FAKE_BACKTRACE_COUNT];

static ussp_generic_TaskInfo fake_reset_tasks[FAKE_RESET_TASK_COUNT];
static uint8_t fake_reset_task_name_0[USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH];
static uint8_t fake_reset_task_name_1[USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH];
static uint8_t fake_reset_task_name_2[USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH];
static uint8_t fake_reset_task_name_3[USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH];
static uint8_t *const fake_reset_task_names[FAKE_RESET_TASK_COUNT] = {
    fake_reset_task_name_0,
    fake_reset_task_name_1,
    fake_reset_task_name_2,
    fake_reset_task_name_3,
};

static uint8_t fake_current_task[USSP_GENERIC_FREERTOSPS_CURRENT_TASK_MAX_LENGTH];

static uint64_t getMonotonicTimestampUSec(void) {
    struct timespec ts;
    memset(&ts, 0, sizeof(ts));
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        abort();
    }
    return (uint64_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);
}

static void set_u8_array(uint8_t *dst, uint8_t *len, const char *text, uint8_t max_len) {
    const size_t text_len = strlen(text);
    const size_t copy_len = text_len < max_len ? text_len : max_len;

    memcpy(dst, text, copy_len);
    *len = (uint8_t)copy_len;
}

static void fill_task(ussp_generic_TaskInfo *task, uint8_t priority, char state, const char *name,
                      uint16_t stack_high_water, uint16_t cpu_x100, uint8_t *name_storage) {
    memset(task, 0, sizeof(*task));
    task->priority = priority;
    task->state[0] = (uint8_t)state;
    task->name.data = name_storage;
    set_u8_array(name_storage, &task->name.len, name, USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH);
    task->task_stack_high_water = stack_high_water;
    task->percentage_time_x100 = cpu_x100;
}

static void fill_fake_reset_info_response(ussp_generic_ResetInfoResponse *response) {
    memset(response, 0, sizeof(*response));
    memset(fake_causes, 0, sizeof(fake_causes));
    memset(fake_watchdog_tasks, 0, sizeof(fake_watchdog_tasks));
    memset(fake_reset_tasks, 0, sizeof(fake_reset_tasks));

    fake_causes[0].name.data = fake_cause_names[0];
    set_u8_array(fake_cause_names[0], &fake_causes[0].name.len, "power_on", USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH);
    fake_causes[1].name.data = fake_cause_names[1];
    set_u8_array(fake_cause_names[1], &fake_causes[1].name.len, "watchdog", USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH);
    fake_causes[2].name.data = fake_cause_names[2];
    set_u8_array(fake_cause_names[2], &fake_causes[2].name.len, "assert", USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH);

    fill_task(&fake_watchdog_tasks[0], 3, 'B', "can_rx", 312, 1250, fake_watchdog_task_names[0]);
    fill_task(&fake_watchdog_tasks[1], 4, 'R', "logger", 488, 410, fake_watchdog_task_names[1]);
    fill_task(&fake_watchdog_tasks[2], 7, 'S', "storage", 144, 845, fake_watchdog_task_names[2]);

    fake_backtrace[0] = 0x08001234U;
    fake_backtrace[1] = 0x08004567U;
    fake_backtrace[2] = 0x0800789AU;
    fake_backtrace[3] = 0x0800ABCDU;
    fake_backtrace[4] = 0x0800F00DU;

    fill_task(&fake_reset_tasks[0], 2, 'R', "idle", 900, 5, fake_reset_task_names[0]);
    fill_task(&fake_reset_tasks[1], 5, 'B', "telemetry", 240, 2120, fake_reset_task_names[1]);
    fill_task(&fake_reset_tasks[2], 6, 'S', "uavcan", 128, 3300, fake_reset_task_names[2]);
    fill_task(&fake_reset_tasks[3], 8, 'R', "diagnostics", 384, 650, fake_reset_task_names[3]);

    response->causes.len = FAKE_CAUSE_COUNT;
    response->causes.data = fake_causes;
    response->watchdog.blocking_tasks.len = FAKE_WATCHDOG_TASK_COUNT;
    response->watchdog.blocking_tasks.data = fake_watchdog_tasks;
    response->assert_info.backtrace.len = FAKE_BACKTRACE_COUNT;
    response->assert_info.backtrace.data = fake_backtrace;
    response->assert_info.sp = 0x20004FF0U;
    response->assert_info.r0 = 0x11111111U;
    response->assert_info.r1 = 0x22222222U;
    response->assert_info.r2 = 0x33333333U;
    response->assert_info.r3 = 0x44444444U;
    response->assert_info.r12 = 0x12121212U;
    response->assert_info.psr = 0x21000000U;
    response->assert_info.primask = 0U;
    response->assert_info.interrupt_nesting = true;
    response->assert_info.valid_assert = true;
    response->assert_info.state_at_reset.system_tasks.len = FAKE_RESET_TASK_COUNT;
    response->assert_info.state_at_reset.system_tasks.data = fake_reset_tasks;
    response->assert_info.state_at_reset.current_task.data = fake_current_task;
    set_u8_array(fake_current_task, &response->assert_info.state_at_reset.current_task.len, "uavcan",
                 USSP_GENERIC_FREERTOSPS_CURRENT_TASK_MAX_LENGTH);
    response->assert_info.state_at_reset.memory_free = 54321U;
    response->assert_info.state_at_reset.scheduler_running = true;
    response->assert_info.state_at_reset.in_task = true;
    response->assert_info.state_at_reset.num_tasks = 3;
    response->success = 1;
}

static uint16_t encode_reset_info_request(uint8_t *buffer, uint8_t reset_reg) {
    ussp_generic_ResetInfoRequest request;
    memset(&request, 0, sizeof(request));
    request.reset_reg = reset_reg;
    return (uint16_t)ussp_generic_ResetInfoRequest_encode(&request, buffer);
}

static uint16_t encode_reset_info_response(uint8_t *buffer) {
    ussp_generic_ResetInfoResponse response;
    fill_fake_reset_info_response(&response);
    return (uint16_t)ussp_generic_ResetInfoResponse_encode(&response, buffer);
}

static int32_t decode_reset_info_request(const CanardRxTransfer *transfer, ussp_generic_ResetInfoRequest *request) {
    uint8_t decode_scratch[DECODE_SCRATCH_SIZE];
    uint8_t *decode_ptr = decode_scratch;
    memset(decode_scratch, 0, sizeof(decode_scratch));
    return ussp_generic_ResetInfoRequest_decode(transfer, transfer->payload_len, request, &decode_ptr);
}

static int32_t decode_reset_info_response(const CanardRxTransfer *transfer, ussp_generic_ResetInfoResponse *response) {
    uint8_t decode_scratch[DECODE_SCRATCH_SIZE];
    uint8_t *decode_ptr = decode_scratch;
    memset(decode_scratch, 0, sizeof(decode_scratch));
    return ussp_generic_ResetInfoResponse_decode(transfer, transfer->payload_len, response, &decode_ptr);
}

static void print_u8_text(const uint8_t *data, uint8_t len) {
    if (!data || len == 0U) {
        printf("<empty>");
        return;
    }
    printf("%.*s", (int)len, (const char *)data);
}

static void print_task_info(const char *prefix, uint8_t index, const ussp_generic_TaskInfo *task) {
    printf("%s[%u]: priority=%u state='%c' name=", prefix, (unsigned)index, (unsigned)task->priority,
           (char)task->state[0]);
    print_u8_text(task->name.data, task->name.len);
    printf(" name_len=%u stack_high_water=%u cpu_x100=%u\n", (unsigned)task->name.len,
           (unsigned)task->task_stack_high_water, (unsigned)task->percentage_time_x100);
}

static void print_reset_info_response_summary(const ussp_generic_ResetInfoResponse *response) {
    printf("ResetInfo response detail:\n");
    printf("  success=%u\n", (unsigned)response->success);
    printf("  causes.len=%u\n", (unsigned)response->causes.len);
    for (uint8_t i = 0; i < response->causes.len; i++) {
        printf("  causes[%u].name=", (unsigned)i);
        print_u8_text(response->causes.data[i].name.data, response->causes.data[i].name.len);
        printf(" name_len=%u\n", (unsigned)response->causes.data[i].name.len);
    }

    printf("  watchdog.blocking_tasks.len=%u\n", (unsigned)response->watchdog.blocking_tasks.len);
    for (uint8_t i = 0; i < response->watchdog.blocking_tasks.len; i++) {
        print_task_info("  watchdog.blocking_tasks", i, &response->watchdog.blocking_tasks.data[i]);
    }

    printf("  assert_info.backtrace.len=%u\n", (unsigned)response->assert_info.backtrace.len);
    for (uint8_t i = 0; i < response->assert_info.backtrace.len; i++) {
        printf("  assert_info.backtrace[%u]=0x%08lX\n", (unsigned)i,
               (unsigned long)response->assert_info.backtrace.data[i]);
    }

    printf("  assert_info.sp=0x%08lX\n", (unsigned long)response->assert_info.sp);
    printf("  assert_info.r0=0x%08lX\n", (unsigned long)response->assert_info.r0);
    printf("  assert_info.r1=0x%08lX\n", (unsigned long)response->assert_info.r1);
    printf("  assert_info.r2=0x%08lX\n", (unsigned long)response->assert_info.r2);
    printf("  assert_info.r3=0x%08lX\n", (unsigned long)response->assert_info.r3);
    printf("  assert_info.r12=0x%08lX\n", (unsigned long)response->assert_info.r12);
    printf("  assert_info.psr=0x%08lX\n", (unsigned long)response->assert_info.psr);
    printf("  assert_info.primask=0x%08lX\n", (unsigned long)response->assert_info.primask);
    printf("  assert_info.interrupt_nesting=%u\n", (unsigned)response->assert_info.interrupt_nesting);
    printf("  assert_info.valid_assert=%u\n", (unsigned)response->assert_info.valid_assert);

    printf("  assert_info.state_at_reset.system_tasks.len=%u\n",
           (unsigned)response->assert_info.state_at_reset.system_tasks.len);
    for (uint8_t i = 0; i < response->assert_info.state_at_reset.system_tasks.len; i++) {
        print_task_info("  assert_info.state_at_reset.system_tasks", i,
                        &response->assert_info.state_at_reset.system_tasks.data[i]);
    }

    printf("  assert_info.state_at_reset.current_task=");
    print_u8_text(response->assert_info.state_at_reset.current_task.data,
                  response->assert_info.state_at_reset.current_task.len);
    printf(" current_task_len=%u\n", (unsigned)response->assert_info.state_at_reset.current_task.len);
    printf("  assert_info.state_at_reset.memory_free=%lu\n",
           (unsigned long)response->assert_info.state_at_reset.memory_free);
    printf("  assert_info.state_at_reset.scheduler_running=%u\n",
           (unsigned)response->assert_info.state_at_reset.scheduler_running);
    printf("  assert_info.state_at_reset.in_task=%u\n", (unsigned)response->assert_info.state_at_reset.in_task);
    printf("  assert_info.state_at_reset.num_tasks=%u\n", (unsigned)response->assert_info.state_at_reset.num_tasks);
}

static void send_reset_info_request(void) {
    static uint8_t transfer_id;
    uint8_t buffer[USSP_GENERIC_RESETINFO_REQUEST_MAX_SIZE];
    const uint16_t payload_len = encode_reset_info_request(buffer, 0xA5U);

    const int16_t res = canardRequestOrRespond(&canard, SERVER_NODE_ID, USSP_GENERIC_RESETINFO_SIGNATURE,
                                               USSP_GENERIC_RESETINFO_ID, &transfer_id, CANARD_TRANSFER_PRIORITY_LOW,
                                               CanardRequest, buffer, payload_len);
    if (res <= 0) {
        fprintf(stderr, "Could not send ResetInfo request; error %d\n", res);
        return;
    }

    printf("Sent ResetInfo request to node %u, payload_len=%u, frames=%d\n", (unsigned)SERVER_NODE_ID,
           (unsigned)payload_len, res);
}

static void send_reset_info_response(uint8_t destination_node_id, uint8_t transfer_id) {
    uint8_t buffer[USSP_GENERIC_RESETINFO_RESPONSE_MAX_SIZE];
    const uint16_t payload_len = encode_reset_info_response(buffer);

    const int16_t res =
        canardRequestOrRespond(&canard, destination_node_id, USSP_GENERIC_RESETINFO_SIGNATURE,
                               USSP_GENERIC_RESETINFO_ID, &transfer_id, CANARD_TRANSFER_PRIORITY_LOW, CanardResponse,
                               buffer, payload_len);
    if (res <= 0) {
        fprintf(stderr, "Could not send ResetInfo response; error %d\n", res);
        return;
    }

    printf("Sent fake ResetInfo response to node %u, payload_len=%u, frames=%d\n", (unsigned)destination_node_id,
           (unsigned)payload_len, res);
}

static void onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer) {
    (void)ins;

    if (!transfer || transfer->data_type_id != USSP_GENERIC_RESETINFO_ID) {
        return;
    }

    if (transfer->transfer_type == CanardTransferTypeRequest) {
        ussp_generic_ResetInfoRequest request;
        const int32_t decode_res = decode_reset_info_request(transfer, &request);
        if (decode_res < 0) {
            fprintf(stderr, "Failed to decode ResetInfo request; error %ld\n", (long)decode_res);
            return;
        }

        printf("Received ResetInfo request from node %u: reset_reg=0x%02X payload_len=%u\n",
               (unsigned)transfer->source_node_id, (unsigned)request.reset_reg, (unsigned)transfer->payload_len);
        if (!TEST_MODE) {
            send_reset_info_response(transfer->source_node_id, transfer->transfer_id);
        }
    } else if (transfer->transfer_type == CanardTransferTypeResponse) {
        ussp_generic_ResetInfoResponse response;
        const int32_t decode_res = decode_reset_info_response(transfer, &response);
        if (decode_res < 0) {
            fprintf(stderr, "Failed to decode ResetInfo response; error %ld\n", (long)decode_res);
            return;
        }

        printf("Received ResetInfo response from node %u, payload_len=%u\n", (unsigned)transfer->source_node_id,
               (unsigned)transfer->payload_len);
        print_reset_info_response_summary(&response);
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
        return "Unknown";
    }
}

static bool shouldAcceptTransfer(const CanardInstance *ins, uint64_t *out_data_type_signature, uint16_t data_type_id,
                                 CanardTransferType transfer_type, uint8_t source_node_id) {
    if (data_type_id != USSP_GENERIC_RESETINFO_ID) {
        return false;
    }

    *out_data_type_signature = USSP_GENERIC_RESETINFO_SIGNATURE;
    printf("RX candidate: from=%u to=%u type=%s data_type_id=%u\n", (unsigned)source_node_id,
           (unsigned)ins->node_id, transfer_type_to_str(transfer_type), (unsigned)data_type_id);
    return true;
}

static void process1HzTasks(uint64_t timestamp_usec) {
    canardCleanupStaleTransfers(&canard, timestamp_usec);

    if (TEST_MODE) {
        send_reset_info_request();
    }
}

static void processTxRxOnce(SocketCANInstance *socketcan, int32_t timeout_msec) {
    for (const CanardCANFrame *txf = NULL; (txf = canardPeekTxQueue(&canard)) != NULL;) {
        const int16_t tx_res = socketcanTransmit(socketcan, txf, 0);
        if (tx_res < 0) {
            canardPopTxQueue(&canard);
            fprintf(stderr, "Transmit error %d, frame dropped, errno '%s'\n", tx_res, strerror(errno));
        } else if (tx_res > 0) {
            canardPopTxQueue(&canard);
        } else {
            break;
        }
    }

    CanardCANFrame rx_frame;
    const uint64_t timestamp = getMonotonicTimestampUSec();
    const int16_t rx_res = socketcanReceive(socketcan, &rx_frame, timeout_msec);
    if (rx_res < 0) {
        fprintf(stderr, "Receive error %d, errno '%s'\n", rx_res, strerror(errno));
    } else if (rx_res > 0) {
        canardHandleRxFrame(&canard, &rx_frame, timestamp);
    }
}

static struct option parameters[] = {
    {"iface", required_argument, 0, 'i'}, {"nodeid", required_argument, 0, 'n'},
    {"dest", required_argument, 0, 'd'},  {"test", no_argument, 0, 't'},
    {"help", no_argument, 0, 'h'},        {NULL, 0, 0, 0},
};

static void show_usage_and_exit(char *appname) {
    if (appname) {
        printf("usage of %s ", appname);
    }
    printf("(version: %s)\n", VERSION);
    printf("\t\"--iface\"/\"-i\"\t:\tCAN interface (default: can0)\n");
    printf("\t\"--nodeid\"/\"-n\"\t:\tlocal node id (default: 5, or 6 in -t mode)\n");
    printf("\t\"--dest\"/\"-d\"\t:\tResetInfo server node id used by -t mode (default: 5)\n");
    printf("\t\"--test\"/\"-t\"\t:\tsend ResetInfo request every second\n");
    printf("\twithout -t this process answers ResetInfo requests with fake data\n");

    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int c = 0;
    int o = 0;
    bool node_id_was_set = false;
    char *endptr = NULL;
    uint64_t next_1hz_service_at = 0;
    int16_t res = 0;
    SocketCANInstance socketcan;

    while ((c = getopt_long(argc, argv, "i:n:d:th", parameters, &o)) != -1) {
        switch (c) {
        case 'i':
            strncpy(INTERFACE_NAME, optarg, sizeof(INTERFACE_NAME) - 1U);
            INTERFACE_NAME[sizeof(INTERFACE_NAME) - 1U] = '\0';
            break;
        case 'n':
            LOCAL_NODE_ID = (uint8_t)strtoul(optarg, &endptr, 10);
            node_id_was_set = true;
            break;
        case 'd':
            SERVER_NODE_ID = (uint8_t)strtoul(optarg, &endptr, 10);
            break;
        case 't':
            TEST_MODE = true;
            break;
        case 'h':
        default:
            show_usage_and_exit(argv[0]);
        }
    }

    if (TEST_MODE && !node_id_was_set) {
        LOCAL_NODE_ID = CLIENT_DEFAULT_NODE_ID;
    }

    printf("Configuration:\n");
    printf("\tinterface : %s\n", INTERFACE_NAME);
    printf("\tnode id   : %u\n", (unsigned)LOCAL_NODE_ID);
    printf("\tmode      : %s\n", TEST_MODE ? "tester/requester" : "fake responder");
    printf("\tdest node : %u\n\n", (unsigned)SERVER_NODE_ID);

    res = socketcanInit(&socketcan, INTERFACE_NAME);
    if (res < 0) {
        fprintf(stderr, "Failed to open CAN iface '%s'\n", INTERFACE_NAME);
        return 1;
    }

    canardInit(&canard, canard_memory_pool, sizeof(canard_memory_pool), onTransferReceived, shouldAcceptTransfer, NULL);

    if (canardGetLocalNodeID(&canard) == 0) {
        canardSetLocalNodeID(&canard, LOCAL_NODE_ID);
    }

    next_1hz_service_at = getMonotonicTimestampUSec();

    for (;;) {
        processTxRxOnce(&socketcan, 10);

        const uint64_t ts = getMonotonicTimestampUSec();
        if (ts >= next_1hz_service_at) {
            next_1hz_service_at += 1000000;
            process1HzTasks(ts);
        }
    }

    return 0;
}
