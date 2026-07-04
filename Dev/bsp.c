#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsp.h"
#include "uart_handlers.h"
#include "main.h"
#include "usart.h"
#include "logging.h"
#include "stm32l4xx_hal.h"

static uint8_t uart1_rx_byte;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart1) {
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart1_rx_byte, 1);
        custom_uart_interrupt_handler(uart1_rx_byte);
    }
}

static void uart_send(uint8_t *buffer, uint32_t len) {
  HAL_UART_Transmit(&huart1, (uint8_t *)buffer, len, len);
}

void board_init(void) {
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart1_rx_byte, 1); // arm uart it

  logging_init(uart_send, LEVEL_DEBUG);
  log_debug("Board initialized\n\r");
}
