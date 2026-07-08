#!/bin/bash

cd ../../Dev/libcspv2

./waf configure --with-os=posix --enable-examples --enable-can-socketcan --enable-if-zmqhub --with-driver-usart linux --with-max-bind-port 61 --enable-promisc --with-rtable-size 32 --enable-rtable --includes ../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2,../../Middlewares/Third_Party/FreeRTOS/Source/include,../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F,../../Core/Inc
./waf build

cp -rf build/examples/csp_server ../../tools/libcsp_listener/server.exe

cd -

