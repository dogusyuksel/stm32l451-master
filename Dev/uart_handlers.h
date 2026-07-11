#ifndef __UART_HANDLERS_
#define __UART_HANDLERS_

#include <stdint.h>

#define TRIGGER_FAULT_COMMAND "trigger_fault"
#define TRIGGER_WDT_COMMAND "trigger_wdt"

#define UART_RX_BUFFER_SIZE 256
#define UART_RX_TIMEOUT_MS 100

void custom_uart_interrupt_handler(uint8_t byte);
void custom_uart_handler(uint32_t current_tick);

#endif // __UART_HANDLERS_
