# stm32l451-master


## How To Start

I tried to put every possible commands in "environment/aliases.sh" file. So, pleae check there firs to find out which command suits to you.

Generally, if you want to use Docker with this project, you should;

```
source environment/aliases.sh
build_docker
build_fw_in_docker
```

If you want to use your environment directly, then;

```
source environment/aliases.sh
build_fw_linux
```

If you are a Windows user, use the below command (note that, I am now a Windows user so this script may not work time to time)

```
.\environment\build.bat
```

## How To Test (Renode)

First, you need to build your project with RENODE flag enabled than you should start renode machine.

```
source environment/aliases.sh

build_fw_linux_renode // for linux
build_fw_in_docker_renode // for docker

start_renode_machine_linux // for linux
start_renode_machine_in_docker // for docker
```

To use GDB with Renode

```
arm-none-eabi-gdb build/stm32l451-master.elf
target remote localhost:333
bt full
```
