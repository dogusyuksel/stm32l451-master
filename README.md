# STM32L451 Master

<p align="center">

![STM32](https://img.shields.io/badge/MCU-STM32L451-blue)
![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green)
![Renode](https://img.shields.io/badge/Simulation-Renode-orange)
![Docker](https://img.shields.io/badge/Docker-Supported-2496ED)
![Linux](https://img.shields.io/badge/Linux-Supported-success)
![CMake](https://img.shields.io/badge/CMake-3.22+-red)
![libcsp](https://img.shields.io/badge/libcsp-CSP%20v2-blueviolet)
![Libcanard](https://img.shields.io/badge/Libcanard-Legacy%20UAVCAN-orange)
![License](https://img.shields.io/badge/License-MIT-yellow)

STM32L451 firmware development project with **FreeRTOS**, **CMake**,
**Docker**, **Renode**, **SocketCAN**, **GDB**, **libcsp**, and legacy
**libcanard/UAVCAN** support.

</p>

---

## Overview

This repository contains an STM32L451 FreeRTOS firmware project. It combines
STM32CubeMX-generated HAL/RTOS code, project-specific application logic,
Renode simulation assets, SocketCAN/GDB integration, and CAN communication
examples based on libcsp v2 and legacy libcanard.

The project is intended for two main workflows:

- Building firmware for a real STM32L451 target with `arm-none-eabi`.
- Running the same firmware in Renode with virtual UART, CAN, SPI, I2C, RNG,
  GPIO, SocketCAN, telnet, and GDB support.

## Features

- STM32L451 Cortex-M4 firmware
- FreeRTOS with CMSIS-RTOS v2
- CMake preset-based native Linux and Docker build flow
- Renode virtual board support
- Custom Renode peripheral models
- UART1 TCP/telnet output
- GDB server for firmware debugging
- SocketCAN bridge for CAN traffic
- libcsp v2 over CAN
- Legacy libcanard/UAVCAN example handlers
- DSDL source generation for libcanard messages
- CmBacktrace fault/backtrace support
- Lightweight UART logging layer

## Simulated Hardware

The current virtual board contains the following peripherals.

| Peripheral         | Status |
| ------------------ | :----: |
| GPIO Button        |    ✅   |
| User LED           |    ✅   |
| UART1              |    ✅   |
| CAN1               |    ✅   |
| SocketCAN Bridge   |    ✅   |
| SPI1               |    ✅   |
| MX25R Flash        |    ✅   |
| I2C1               |    ✅   |
| BMA180             |    ✅   |
| STM32 RNG          |    ✅   |
| RCC                |    ✅   |
| Chip Select Logger |    ✅   |

---

## Project Structure

```text
.
|-- Core/                    STM32CubeMX-generated application skeleton
|-- Drivers/                 STM32 HAL, CMSIS, and device support files
|-- Middlewares/             FreeRTOS sources
|-- Dev/                     Project code and embedded third-party libraries
|   |-- cm_backtrace/        Fault and backtrace library
|   |-- libcanard/           Legacy UAVCAN/libcanard sources
|   |-- libcspv2/            libcsp v2 sources
|   |-- logging/             UART logging and embedded printf
|   `-- nodes/               DSDL/UAVCAN message definitions
|-- cmake/                   STM32CubeMX and project CMake fragments
|-- environment/             Dockerfile and shell aliases
|-- renode/                  Renode scripts, platform files, and C# models
|-- tools/                   Host-side CAN/CSP helper tools
|-- CMakeLists.txt           Top-level build definition
|-- CMakePresets.json        Linux and Renode presets
|-- stm32l451-master.ioc     STM32CubeMX project file
`-- STM32L451XX_FLASH.ld     Linker script
```

## Requirements

For native builds:

- CMake 3.22 or newer
- `arm-none-eabi-gcc`
- `make` or Ninja, depending on the selected environment
- Python 3
- Renode, if simulation is needed
- Linux SocketCAN tools, if CAN testing is needed (`ip`, `candump`, `cansend`)

For the Docker workflow, Docker is enough; the firmware toolchain is installed
inside the container.

## Quick Start

Load the helper aliases:

```bash
source environment/aliases.sh
```

Build the Docker image:

```bash
build_docker
```

Build firmware natively:

```bash
build_fw_linux
```

Build firmware inside Docker:

```bash
build_fw_in_docker
```

Build firmware for Renode:

```bash
build_fw_linux_renode
```

Start the Renode simulation:

```bash
start_renode_machine_linux
```

Start the Renode simulation inside Docker:

```bash
start_renode_machine_in_docker
```

## Available Aliases

| Alias | Description |
| --- | --- |
| `build_docker` | Build the Docker development image |
| `run_docker` | Open an interactive Docker shell as the current user |
| `run_docker_as_root` | Open an interactive Docker shell as root |
| `build_fw_linux` | Build firmware natively on Linux |
| `build_fw_linux_renode` | Build Renode firmware natively on Linux |
| `build_fw_in_docker` | Build firmware inside Docker |
| `build_fw_in_docker_renode` | Build Renode firmware inside Docker |
| `start_renode_machine_linux` | Start the Renode machine natively |
| `start_renode_machine_in_docker` | Start the Renode machine inside Docker |
| `build_libcanard_tool_in_linux` | Build the libcanard host listener natively |
| `build_libcanard_tool_in_docker` | Build the libcanard host listener in Docker |
| `build_libcsp_tool_in_linux` | Build the libcsp host listener natively |
| `build_libccsp_tool_in_docker` | Build the libcsp host listener in Docker |

## Firmware Architecture

```text
STM32CubeMX HAL/FreeRTOS
        |
        +-- Dev/bsp.c
        |     UART logging, GPIO interrupt handling, SPI flash checks,
        |     I2C sensor probing, and RNG test logging
        |
        +-- Dev/libcsp2_handlers.c
        |     CSP interface, router task, and server task over CAN
        |
        +-- Dev/libcanard_handlers.c
        |     Legacy UAVCAN/libcanard ping-pong and request/response example
        |
        +-- Dev/freertos_over.c
        |     FreeRTOS task stack adapter for CmBacktrace
        |
        `-- Dev/logging
              Lightweight UART-backed logging
```

The default build enables `USE_CSP_OVER_CANARD=1`, so CAN traffic uses the CSP
path by default. Legacy libcanard support is still kept in the repository and
can be used by changing that compile-time selection.

## Renode Simulation

The Renode workflow uses `renode/generate.py` to generate platform files and
starts the board from the Renode startup script. The simulation:

- Loads the firmware ELF
- Starts the virtual STM32 platform
- Opens UART analyzer/TCP terminal support
- Starts a GDB server
- Connects CAN traffic to SocketCAN
- Models the user button, LED, CAN hub, I2C/SPI logging, RNG, and RCC behavior

Custom Renode models:

```text
BUTTON.cs
CANHUB.cs
CS.cs
LOGGER_I2C.cs
LOGGER_SPI.cs
STM32_RNG.cs
STM32L4_RCC.cs
STML4_I2C.cs
USERLED.cs
```

## Architecture

```text
                    +-----------------------------+
                    |      STM32L451 Firmware     |
                    +-------------+---------------+
                                  |
               +------------------+------------------+
               |                                     |
        Physical Hardware                    Renode Simulation
               |                                     |
      STM32L451 MCU Model                   Custom Peripheral Models
               |                                     |
      UART / SPI / I2C / CAN           UART / SPI / I2C / CAN
               |                                     |
               +------------------+------------------+
                                  |
                        SocketCAN / GDB / Telnet
                                  |
                       Linux Host Development
```

## GDB Debugging

Renode starts a GDB server at:

```text
localhost:3333
```

Connect with:

```bash
arm-none-eabi-gdb build/stm32l451-master.elf
target remote localhost:3333
bt full
```

## UART

UART1 is exposed as a TCP terminal:

```text
localhost:1234
```

Example:

```bash
telnet localhost 1234
```

Sending `trigger_fault` over UART intentionally triggers a fault so the
CmBacktrace path can be tested.

## SocketCAN

Create a virtual CAN interface:

```bash
sudo ip link add dev can0 type vcan
sudo ip link set up can0
```

Receive CAN frames:

```bash
candump can0
```

Transmit a CAN frame:

```bash
cansend can0 123#1122334455667788
```

The Renode CAN model is connected to SocketCAN, so Linux CAN tools can interact
with the simulation much like they would with a physical CAN transceiver.

## Third-Party Components

This repository vendors several external components:

- STM32 HAL/CMSIS and FreeRTOS: `Drivers/`, `Middlewares/`
- libcsp v2: `Dev/libcspv2`
- OpenCyphal libcanard: `Dev/libcanard`
- CmBacktrace: `Dev/cm_backtrace`
- mpaland/printf: `Dev/logging`
- libsocketcan: `tools/libcsp_listener/rust-server/libsocketcan`

Some third-party code contains project-specific modifications. Search for:

```bash
rg CUSTOM_CHANGES
```

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the third-party
license summary.

## Test Commands

```
printf '\x74\x72\x69\x67\x67\x65\x72\x5f\x66\x61\x75\x6c\x74' | nc localhost 1234  // test crash
printf '\x74\x72\x69\x67\x67\x65\x72\x5f\x77\x64\x74' | nc localhost 1234          // test wdt
```

## License

Project-specific code in this repository is released under the MIT license.
See [LICENSE](LICENSE).

Third-party components remain under their original licenses. Their license
files are kept in the relevant source directories and summarized in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

---

<p align="center">
Using STM32 - FreeRTOS - Renode - Docker - SocketCAN - libcsp - libcanard
</p>
