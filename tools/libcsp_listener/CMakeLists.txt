cmake_minimum_required(VERSION 3.20)
project(libcsp_listener C)

find_package(Threads REQUIRED)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CSP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(CSP_USE_PROMISC ON CACHE BOOL "" FORCE)
set(CSP_USE_RTABLE ON CACHE BOOL "" FORCE)
set(CSP_PORT_MAX_BIND 61 CACHE STRING "" FORCE)
set(CSP_RTABLE_SIZE 32 CACHE STRING "" FORCE)

add_subdirectory(${REPO_ROOT}/Dev/libcspv2 libcspv2)

add_library(csp_listener_posix_helper OBJECT
    csp_posix_helper.c
)
target_include_directories(csp_listener_posix_helper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(csp_listener_posix_helper PRIVATE
    csp
    csp_common
)

add_library(csp_listener_socketcan OBJECT
    ${REPO_ROOT}/Dev/libcspv2/src/drivers/can/can_socketcan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rust-server/libsocketcan/src/libsocketcan.c
)
target_include_directories(csp_listener_socketcan PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/rust-server/libsocketcan/include
)
target_link_libraries(csp_listener_socketcan PRIVATE
    csp
    csp_common
    Threads::Threads
)
target_compile_options(csp_listener_socketcan PRIVATE
    -Wno-error=pointer-arith
)

add_executable(csp_listener_server
    csp_server.c
)
set_target_properties(csp_listener_server PROPERTIES
    OUTPUT_NAME server
    SUFFIX ".exe"
)
target_include_directories(csp_listener_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(csp_listener_server PRIVATE
    csp
    csp_common
    csp_listener_posix_helper
    Threads::Threads
)

add_executable(csp_listener_client
    csp_client.c
)
set_target_properties(csp_listener_client PROPERTIES
    OUTPUT_NAME client
    SUFFIX ".exe"
)
target_include_directories(csp_listener_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(csp_listener_client PRIVATE
    csp
    csp_common
    csp_listener_posix_helper
    Threads::Threads
)

add_executable(csp_listener_server_client
    csp_server_client.c
)
set_target_properties(csp_listener_server_client PROPERTIES
    OUTPUT_NAME server_client
    SUFFIX ".exe"
)
target_include_directories(csp_listener_server_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(csp_listener_server_client PRIVATE
    csp
    csp_common
    csp_listener_posix_helper
    Threads::Threads
)

add_executable(csp_listener_bridge_can2udp
    csp_bridge_can2udp.c
)
set_target_properties(csp_listener_bridge_can2udp PROPERTIES
    OUTPUT_NAME bridge_can2udp
    SUFFIX ".exe"
)
target_include_directories(csp_listener_bridge_can2udp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(csp_listener_bridge_can2udp PRIVATE
    csp
    csp_common
    csp_listener_socketcan
    Threads::Threads
)

add_executable(csp_listener_arch
    csp_arch.c
)
set_target_properties(csp_listener_arch PROPERTIES
    OUTPUT_NAME arch
    SUFFIX ".exe"
)
target_link_libraries(csp_listener_arch PRIVATE
    csp
    csp_common
)
