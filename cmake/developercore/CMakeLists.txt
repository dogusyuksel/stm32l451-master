cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)

set(LIBCSP_DIR ${CMAKE_SOURCE_DIR}/Dev/libcspv2)
set(LIBCSP_INCLUDE ${LIBCSP_DIR}/include)
set(LIBCSP_CONF_INCLUDE ${LIBCSP_DIR}/build/include)
set(LIBCSP_LIB ${LIBCSP_DIR}/build/libcsp.a)

# developercore generated include paths
set(Dev_Include_Dirs
    ${LIBCSP_CONF_INCLUDE}
    ${LIBCSP_INCLUDE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/logging/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/cm_backtrace/Languages/en-US
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/cm_backtrace
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/libcanard
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/libcanard/drivers/stm32
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/libcanard_auto_generated
)

# developercore generated application sources
set(Dev_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/bsp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/uart_handlers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/freertos_over.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/logging/src/printf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/logging/src/logging.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/cm_backtrace/cm_backtrace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/libcanard/canard.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/libcanard/drivers/stm32/canard_stm32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/libcanard_handlers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/libcspv2/contrib/atomics/atomics_freertos_gcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Dev/libcsp2_handlers.c
)

# Link directories setup
set(MX_LINK_DIRS

)
# Project static libraries
set(MX_LINK_LIBS 
    ${TOOLCHAIN_LINK_LIBRARIES}
	${LIBCSP_LIB}
)
# Interface library for includes and symbols
add_library(developercore INTERFACE)
target_include_directories(developercore INTERFACE ${Dev_Include_Dirs})
target_compile_definitions(developercore INTERFACE ${Dev_Defines_Syms})

# Add developercore generated application sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${Dev_Application_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-fno-omit-frame-pointer>
)

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that developercore code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
