# Third-Party Notices

This project vendors several third-party components. They are not relicensed by
the project-level MIT license; each component remains under its own license.

| Component | Path | License |
| --- | --- | --- |
| STM32Cube HAL / CMSIS device support | `Drivers/` | STMicroelectronics/ARM licenses included in source tree |
| FreeRTOS | `Middlewares/Third_Party/FreeRTOS/Source` | MIT, see `Middlewares/Third_Party/FreeRTOS/Source/LICENSE` |
| libcsp v2 | `Dev/libcspv2` | MIT, see `Dev/libcspv2/LICENSE` |
| OpenCyphal libcanard | `Dev/libcanard` | MIT, see `Dev/libcanard/LICENSE` |
| CmBacktrace | `Dev/cm_backtrace` | MIT-style license in source headers |
| mpaland/printf | `Dev/logging` | MIT license in `printf.c` and `printf.h` headers |
| libsocketcan | `tools/libcsp_listener/rust-server/libsocketcan` | LGPL-2.1-or-later, see that directory's `LICENSE` |
| xmodem | `Dev/Src/xmodem` |  MIT license in `xmodem.c` and `xmodem.h` headers |

When redistributing binaries or source archives, keep the relevant third-party
copyright notices and license files with the distribution.
