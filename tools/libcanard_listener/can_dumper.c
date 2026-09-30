// This is needed to enable necessary declarations in sys/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <canard.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
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
static bool VERBOSE = false;

#define VERSION "00.01"
#define CLIENT_DEFAULT_NODE_ID 6U
#define DECODE_SCRATCH_SIZE 8192U
#define RESETINFO_CAUSE_SLOT_COUNT 3U
#define WATCHDOG_TASK_SLOT_COUNT 3U
#define RESET_TASK_SLOT_COUNT 4U
#define FAKE_CAUSE_COUNT 3U
#define FAKE_WATCHDOG_TASK_COUNT 3U
#define FAKE_BACKTRACE_COUNT 5U
#define FAKE_RESET_TASK_COUNT 4U
#define MAX_VERBOSE_PAYLOAD_DUMP_SIZE USSP_GENERIC_RESETINFO_RESPONSE_MAX_SIZE

static CanardInstance canard;
static uint8_t canard_memory_pool[8192];
static uint8_t decode_scratch[DECODE_SCRATCH_SIZE];
static size_t decode_scratch_used = 0U;

static uint8_t fake_cause_name_0[USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH];
static uint8_t fake_cause_name_1[USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH];
static uint8_t fake_cause_name_2[USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH];
static uint8_t *const fake_cause_names[FAKE_CAUSE_COUNT] = {
    fake_cause_name_0,
    fake_cause_name_1,
    fake_cause_name_2,
};

static uint8_t fake_watchdog_task_name_0[USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH];
static uint8_t fake_watchdog_task_name_1[USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH];
static uint8_t fake_watchdog_task_name_2[USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH];
static uint8_t *const fake_watchdog_task_names[FAKE_WATCHDOG_TASK_COUNT] = {
    fake_watchdog_task_name_0,
    fake_watchdog_task_name_1,
    fake_watchdog_task_name_2,
};

static uint32_t fake_backtrace[FAKE_BACKTRACE_COUNT];

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

static char *transfer_type_to_str(CanardTransferType type);

static uint64_t getMonotonicTimestampUSec(void) {
    struct timespec ts;
    memset(&ts, 0, sizeof(ts));
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        abort();
    }
    return (uint64_t)(ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL);
}

static void verbose_log(const char *fmt, ...) {
    if (!VERBOSE) {
        return;
    }

    va_list args;
    fprintf(stderr, "[%llu us] DEBUG ", (unsigned long long)getMonotonicTimestampUSec());
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static void verbose_dump_bytes(const char *label, const uint8_t *data, uint16_t len) {
    if (!VERBOSE) {
        return;
    }

    fprintf(stderr, "[%llu us] DEBUG %s len=%u\n", (unsigned long long)getMonotonicTimestampUSec(), label,
            (unsigned)len);
    if (!data && len > 0U) {
        fprintf(stderr, "  <null>\n");
        return;
    }

    for (uint16_t i = 0; i < len; i = (uint16_t)(i + 16U)) {
        const uint16_t remaining = (uint16_t)(len - i);
        const uint16_t row_len = remaining < 16U ? remaining : 16U;
        fprintf(stderr, "  %04u:", (unsigned)i);
        for (uint16_t j = 0; j < row_len; j++) {
            fprintf(stderr, " %02X", (unsigned)data[i + j]);
        }
        fprintf(stderr, "\n");
    }
}

static void verbose_log_pool_stats(const char *label) {
    if (!VERBOSE) {
        return;
    }

    const CanardPoolAllocatorStatistics stats = canardGetPoolAllocatorStatistics(&canard);
    verbose_log("%s pool: capacity_blocks=%u current_usage_blocks=%u peak_usage_blocks=%u", label,
                (unsigned)stats.capacity_blocks, (unsigned)stats.current_usage_blocks,
                (unsigned)stats.peak_usage_blocks);
}

static bool ptr_in_decode_scratch(const void *ptr, size_t len) {
    const uintptr_t start = (uintptr_t)decode_scratch;
    const uintptr_t end = start + sizeof(decode_scratch);
    const uintptr_t p = (uintptr_t)ptr;

    if (!ptr) {
        return false;
    }
    if (p < start || p > end) {
        return false;
    }
    return len <= (size_t)(end - p);
}

static uint8_t clamp_u8_len(const char *field_name, uint8_t len, uint8_t max_len) {
    if (len > max_len) {
        verbose_log("invalid %s len=%u max=%u; clamping print to max", field_name, (unsigned)len,
                    (unsigned)max_len);
        return max_len;
    }
    return len;
}

static void set_u8_array(uint8_t *dst, uint8_t *len, const char *text, uint8_t max_len) {
    const size_t text_len = strlen(text);
    const size_t copy_len = text_len < max_len ? text_len : max_len;

    memcpy(dst, text, copy_len);
    *len = (uint8_t)copy_len;
}

static ussp_generic_ResetReason *reset_info_cause_slot(ussp_generic_ResetInfoResponse *response, uint8_t index) {
    switch (index) {
    case 0:
        return &response->cause0;
    case 1:
        return &response->cause1;
    case 2:
        return &response->cause2;
    default:
        return NULL;
    }
}

static const ussp_generic_ResetReason *reset_info_cause_slot_const(const ussp_generic_ResetInfoResponse *response,
                                                                   uint8_t index) {
    switch (index) {
    case 0:
        return &response->cause0;
    case 1:
        return &response->cause1;
    case 2:
        return &response->cause2;
    default:
        return NULL;
    }
}

static ussp_generic_TaskInfo *watchdog_task_slot(ussp_generic_WatchdogMonitor *watchdog, uint8_t index) {
    switch (index) {
    case 0:
        return &watchdog->blocking_task0;
    case 1:
        return &watchdog->blocking_task1;
    case 2:
        return &watchdog->blocking_task2;
    default:
        return NULL;
    }
}

static const ussp_generic_TaskInfo *watchdog_task_slot_const(const ussp_generic_WatchdogMonitor *watchdog,
                                                            uint8_t index) {
    switch (index) {
    case 0:
        return &watchdog->blocking_task0;
    case 1:
        return &watchdog->blocking_task1;
    case 2:
        return &watchdog->blocking_task2;
    default:
        return NULL;
    }
}

static ussp_generic_TaskInfo *reset_task_slot(ussp_generic_FreeRTOSPS *state, uint8_t index) {
    switch (index) {
    case 0:
        return &state->system_task0;
    case 1:
        return &state->system_task1;
    case 2:
        return &state->system_task2;
    case 3:
        return &state->system_task3;
    default:
        return NULL;
    }
}

static const ussp_generic_TaskInfo *reset_task_slot_const(const ussp_generic_FreeRTOSPS *state, uint8_t index) {
    switch (index) {
    case 0:
        return &state->system_task0;
    case 1:
        return &state->system_task1;
    case 2:
        return &state->system_task2;
    case 3:
        return &state->system_task3;
    default:
        return NULL;
    }
}

static void fill_reset_reason(ussp_generic_ResetReason *reason, const char *name, uint8_t *name_storage) {
    memset(reason, 0, sizeof(*reason));
    reason->name.data = name_storage;
    set_u8_array(name_storage, &reason->name.len, name, USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH);
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

    fill_reset_reason(reset_info_cause_slot(response, 0), "power_on", fake_cause_names[0]);
    fill_reset_reason(reset_info_cause_slot(response, 1), "watchdog", fake_cause_names[1]);
    fill_reset_reason(reset_info_cause_slot(response, 2), "assert", fake_cause_names[2]);

    fill_task(watchdog_task_slot(&response->watchdog, 0), 3, 'B', "can_rx", 312, 1250,
              fake_watchdog_task_names[0]);
    fill_task(watchdog_task_slot(&response->watchdog, 1), 4, 'R', "logger", 488, 410,
              fake_watchdog_task_names[1]);
    fill_task(watchdog_task_slot(&response->watchdog, 2), 7, 'S', "storage", 144, 845,
              fake_watchdog_task_names[2]);

    fake_backtrace[0] = 0x08001234U;
    fake_backtrace[1] = 0x08004567U;
    fake_backtrace[2] = 0x0800789AU;
    fake_backtrace[3] = 0x0800ABCDU;
    fake_backtrace[4] = 0x0800F00DU;

    fill_task(reset_task_slot(&response->assert_info.state_at_reset, 0), 2, 'R', "idle", 900, 5,
              fake_reset_task_names[0]);
    fill_task(reset_task_slot(&response->assert_info.state_at_reset, 1), 5, 'B', "telemetry", 240, 2120,
              fake_reset_task_names[1]);
    fill_task(reset_task_slot(&response->assert_info.state_at_reset, 2), 6, 'S', "uavcan", 128, 3300,
              fake_reset_task_names[2]);
    fill_task(reset_task_slot(&response->assert_info.state_at_reset, 3), 8, 'R', "diagnostics", 384, 650,
              fake_reset_task_names[3]);

    response->causes_len = FAKE_CAUSE_COUNT;
    response->watchdog.blocking_tasks_len = FAKE_WATCHDOG_TASK_COUNT;
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
    response->assert_info.state_at_reset.system_tasks_len = FAKE_RESET_TASK_COUNT;
    response->assert_info.state_at_reset.current_task.data = fake_current_task;
    set_u8_array(fake_current_task, &response->assert_info.state_at_reset.current_task.len, "uavcan",
                 USSP_GENERIC_FREERTOSPS_CURRENT_TASK_MAX_LENGTH);
    response->assert_info.state_at_reset.memory_free = 54321U;
    response->assert_info.state_at_reset.scheduler_running = true;
    response->assert_info.state_at_reset.in_task = true;
    response->assert_info.state_at_reset.num_tasks = response->assert_info.state_at_reset.system_tasks_len;
    response->success = 1;
}

static bool validate_reset_reason_for_encode(const char *field_name, const ussp_generic_ResetReason *reason) {
    bool valid = true;
    if (!reason) {
        fprintf(stderr, "%s slot is NULL\n", field_name);
        return false;
    }
    if (reason->name.len > USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH) {
        fprintf(stderr, "%s.name.len=%u exceeds max %u\n", field_name, (unsigned)reason->name.len,
                (unsigned)USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH);
        valid = false;
    }
    if (reason->name.len > 0U && !reason->name.data) {
        fprintf(stderr, "%s.name.data is NULL while len=%u\n", field_name, (unsigned)reason->name.len);
        valid = false;
    }
    return valid;
}

static bool validate_task_for_encode(const char *field_name, const ussp_generic_TaskInfo *task) {
    bool valid = true;
    if (!task) {
        fprintf(stderr, "%s slot is NULL\n", field_name);
        return false;
    }
    if (task->name.len > USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH) {
        fprintf(stderr, "%s.name.len=%u exceeds max %u\n", field_name, (unsigned)task->name.len,
                (unsigned)USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH);
        valid = false;
    }
    if (task->name.len > 0U && !task->name.data) {
        fprintf(stderr, "%s.name.data is NULL while len=%u\n", field_name, (unsigned)task->name.len);
        valid = false;
    }
    return valid;
}

static bool validate_fake_reset_info_response_for_encode(const ussp_generic_ResetInfoResponse *response) {
    bool valid = true;

    if (response->causes_len > RESETINFO_CAUSE_SLOT_COUNT) {
        fprintf(stderr, "causes.len=%u exceeds max %u\n", (unsigned)response->causes_len,
                (unsigned)RESETINFO_CAUSE_SLOT_COUNT);
        valid = false;
    }
    for (uint8_t i = 0; i < response->causes_len && i < RESETINFO_CAUSE_SLOT_COUNT; i++) {
        valid = validate_reset_reason_for_encode("causes", reset_info_cause_slot_const(response, i)) && valid;
    }

    if (response->watchdog.blocking_tasks_len > WATCHDOG_TASK_SLOT_COUNT) {
        fprintf(stderr, "watchdog.blocking_tasks.len=%u exceeds max %u\n",
                (unsigned)response->watchdog.blocking_tasks_len, (unsigned)WATCHDOG_TASK_SLOT_COUNT);
        valid = false;
    }
    for (uint8_t i = 0; i < response->watchdog.blocking_tasks_len && i < WATCHDOG_TASK_SLOT_COUNT; i++) {
        valid = validate_task_for_encode("watchdog.blocking_tasks",
                                         watchdog_task_slot_const(&response->watchdog, i)) &&
                valid;
    }

    if (response->assert_info.backtrace.len > USSP_GENERIC_ASSERTINFO_BACKTRACE_MAX_LENGTH) {
        fprintf(stderr, "assert_info.backtrace.len=%u exceeds max %u\n",
                (unsigned)response->assert_info.backtrace.len,
                (unsigned)USSP_GENERIC_ASSERTINFO_BACKTRACE_MAX_LENGTH);
        valid = false;
    }
    if (response->assert_info.state_at_reset.system_tasks_len > RESET_TASK_SLOT_COUNT) {
        fprintf(stderr, "state_at_reset.system_tasks.len=%u exceeds max %u\n",
                (unsigned)response->assert_info.state_at_reset.system_tasks_len, (unsigned)RESET_TASK_SLOT_COUNT);
        valid = false;
    }
    for (uint8_t i = 0; i < response->assert_info.state_at_reset.system_tasks_len && i < RESET_TASK_SLOT_COUNT; i++) {
        valid = validate_task_for_encode("state_at_reset.system_tasks",
                                         reset_task_slot_const(&response->assert_info.state_at_reset, i)) &&
                valid;
    }
    if (response->assert_info.state_at_reset.current_task.len > USSP_GENERIC_FREERTOSPS_CURRENT_TASK_MAX_LENGTH) {
        fprintf(stderr, "state_at_reset.current_task.len=%u exceeds max %u\n",
                (unsigned)response->assert_info.state_at_reset.current_task.len,
                (unsigned)USSP_GENERIC_FREERTOSPS_CURRENT_TASK_MAX_LENGTH);
        valid = false;
    }
    if (response->assert_info.state_at_reset.num_tasks != response->assert_info.state_at_reset.system_tasks_len) {
        fprintf(stderr, "state_at_reset.num_tasks=%u does not match system_tasks.len=%u\n",
                (unsigned)response->assert_info.state_at_reset.num_tasks,
                (unsigned)response->assert_info.state_at_reset.system_tasks_len);
        valid = false;
    }

    return valid;
}

static uint16_t encode_reset_info_request(uint8_t *buffer, uint8_t reset_reg) {
    ussp_generic_ResetInfoRequest request;
    memset(&request, 0, sizeof(request));
    request.reset_reg = reset_reg;
    const uint16_t payload_len = (uint16_t)ussp_generic_ResetInfoRequest_encode(&request, buffer);
    verbose_log("encoded ResetInfo request: reset_reg=0x%02X payload_len=%u max=%u", (unsigned)reset_reg,
                (unsigned)payload_len, (unsigned)USSP_GENERIC_RESETINFO_REQUEST_MAX_SIZE);
    verbose_dump_bytes("encoded ResetInfo request payload", buffer, payload_len);
    return payload_len;
}

static uint16_t encode_reset_info_response(uint8_t *buffer) {
    ussp_generic_ResetInfoResponse response;
    fill_fake_reset_info_response(&response);
    if (!validate_fake_reset_info_response_for_encode(&response)) {
        verbose_log("fake ResetInfo response validation failed before encode");
    }

    const uint16_t payload_len = (uint16_t)ussp_generic_ResetInfoResponse_encode(&response, buffer);
    verbose_log("encoded ResetInfo response: payload_len=%u max=%u causes=%u watchdog_tasks=%u backtrace=%u "
                "reset_tasks=%u current_task_len=%u",
                (unsigned)payload_len, (unsigned)USSP_GENERIC_RESETINFO_RESPONSE_MAX_SIZE,
                (unsigned)response.causes_len, (unsigned)response.watchdog.blocking_tasks_len,
                (unsigned)response.assert_info.backtrace.len,
                (unsigned)response.assert_info.state_at_reset.system_tasks_len,
                (unsigned)response.assert_info.state_at_reset.current_task.len);
    verbose_dump_bytes("encoded ResetInfo response payload", buffer, payload_len);
    return payload_len;
}

static int32_t decode_reset_info_request(const CanardRxTransfer *transfer, ussp_generic_ResetInfoRequest *request) {
    uint8_t *decode_ptr = decode_scratch;
    memset(decode_scratch, 0, sizeof(decode_scratch));
    const int32_t res = ussp_generic_ResetInfoRequest_decode(transfer, transfer->payload_len, request, &decode_ptr);
    decode_scratch_used = (size_t)(decode_ptr - decode_scratch);
    verbose_log("decoded ResetInfo request: res_bits=%ld payload_len=%u scratch_used=%lu reset_reg=0x%02X",
                (long)res, (unsigned)transfer->payload_len, (unsigned long)decode_scratch_used,
                (unsigned)request->reset_reg);
    return res;
}

static int32_t decode_reset_info_response(const CanardRxTransfer *transfer, ussp_generic_ResetInfoResponse *response) {
    uint8_t *decode_ptr = decode_scratch;
    memset(decode_scratch, 0, sizeof(decode_scratch));
    const int32_t res = ussp_generic_ResetInfoResponse_decode(transfer, transfer->payload_len, response, &decode_ptr);
    decode_scratch_used = (size_t)(decode_ptr - decode_scratch);
    verbose_log("decoded ResetInfo response: res_bits=%ld payload_len=%u scratch_used=%lu scratch_capacity=%u",
                (long)res, (unsigned)transfer->payload_len, (unsigned long)decode_scratch_used,
                (unsigned)DECODE_SCRATCH_SIZE);
    return res;
}

static void print_u8_text(const char *field_name, const uint8_t *data, uint8_t len, uint8_t max_len) {
    const uint8_t safe_len = clamp_u8_len(field_name, len, max_len);

    if (!data || len == 0U) {
        printf("<empty>");
        return;
    }
    if (!ptr_in_decode_scratch(data, safe_len)) {
        printf("<invalid-ptr:%p len=%u>", (const void *)data, (unsigned)safe_len);
        verbose_log("%s points outside decode scratch: ptr=%p len=%u", field_name, (const void *)data,
                    (unsigned)safe_len);
        return;
    }
    printf("%.*s", (int)safe_len, (const char *)data);
}

static void print_task_info(const char *prefix, uint8_t index, const ussp_generic_TaskInfo *task) {
    printf("%s[%u]: priority=%u state='%c' name=", prefix, (unsigned)index, (unsigned)task->priority,
           (char)task->state[0]);
    print_u8_text("TaskInfo.name", task->name.data, task->name.len, USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH);
    printf(" name_len=%u stack_high_water=%u cpu_x100=%u\n", (unsigned)task->name.len,
           (unsigned)task->task_stack_high_water, (unsigned)task->percentage_time_x100);
}

static void verbose_validate_u8_text_pointer(const char *field_name, const uint8_t *data, uint8_t len,
                                             uint8_t max_len) {
    if (!VERBOSE) {
        return;
    }

    const uint8_t safe_len = clamp_u8_len(field_name, len, max_len);
    if (len > 0U && !data) {
        verbose_log("%s has len=%u but data is NULL", field_name, (unsigned)len);
    } else if (data && !ptr_in_decode_scratch(data, safe_len)) {
        verbose_log("%s points outside decode scratch: ptr=%p len=%u", field_name, (const void *)data,
                    (unsigned)safe_len);
    }
}

static void validate_decoded_response_pointers(const ussp_generic_ResetInfoResponse *response) {
    if (!VERBOSE) {
        return;
    }

    const uint8_t causes_len = clamp_u8_len("causes.len", response->causes_len, RESETINFO_CAUSE_SLOT_COUNT);
    for (uint8_t i = 0; i < causes_len; i++) {
        const ussp_generic_ResetReason *reason = reset_info_cause_slot_const(response, i);
        if (reason) {
            verbose_validate_u8_text_pointer("causes.name", reason->name.data, reason->name.len,
                                             USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH);
        }
    }

    const uint8_t watchdog_len =
        clamp_u8_len("watchdog.blocking_tasks.len", response->watchdog.blocking_tasks_len, WATCHDOG_TASK_SLOT_COUNT);
    for (uint8_t i = 0; i < watchdog_len; i++) {
        const ussp_generic_TaskInfo *task = watchdog_task_slot_const(&response->watchdog, i);
        if (task) {
            verbose_validate_u8_text_pointer("watchdog.task.name", task->name.data, task->name.len,
                                             USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH);
        }
    }

    const uint8_t system_len = clamp_u8_len("state_at_reset.system_tasks.len",
                                            response->assert_info.state_at_reset.system_tasks_len,
                                            RESET_TASK_SLOT_COUNT);
    for (uint8_t i = 0; i < system_len; i++) {
        const ussp_generic_TaskInfo *task = reset_task_slot_const(&response->assert_info.state_at_reset, i);
        if (task) {
            verbose_validate_u8_text_pointer("state_at_reset.task.name", task->name.data, task->name.len,
                                             USSP_GENERIC_TASKINFO_NAME_MAX_LENGTH);
        }
    }

    const uint8_t backtrace_len =
        clamp_u8_len("assert_info.backtrace.len", response->assert_info.backtrace.len,
                     USSP_GENERIC_ASSERTINFO_BACKTRACE_MAX_LENGTH);
    if (response->assert_info.backtrace.data &&
        !ptr_in_decode_scratch(response->assert_info.backtrace.data,
                               (size_t)backtrace_len * sizeof(response->assert_info.backtrace.data[0]))) {
        verbose_log("assert_info.backtrace.data pointer is outside decode scratch: ptr=%p len=%u",
                    (const void *)response->assert_info.backtrace.data, (unsigned)backtrace_len);
    }
    verbose_validate_u8_text_pointer("state_at_reset.current_task",
                                     response->assert_info.state_at_reset.current_task.data,
                                     response->assert_info.state_at_reset.current_task.len,
                                     USSP_GENERIC_FREERTOSPS_CURRENT_TASK_MAX_LENGTH);
}

static void print_reset_info_response_summary(const ussp_generic_ResetInfoResponse *response) {
    validate_decoded_response_pointers(response);

    const uint8_t causes_len = clamp_u8_len("causes.len", response->causes_len, RESETINFO_CAUSE_SLOT_COUNT);
    const uint8_t watchdog_len =
        clamp_u8_len("watchdog.blocking_tasks.len", response->watchdog.blocking_tasks_len, WATCHDOG_TASK_SLOT_COUNT);
    const uint8_t backtrace_len =
        clamp_u8_len("assert_info.backtrace.len", response->assert_info.backtrace.len,
                     USSP_GENERIC_ASSERTINFO_BACKTRACE_MAX_LENGTH);
    const uint8_t system_tasks_len =
        clamp_u8_len("state_at_reset.system_tasks.len",
                     response->assert_info.state_at_reset.system_tasks_len,
                     RESET_TASK_SLOT_COUNT);
    const bool backtrace_data_ok =
        response->assert_info.backtrace.data &&
        ptr_in_decode_scratch(response->assert_info.backtrace.data,
                              (size_t)backtrace_len * sizeof(response->assert_info.backtrace.data[0]));

    printf("ResetInfo response detail:\n");
    printf("  success=%u\n", (unsigned)response->success);
    printf("  causes.len=%u\n", (unsigned)response->causes_len);
    for (uint8_t i = 0; i < causes_len; i++) {
        const ussp_generic_ResetReason *reason = reset_info_cause_slot_const(response, i);
        if (!reason) {
            printf("  causes[%u]=<missing-slot>\n", (unsigned)i);
            continue;
        }
        printf("  causes[%u].name=", (unsigned)i);
        print_u8_text("causes.name", reason->name.data, reason->name.len, USSP_GENERIC_RESETREASON_NAME_MAX_LENGTH);
        printf(" name_len=%u\n", (unsigned)reason->name.len);
    }

    printf("  watchdog.blocking_tasks.len=%u\n", (unsigned)response->watchdog.blocking_tasks_len);
    for (uint8_t i = 0; i < watchdog_len; i++) {
        const ussp_generic_TaskInfo *task = watchdog_task_slot_const(&response->watchdog, i);
        if (!task) {
            printf("  watchdog.blocking_tasks[%u]=<missing-slot>\n", (unsigned)i);
            continue;
        }
        print_task_info("  watchdog.blocking_tasks", i, task);
    }

    printf("  assert_info.backtrace.len=%u\n", (unsigned)response->assert_info.backtrace.len);
    if (response->assert_info.backtrace.data && !backtrace_data_ok) {
        printf("  assert_info.backtrace.data=<invalid-ptr:%p>\n",
               (const void *)response->assert_info.backtrace.data);
    }
    for (uint8_t i = 0; backtrace_data_ok && i < backtrace_len; i++) {
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
           (unsigned)response->assert_info.state_at_reset.system_tasks_len);
    for (uint8_t i = 0; i < system_tasks_len; i++) {
        const ussp_generic_TaskInfo *task = reset_task_slot_const(&response->assert_info.state_at_reset, i);
        if (!task) {
            printf("  assert_info.state_at_reset.system_tasks[%u]=<missing-slot>\n", (unsigned)i);
            continue;
        }
        print_task_info("  assert_info.state_at_reset.system_tasks", i, task);
    }

    printf("  assert_info.state_at_reset.current_task=");
    print_u8_text("state_at_reset.current_task", response->assert_info.state_at_reset.current_task.data,
                  response->assert_info.state_at_reset.current_task.len,
                  USSP_GENERIC_FREERTOSPS_CURRENT_TASK_MAX_LENGTH);
    printf(" current_task_len=%u\n", (unsigned)response->assert_info.state_at_reset.current_task.len);
    printf("  assert_info.state_at_reset.memory_free=%lu\n",
           (unsigned long)response->assert_info.state_at_reset.memory_free);
    printf("  assert_info.state_at_reset.scheduler_running=%u\n",
           (unsigned)response->assert_info.state_at_reset.scheduler_running);
    printf("  assert_info.state_at_reset.in_task=%u\n", (unsigned)response->assert_info.state_at_reset.in_task);
    printf("  assert_info.state_at_reset.num_tasks=%u\n", (unsigned)response->assert_info.state_at_reset.num_tasks);
}

static void verbose_dump_transfer_payload(const CanardRxTransfer *transfer) {
    if (!VERBOSE || !transfer) {
        return;
    }
    if (transfer->payload_len > MAX_VERBOSE_PAYLOAD_DUMP_SIZE) {
        verbose_log("payload_len=%u is larger than dump buffer=%u; skipping full payload dump",
                    (unsigned)transfer->payload_len, (unsigned)MAX_VERBOSE_PAYLOAD_DUMP_SIZE);
        return;
    }

    uint8_t payload[MAX_VERBOSE_PAYLOAD_DUMP_SIZE];
    memset(payload, 0, sizeof(payload));
    for (uint16_t i = 0; i < transfer->payload_len; i++) {
        const int16_t ret = canardDecodeScalar(transfer, (uint32_t)i * 8U, 8, false, &payload[i]);
        if (ret != 8) {
            verbose_log("failed to reconstruct payload byte %u; canardDecodeScalar ret=%d", (unsigned)i, ret);
            return;
        }
    }
    verbose_dump_bytes("received transfer payload", payload, transfer->payload_len);
}

static void send_reset_info_request(void) {
    static uint8_t transfer_id;
    uint8_t buffer[USSP_GENERIC_RESETINFO_REQUEST_MAX_SIZE];
    const uint8_t transfer_id_before = transfer_id;
    const uint16_t payload_len = encode_reset_info_request(buffer, 0xA5U);

    verbose_log_pool_stats("before ResetInfo request enqueue");
    const int16_t res = canardRequestOrRespond(&canard, SERVER_NODE_ID, USSP_GENERIC_RESETINFO_SIGNATURE,
                                               USSP_GENERIC_RESETINFO_ID, &transfer_id, CANARD_TRANSFER_PRIORITY_LOW,
                                               CanardRequest, buffer, payload_len);
    if (res <= 0) {
        fprintf(stderr, "Could not send ResetInfo request; error %d\n", res);
        return;
    }

    printf("Sent ResetInfo request to node %u, payload_len=%u, frames=%d\n", (unsigned)SERVER_NODE_ID,
           (unsigned)payload_len, res);
    verbose_log("request transfer_id before=%u after=%u destination=%u signature=0x%016llX type_id=%u",
                (unsigned)transfer_id_before, (unsigned)transfer_id, (unsigned)SERVER_NODE_ID,
                (unsigned long long)USSP_GENERIC_RESETINFO_SIGNATURE, (unsigned)USSP_GENERIC_RESETINFO_ID);
    verbose_log_pool_stats("after ResetInfo request enqueue");
}

static void send_reset_info_response(uint8_t destination_node_id, uint8_t transfer_id) {
    uint8_t buffer[USSP_GENERIC_RESETINFO_RESPONSE_MAX_SIZE];
    const uint16_t payload_len = encode_reset_info_response(buffer);

    verbose_log_pool_stats("before ResetInfo response enqueue");
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
    verbose_log("response transfer_id=%u destination=%u signature=0x%016llX type_id=%u", (unsigned)transfer_id,
                (unsigned)destination_node_id, (unsigned long long)USSP_GENERIC_RESETINFO_SIGNATURE,
                (unsigned)USSP_GENERIC_RESETINFO_ID);
    verbose_log_pool_stats("after ResetInfo response enqueue");
}

static void onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer) {
    (void)ins;

    if (!transfer) {
        verbose_log("onTransferReceived called with NULL transfer");
        return;
    }

    verbose_log("transfer received: source=%u type=%s transfer_id=%u priority=%u data_type_id=%u payload_len=%u "
                "payload_head=%p payload_middle=%p payload_tail=%p",
                (unsigned)transfer->source_node_id, transfer_type_to_str((CanardTransferType)transfer->transfer_type),
                (unsigned)transfer->transfer_id, (unsigned)transfer->priority, (unsigned)transfer->data_type_id,
                (unsigned)transfer->payload_len, (const void *)transfer->payload_head,
                (void *)transfer->payload_middle, (const void *)transfer->payload_tail);

    if (transfer->data_type_id != USSP_GENERIC_RESETINFO_ID) {
        verbose_log("ignoring transfer with data_type_id=%u expected=%u", (unsigned)transfer->data_type_id,
                    (unsigned)USSP_GENERIC_RESETINFO_ID);
        return;
    }
    verbose_dump_transfer_payload(transfer);

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
        } else {
            verbose_log("TEST_MODE is enabled; not responding to ResetInfo request");
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
    } else {
        verbose_log("ResetInfo transfer has unsupported transfer_type=%u", (unsigned)transfer->transfer_type);
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
        verbose_log("rejecting transfer candidate: source=%u type=%s data_type_id=%u expected=%u",
                    (unsigned)source_node_id, transfer_type_to_str(transfer_type), (unsigned)data_type_id,
                    (unsigned)USSP_GENERIC_RESETINFO_ID);
        return false;
    }

    *out_data_type_signature = USSP_GENERIC_RESETINFO_SIGNATURE;
    verbose_log("accepting transfer candidate: source=%u local=%u type=%s data_type_id=%u signature=0x%016llX",
                (unsigned)source_node_id, (unsigned)ins->node_id, transfer_type_to_str(transfer_type),
                (unsigned)data_type_id, (unsigned long long)*out_data_type_signature);
    return true;
}

static void process1HzTasks(uint64_t timestamp_usec) {
    canardCleanupStaleTransfers(&canard, timestamp_usec);
    verbose_log_pool_stats("after stale transfer cleanup");

    if (TEST_MODE) {
        send_reset_info_request();
    }
}

static void processTxRxOnce(SocketCANInstance *socketcan, int32_t timeout_msec) {
    for (const CanardCANFrame *txf = NULL; (txf = canardPeekTxQueue(&canard)) != NULL;) {
        verbose_log("TX frame queued: id=0x%08lX data_len=%u", (unsigned long)txf->id, (unsigned)txf->data_len);
        verbose_dump_bytes("TX CAN frame data", txf->data, txf->data_len);
        const int16_t tx_res = socketcanTransmit(socketcan, txf, 0);
        if (tx_res < 0) {
            canardPopTxQueue(&canard);
            fprintf(stderr, "Transmit error %d, frame dropped, errno '%s'\n", tx_res, strerror(errno));
        } else if (tx_res > 0) {
            canardPopTxQueue(&canard);
            verbose_log("TX frame sent successfully");
        } else {
            verbose_log("TX frame transmit timed out; keeping frame queued");
            break;
        }
    }

    CanardCANFrame rx_frame;
    const uint64_t timestamp = getMonotonicTimestampUSec();
    const int16_t rx_res = socketcanReceive(socketcan, &rx_frame, timeout_msec);
    if (rx_res < 0) {
        fprintf(stderr, "Receive error %d, errno '%s'\n", rx_res, strerror(errno));
    } else if (rx_res > 0) {
        verbose_log("RX CAN frame: id=0x%08lX data_len=%u", (unsigned long)rx_frame.id, (unsigned)rx_frame.data_len);
        verbose_dump_bytes("RX CAN frame data", rx_frame.data, rx_frame.data_len);
        canardHandleRxFrame(&canard, &rx_frame, timestamp);
    }
}

static struct option parameters[] = {
    {"iface", required_argument, 0, 'i'}, {"nodeid", required_argument, 0, 'n'},
    {"dest", required_argument, 0, 'd'},  {"test", no_argument, 0, 't'},
    {"verbose", no_argument, 0, 'v'},     {"help", no_argument, 0, 'h'},
    {NULL, 0, 0, 0},
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
    printf("\t\"--verbose\"/\"-v\"\t:\tprint payload, frame, encode/decode and allocator debug logs\n");
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

    while ((c = getopt_long(argc, argv, "i:n:d:tvh", parameters, &o)) != -1) {
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
        case 'v':
            VERBOSE = true;
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
    printf("\tdest node : %u\n", (unsigned)SERVER_NODE_ID);
    printf("\tverbose   : %s\n\n", VERBOSE ? "on" : "off");

    res = socketcanInit(&socketcan, INTERFACE_NAME);
    if (res < 0) {
        fprintf(stderr, "Failed to open CAN iface '%s'\n", INTERFACE_NAME);
        return 1;
    }

    canardInit(&canard, canard_memory_pool, sizeof(canard_memory_pool), onTransferReceived, shouldAcceptTransfer, NULL);

    if (canardGetLocalNodeID(&canard) == 0) {
        canardSetLocalNodeID(&canard, LOCAL_NODE_ID);
    }
    verbose_log("canard initialized: local_node_id=%u memory_pool_bytes=%u decode_scratch_bytes=%u",
                (unsigned)canardGetLocalNodeID(&canard), (unsigned)sizeof(canard_memory_pool),
                (unsigned)sizeof(decode_scratch));
    verbose_log_pool_stats("after canard init");

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
