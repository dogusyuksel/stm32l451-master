cmake_minimum_required(VERSION 3.22)

file(READ VERSION version)
string(REGEX REPLACE "\n$" "" version "${version}")

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME stm32l451-master)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(MY_ACTIVE_PRESET MATCHES "Linux")
    set(CMAKE_TOOLCHAIN_FILE
        ${CMAKE_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake
        CACHE STRING "Toolchain file")
endif()

# Core project settings
project(${CMAKE_PROJECT_NAME} VERSION ${version})
message("Build type: " ${CMAKE_BUILD_TYPE})

add_definitions(-DVERSION="${version}")
add_definitions(-DCMB_USER_CFG=1)
# enable below line if you want to use the can1 as csp instead of canard
add_definitions(-DUSE_CSP_OVER_CANARD=1)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

option(ENABLE_XMODEM "Enable XMODEM support" OFF)
if(ENABLE_XMODEM)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        ENABLE_XMODEM
    )
endif()

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)
add_subdirectory(cmake/developercore)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

include(cmake/libcsp2.cmake)

libcsp2_build(
    TARGET ${CMAKE_PROJECT_NAME}
    LIBCSP_DIR ${CMAKE_SOURCE_DIR}/Dev/libcspv2
    INCLUDES
        ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
        ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
        ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
        ${CMAKE_SOURCE_DIR}/Core/Inc
)

if(MY_ACTIVE_PRESET STREQUAL "Linux_Renode")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        RENODE_BUILD
    )
endif()

include(cmake/libcanard_dsdl.cmake)

libcanard_generate_dsdl(
    TARGET ${CMAKE_PROJECT_NAME}
    DSDL_ROOT ${CMAKE_SOURCE_DIR}/Dev/nodes
    GENERATED_DIR ${CMAKE_SOURCE_DIR}/Dev/libcanard_auto_generated
    COMPILER ${CMAKE_SOURCE_DIR}/Dev/libcanard/dsdl_compiler/libcanard_dsdlc
    INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/Dev/libcanard/dsdl_compiler/pyuavcan/uavcan
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    developercore
)
