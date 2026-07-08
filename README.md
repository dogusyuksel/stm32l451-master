# STM32L451 Master

<p align="center">

![STM32](https://img.shields.io/badge/MCU-STM32L451-blue)
![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green)
![Renode](https://img.shields.io/badge/Simulation-Renode-orange)
![Docker](https://img.shields.io/badge/Docker-Supported-2496ED)
![Linux](https://img.shields.io/badge/Linux-Supported-success)
![CMake](https://img.shields.io/badge/CMake-3.21+-red)
![Libcanard](https://img.shields.io/badge/Libcanard-UAVCAN%20v1-orange)
![libcsp](https://img.shields.io/badge/libcsp-CSP2-blueviolet)

Embedded firmware development platform with **Docker**, **Renode**, **SocketCAN**, **GDB** and **native Linux** support.

</p>

---

# ✨ Features

* STM32L451 firmware
* FreeRTOS
* Docker development environment
* Native Linux build
* Renode simulation
* Remote GDB debugging
* SocketCAN integration
* CSP
* Legacy Libcanard (UAVCAN)
* CMake build system
* Custom Renode peripheral models

---

# 📁 Project Structure

```text
.
├── build/
├── cmake/
├── Core/
├── Dev/
├── Drivers/
├── environment/
├── Middlewares/
├── renode/
├── CMakeLists.txt
├── CMakePresets.json
├── startup_stm32l451xx.s
├── stm32l451-master.ioc
├── STM32L451XX_FLASH.ld
├── VERSION
└── README.md
```

---

# 🏗 Build

Almost every commonly used command has already been defined inside

```bash
environment/aliases.sh
```

Load them once

```bash
source environment/aliases.sh
```

---

## 🐳 Build Docker Image

```bash
build_docker
```

---

## 💻 Build Firmware

### Native Linux

```bash
build_fw_linux
```

### Docker

```bash
build_fw_in_docker
```

---

## 🧪 Build Firmware for Renode

### Linux

```bash
build_fw_linux_renode
```

### Docker

```bash
build_fw_in_docker_renode
```

---

# 🚀 Available Aliases

| Alias                            | Description                  |
| -------------------------------- | ---------------------------- |
| `build_docker`                   | Build Docker image           |
| `run_docker`                     | Interactive Docker shell     |
| `run_docker_as_root`             | Docker shell as root         |
| `build_fw_linux`                 | Native firmware build        |
| `build_fw_linux_renode`          | Native Renode build          |
| `build_fw_in_docker`             | Docker firmware build        |
| `build_fw_in_docker_renode`      | Docker Renode firmware build |
| `start_renode_machine_linux`     | Launch Renode (Linux)        |
| `start_renode_machine_in_docker` | Launch Renode (Docker)       |

---

# 🖥 Running Renode

After building with the Renode preset:

Linux

```bash
start_renode_machine_linux
```

Docker

```bash
start_renode_machine_in_docker
```

The startup script automatically

* Generates platform files
* Loads the ELF
* Starts the virtual STM32
* Opens UART analyzer
* Starts GDB server
* Connects SocketCAN
* Simulates USER button press

---

# 🧩 Simulated Hardware

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

# 🔧 Custom Renode Models

The project extends Renode using custom peripheral models.

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

These models provide additional logging, simulation support and improved STM32 peripheral behaviour.

---

# 🛠 GDB Debugging

Renode automatically starts a GDB server on

```text
localhost:3333
```

Connect using

```bash
arm-none-eabi-gdb build/stm32l451-master.elf

target remote localhost:3333

bt full
```

---

# 💬 UART

UART1 is exposed as a TCP terminal.

```text
localhost:1234
```

Example

```bash
telnet localhost 1234
```

---

# 🚗 SocketCAN

Create a virtual CAN interface

```bash
sudo ip link add dev can0 type vcan
sudo ip link set up can0
```

Receive CAN frames

```bash
candump can0
```

Transmit a frame

```bash
cansend can0 123#1122334455667788
```

The Renode CAN model is directly connected to the Linux SocketCAN interface, allowing interaction with external CAN tools exactly as if a physical CAN transceiver were attached.

---

# 🧱 Architecture

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

---

# 📦 External Libraries

This project makes use of several external libraries.

Some of them contain project specific modifications.

Search the repository for

```text
CUSTOM_CHANGES
```

to quickly locate every modification.

---

## CAN Selection

Legacy Libcanard can be configured to use **CAN2** by defining

```c
CANARD_STM32_USE_CAN2
```

---

# 📚 References

Legacy UAVCAN Specification

https://legacy.uavcan.org/Specification/1._Introduction/

---

# 📄 License

This project itself is released without a dedicated license.

Please also review the licenses of all third-party libraries.

* mpaland/printf        : https://github.com/mpaland/printf/blob/master/LICENSE
* CmBacktrace           : https://github.com/armink/CmBacktrace/blob/master/LICENSE
* OpenCyphal Libcanard  : https://github.com/OpenCyphal/libcanard/blob/master/LICENSE
* libcsp                : https://github.com/libcsp/libcsp/blob/develop/LICENSE

---

<p align="center">

Using STM32 • FreeRTOS • Renode • Docker • SocketCAN

</p>
