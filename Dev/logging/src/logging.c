
#include "logging.h"
#include "printf.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

log_level_t log_level = LEVEL_NONE;
static uint8_t is_logging_enabled = 0;
static logging_print_function_t logging_print_function = NULL;

void logging_init(logging_print_function_t cb, log_level_t level) {
  logging_print_function = cb;
  log_level = level;
  is_logging_enabled = 1;
}

void print_log(const char *format, ...) {
  if (!is_logging_enabled) {
    return;
  }
  va_list arguments;
  char buffer[1024] = {0};

  va_start(arguments, format);
  vsnprintf_(buffer, sizeof(buffer), format, arguments);
  va_end(arguments);

  if (strlen(buffer) == 0) {
    return;
  }

  if (logging_print_function) {
    logging_print_function((uint8_t *)buffer,
                           strlen((char *)buffer));
  }
}

void set_log_level(log_level_t level) {
  log_level = level;
}

log_level_t get_log_level(void) {
  return log_level;
}
