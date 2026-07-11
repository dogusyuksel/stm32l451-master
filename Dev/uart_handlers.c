#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uart_handlers.h"
#include "main.h"
#include "usart.h"
#include "logging.h"
#include "stm32l4xx_hal.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"

static uint8_t uart_buffer[UART_RX_BUFFER_SIZE];
static uint32_t uart_buffer_index = 0;
static uint32_t last_data_tick = 0;

static uint8_t buffer_contains(const uint8_t *buffer, uint32_t buffer_len,
                               const char *needle) {
  if (!buffer || !needle) {
    return 0;
  }

  uint32_t needle_len = strlen(needle);
  if (needle_len == 0 || buffer_len < needle_len) {
    return 0;
  }

  for (uint32_t i = 0; i <= (buffer_len - needle_len); i++) {
    if (memcmp(&buffer[i], needle, needle_len) == 0) {
      return 1;
    }
  }

  return 0;
}

void custom_uart_interrupt_handler(uint8_t byte) {
  uart_buffer[uart_buffer_index++] = byte;
  if (uart_buffer_index >= UART_RX_BUFFER_SIZE) {
    uart_buffer_index = 0;
  }
  last_data_tick = osKernelGetTickCount();
}

void custom_uart_handler(uint32_t current_tick) {
  if (uart_buffer_index > 0 && (current_tick - last_data_tick) >= UART_RX_TIMEOUT_MS) {
    // Process the received data
    if (buffer_contains(uart_buffer, uart_buffer_index, TRIGGER_FAULT_COMMAND)) {
      // just tp show backtrace is working
      __asm volatile(".word 0xFFFFFFFF");
    }else if (buffer_contains(uart_buffer, uart_buffer_index, TRIGGER_WDT_COMMAND)) {
        while(1){} // trigger wdt
    } else {
      log_debug("Received UART data: ");
      for (uint32_t i = 0; i < uart_buffer_index; i++) {
        log_debug("%02X ", uart_buffer[i]);
      }
      log_debug("\n\r");
    }

    // Reset the buffer index after processing
    uart_buffer_index = 0;
  }
}
