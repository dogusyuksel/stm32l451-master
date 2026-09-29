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

The CMake target regenerates DSDL files from `Dev/ussp`, compiles the generated `.c` files, and includes generated headers from `Dev/libcanard_auto_generated`.

## Run Two Processes

Start the fake responder first. It uses node id `5` by default and waits for `ResetInfo` requests:

```bash
./tools/libcanard_listener/build/can_dumper.exe -i can0
```

Start the requester in another terminal. `-t` sends one `ResetInfo` request per second. In `-t` mode the default local node id is `6`, and the default destination node is `5`:

```bash
./tools/libcanard_listener/build/can_dumper.exe -i can0 -t
```

Useful overrides:

```bash
./tools/libcanard_listener/build/can_dumper.exe -i can0 -n 10
./tools/libcanard_listener/build/can_dumper.exe -i can0 -t -n 11 -d 10
```

## What The Fake Response Contains

The responder sends a detailed fake `ResetInfo.Response`.

Dynamic arrays are backed by static global storage in `can_dumper.c`:

- `fake_causes`: 3 `ResetReason` entries
- `fake_watchdog_tasks`: 3 watchdog blocking `TaskInfo` entries
- `fake_backtrace`: 5 backtrace addresses
- `fake_reset_tasks`: 4 `state_at_reset.system_tasks` entries
- `fake_current_task`: current task string

The requester decodes the response and prints every response field, including list lengths, list entries, register values, booleans, task names, task states, stack high-water values, CPU percentages, memory state, and scheduler state.

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
