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

# =========================
# CSP CONFIGURATION
# =========================

set(LIBCSP_DIR ${CMAKE_SOURCE_DIR}/Dev/libcspv2)

# be careful about below settings, should match with other end
add_custom_target(libcsp_build ALL
    COMMAND ./waf configure
        --with-os=freertos 
        --toolchain=arm-none-eabi- 
        --with-max-bind-port 61 
        --enable-promisc 
        --with-rtable-size 32 
        --enable-rtable 
        --includes 
            ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2,${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include,${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F,${CMAKE_SOURCE_DIR}/Core/Inc
    COMMAND ./waf build
    WORKING_DIRECTORY ${LIBCSP_DIR}
    COMMENT "Building libcsp2 with waf..."
)
add_dependencies(${CMAKE_PROJECT_NAME} libcsp_build)

# =========================

if(MY_ACTIVE_PRESET STREQUAL "Linux_Renode")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        RENODE_BUILD
    )
endif()

# CAN/DSDL generation
set(GENERATED_DIR ${CMAKE_SOURCE_DIR}/Dev/libcanard_auto_generated)

set(GENERATED_SOURCES
    ${GENERATED_DIR}/nodes/node1/node1_StatusShare.c
    ${GENERATED_DIR}/nodes/node2/node2_NodePing.c
    ${GENERATED_DIR}/nodes/node2/node2_NodePong.c
)

add_custom_command(
    OUTPUT ${GENERATED_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${GENERATED_DIR}
    COMMAND ${CMAKE_SOURCE_DIR}/Dev/libcanard/dsdl_compiler/libcanard_dsdlc
            ${CMAKE_SOURCE_DIR}/Dev/nodes
            --incdir ${CMAKE_SOURCE_DIR}/Dev/libcanard/dsdl_compiler/pyuavcan/uavcan
            --outdir ${GENERATED_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating DSDL sources"
)

add_custom_target(generate_dsdl DEPENDS ${GENERATED_SOURCES})
add_dependencies(${CMAKE_PROJECT_NAME} generate_dsdl)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${GENERATED_SOURCES})

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