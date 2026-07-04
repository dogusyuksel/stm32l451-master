#ifndef _CUSTOM_CMB_CFG_H_
#define _CUSTOM_CMB_CFG_H_

#include "logging.h"

/* print line, must config by user */
#define cmb_println(...) log_debug(__VA_ARGS__);log_debug("\r\n")
#define CMB_USING_OS_PLATFORM
#define CMB_OS_PLATFORM_TYPE CMB_OS_PLATFORM_FREERTOS
#define CMB_CPU_PLATFORM_TYPE CMB_CPU_ARM_CORTEX_M4
#define CMB_USING_DUMP_STACK_INFO
#define CMB_PRINT_LANGUAGE CMB_PRINT_LANGUAGE_ENGLISH

#endif /* _CUSTOM_CMB_CFG_H_ */
