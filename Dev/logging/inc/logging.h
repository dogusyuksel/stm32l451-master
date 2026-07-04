#ifndef _LOGGING_
#define _LOGGING_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*logging_print_function_t)(uint8_t *, uint32_t);

typedef enum {
  LEVEL_NONE,
  LEVEL_DEBUG,
  LEVEL_WARNING,
  LEVEL_ERROR,
} log_level_t;

extern log_level_t log_level;

void logging_init(logging_print_function_t cb, log_level_t level);
void print_log(const char *format, ...);
void set_log_level(log_level_t level);
log_level_t get_log_level(void);

#define log_debug(format, ...) \
    do { \
        if (log_level == LEVEL_DEBUG) { \
            print_log(format, ##__VA_ARGS__); \
        } \
    } while(0)

#define log_warning(format, ...) \
    do { \
        if ((log_level == LEVEL_DEBUG) || (log_level == LEVEL_WARNING)) { \
            print_log(format, ##__VA_ARGS__); \
        } \
    } while(0)

#define log_error(format, ...) \
    do { \
        if ((log_level == LEVEL_DEBUG) || (log_level == LEVEL_WARNING) || (log_level == LEVEL_ERROR)) { \
            print_log(format, ##__VA_ARGS__); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif
#endif
