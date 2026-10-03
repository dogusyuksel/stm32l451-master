# libcanard_listener ResetInfo Test Tool

This tool tests `ussp.generic.ResetInfo` request/response transfers with libcanard and SocketCAN.

## Build

From the repository root, use the existing build directory:

```bash
wsl bash -lc "cd /mnt/c/cygwin64/home/dodo-/stm32l451-master && cmake --build tools/libcanard_listener/build"
```

If you configure from scratch:

```bash
wsl bash -lc "cd /mnt/c/cygwin64/home/dodo-/stm32l451-master/tools/libcanard_listener && cmake -S . -B build && cmake --build build"
```

The CMake target regenerates DSDL files from `Dev/ussp`, compiles the generated `.c` files, and includes generated
headers from `Dev/libcanard_auto_generated`.

The listener builds as a native 64-bit host executable by default. This is required on WSL because raw CAN sockets may
not be available to 32-bit processes. If a 32-bit build is needed elsewhere, configure with:

```bash
cmake -S . -B build -DLIBCANARD_LISTENER_32BIT=ON
```

## Enable `vcan0` In WSL

`vcan0` is useful when you want to test the ResetInfo request/response flow without a physical USB-CAN adapter.
Run these commands inside WSL:

```bash
sudo modprobe can
sudo modprobe can_raw
sudo modprobe vcan

sudo ip link show vcan0 >/dev/null 2>&1 || sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up
ip -details link show vcan0
```

Quick SocketCAN smoke test:

```bash
candump -n 1 vcan0 &
sleep 0.2
cansend vcan0 123#DEADBEEF
```

Expected `candump` output:

```text
  vcan0  123   [4]  DE AD BE EF
```

`vcan0` is not persistent across WSL restarts, so rerun the setup commands after `wsl --shutdown` or a Windows reboot.
If `sudo` requires a password from this environment, the same module/interface commands can be run from Windows as
`wsl -u root ...`.

## Run Two Processes

Start the fake responder first. It uses node id `5` by default and waits for `ResetInfo` requests:

```bash
./tools/libcanard_listener/build/can_dumper.exe -i can0
```

Start the requester in another terminal. `-t` sends one `ResetInfo` request per second. In `-t` mode the default local node id is `6`, and the default destination node is `5`:

```bash
./tools/libcanard_listener/build/can_dumper.exe -i can0 -t
```

Enable verbose diagnostics on either side with `-v`:

```bash
./tools/libcanard_listener/build/can_dumper.exe -i can0 -v
./tools/libcanard_listener/build/can_dumper.exe -i can0 -t -v
```

Useful overrides:

```bash
./tools/libcanard_listener/build/can_dumper.exe -i can0 -n 10
./tools/libcanard_listener/build/can_dumper.exe -i can0 -t -n 11 -d 10
```

For WSL virtual CAN testing, use `vcan0` instead of `can0`:

```bash
./tools/libcanard_listener/build/can_dumper.exe -i vcan0 -v
./tools/libcanard_listener/build/can_dumper.exe -i vcan0 -t -v
```

## ResetInfo DSDL Notes

`ResetInfo` is intentionally stressful for the old vendored libcanard DSDL compiler because it uses dynamic arrays of
compound types, and those compound types contain their own dynamic arrays:

- `ResetInfo.causes[]` -> `ResetReason.name[]`
- `ResetInfo.watchdog.blocking_tasks[]` -> `TaskInfo.name[]`
- `ResetInfo.assert_info.state_at_reset.system_tasks[]` -> `TaskInfo.name[]`

This branch intentionally keeps `ResetInfo` in its original complex dynamic form. The active solution keeps the vendored
compiler backward-compatible for the two compatibility-sensitive decode paths by selecting the new behavior only while
this tool is decoding a `ResetInfo` transfer:

```text
ResetReason[<8] causes
TaskInfo[<15] blocking_tasks
TaskInfo[<=15] system_tasks
```

The key compiler-side issue is scratch-buffer reservation while decoding dynamic arrays of compound types. The decoder
must reserve the compound array storage first, then let each compound item allocate its nested dynamic arrays after that
storage. Without this, fields such as `ResetReason.name` and `TaskInfo.name` can overlap the parent compound array.

Relevant references:

- OpenCyphal forum: old libcanard DSDL compiler has multiple known bugs and was removed from upstream:
  https://forum.opencyphal.org/t/libcanard-v0-dsdl-compiler-uavcan-to-h/948
- OpenCyphal forum: example bug report around dynamic array length / TAO behavior in the old compiler:
  https://forum.opencyphal.org/t/i-found-a-bug-in-libcanard-dsdl-compiler/224
- DroneCAN DSDL spec: dynamic arrays with variable-length items are legal but complex:
  https://dronecan.github.io/Specification/3._Data_structure_description_language/#dynamic-arrays
- DroneCAN `dronecan_dsdlc`: newer generator used by the DroneCAN ecosystem:
  https://dronecan.github.io/Implementations/dronecan_dsdlc/

### Selective Compiler Patch

The branch patches the old vendored compiler template in:

- `Dev/libcanard/dsdl_compiler/libcanard_dsdl_compiler/__init__.py`
- `Dev/libcanard/dsdl_compiler/libcanard_dsdl_compiler/code_type_template.tmpl`

The earlier compiler fixes are applied directly:

- saturated bool fields are no longer clamped to `0`
- nested compound encoder/decoder return values are treated as absolute bit offsets
- dynamic arrays of compound types reserve parent storage before nested dynamic fields are decoded

Two compatibility-sensitive decode behaviors are runtime-gated:

- advancing `dyn_arr_buf` after decoding primitive dynamic-array items
- passing `0` instead of the parent `payload_len` to nested compound-field decoders

Generated code provides a weak default `canard_dsdl_resetinfo_fix_enabled = 0`. `can_dumper.c` provides the real symbol
and sets it only while decoding `ResetInfo` request/response transfers, so a shared type such as `TaskInfo` uses the
new path inside the `ResetInfo` decode call chain but keeps the old path when decoded by any other package in the same
executable.

## What The Fake Response Contains

The responder sends a detailed fake `ResetInfo.Response`.

Dynamic arrays are backed by static global storage in `can_dumper.c` before encoding:

- `fake_causes`: 3 `ResetReason` entries, each with a `ResetReason.name` buffer
- `fake_watchdog_tasks`: 3 watchdog `TaskInfo` entries, each with a `TaskInfo.name` buffer
- `fake_backtrace`: 5 backtrace addresses
- `fake_reset_tasks`: 4 reset-state `TaskInfo` entries, each with a `TaskInfo.name` buffer
- `fake_current_task`: current task string

The requester decodes the response and prints every response field, including list lengths, list entries, register values, booleans, task names, task states, stack high-water values, CPU percentages, memory state, and scheduler state.

## Verbose Diagnostics

`-v` prints debug logs to `stderr` without changing the transfer flow.

Verbose logs include:

- encoded request/response payload length and hex dump
- received CAN frame id, frame length, and frame data
- reconstructed received transfer payload
- libcanard transfer metadata: source node, transfer type, transfer id, priority, data type id, payload length, payload head/middle/tail pointers
- libcanard allocator capacity/current/peak block counts
- decode result bit count and decode scratch usage
- dynamic list length checks against generated DSDL limits
- dynamic compound array/string/backtrace pointer checks against the decode scratch buffer

The pointer diagnostics are useful for `ResetInfo`: if a decoded dynamic compound array, string, or backtrace pointer
does not point into the decode scratch buffer, `-v` calls that out explicitly.

## Expected Flow

Responder output includes:

```text
Received ResetInfo request from node 6: reset_reg=0xA5 payload_len=1
Sent fake ResetInfo response to node 6, payload_len=..., frames=...
```

Requester output includes:

```text
Sent ResetInfo request to node 5, payload_len=1, frames=...
Received ResetInfo response from node 5, payload_len=...
ResetInfo response detail:
  success=1
  causes.len=3
  ...
```
