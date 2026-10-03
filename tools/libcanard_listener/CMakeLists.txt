cmake_minimum_required(VERSION 3.22)
project(libcanard_listener)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

include(${REPO_ROOT}/cmake/libcanard.cmake)
include(${REPO_ROOT}/cmake/libcanard_dsdl.cmake)

option(LIBCANARD_LISTENER_32BIT "Build libcanard_listener as a 32-bit executable" OFF)

# Compiler configuration - supporting only Clang and GCC
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=c99   -Wall -Wextra -Werror -pedantic -g -O0")

if(LIBCANARD_LISTENER_32BIT)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
endif()

# C warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdouble-promotion -Wswitch-enum -Wfloat-equal -Wundef")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion -Wtype-limits")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wsign-conversion -Wcast-align -Wmissing-declarations -Wno-conversion -Wno-float-equal")

# Expose internal API for unit testing
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DCANARD_INTERNAL=''")

add_executable(can_dumper.exe
    can_dumper.c
)

libcanard_add_to_target(
    TARGET can_dumper.exe
    LIBCANARD_DIR ${REPO_ROOT}/Dev/libcanard
    DRIVER socketcan
)

libcanard_generate_dsdl(
    TARGET can_dumper.exe
    DSDL_ROOT ${REPO_ROOT}/Dev/ussp
    GENERATED_DIR ${REPO_ROOT}/Dev/libcanard_auto_generated
    COMPILER ${REPO_ROOT}/Dev/libcanard/dsdl_compiler/libcanard_dsdlc
    INCLUDE_DIRS ${REPO_ROOT}/Dev/libcanard/dsdl_compiler/pyuavcan/uavcan
)

file(GLOB_RECURSE generated_libcanard_headers CONFIGURE_DEPENDS
    "${REPO_ROOT}/Dev/libcanard_auto_generated/*.h"
)

target_sources(can_dumper.exe PRIVATE
    ${generated_libcanard_headers}
)

target_include_directories(can_dumper.exe PRIVATE
    ${REPO_ROOT}/Dev/libcanard_auto_generated
)

if(NOT LIBCANARD_LISTENER_32BIT)
    target_compile_definitions(can_dumper.exe PRIVATE CANARD_MEM_BLOCK_SIZE=64U)
endif()
