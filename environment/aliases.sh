#!/usr/bin/env bash

# ==========================
# Environment Variables
# ==========================
export DOCKER_WORKINGDIR="/workspace"
export SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export DOCKER_IMAGE_NAME="stm32l4-master"

# ==========================
# Aliases
# ==========================
alias build_docker="docker build --network=host -t \"$DOCKER_IMAGE_NAME\" \"$SCRIPT_DIR\""
alias run_docker="docker run --rm -it --net=host -v \"$SCRIPT_DIR\"/..:\"$DOCKER_WORKINGDIR\" -v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro -u $(id -u ${USER}):$(id -g ${USER}) --entrypoint=/bin/bash \"$DOCKER_IMAGE_NAME\""
alias run_docker_as_root="docker run --rm -it --net=host -v \"$SCRIPT_DIR\"/..:\"$DOCKER_WORKINGDIR\" --entrypoint=/bin/bash \"$DOCKER_IMAGE_NAME\""
alias build_fw_in_docker="docker run --rm -t --net=host -v \"$SCRIPT_DIR\"/..:\"$DOCKER_WORKINGDIR\" --entrypoint=/bin/bash \"$DOCKER_IMAGE_NAME\" -c \"cd $DOCKER_WORKINGDIR && rm -rf build && mkdir build && cd build && cmake --preset=Linux .. && make && cd -\""
alias build_fw_in_docker_renode="docker run --rm -t --net=host -v \"$SCRIPT_DIR\"/..:\"$DOCKER_WORKINGDIR\" --entrypoint=/bin/bash \"$DOCKER_IMAGE_NAME\" -c \"cd $DOCKER_WORKINGDIR && rm -rf build && mkdir build && cd build && cmake --preset=Linux_Renode .. && make && cd -\""

alias build_fw_linux="cd $SCRIPT_DIR/.. && rm -rf build && mkdir build && cd build && cmake --preset=Linux .. && make && cd -"
alias build_fw_linux_renode="cd $SCRIPT_DIR/.. && rm -rf build && mkdir build && cd build && cmake --preset=Linux_Renode .. && make && cd -"

alias start_renode_machine_in_docker="docker run --rm -t --net=host -v \"$SCRIPT_DIR\"/..:\"$DOCKER_WORKINGDIR\" --entrypoint=/bin/bash \"$DOCKER_IMAGE_NAME\" -c \"cd $DOCKER_WORKINGDIR/renode && python3 generate.py && renode -e 'start @test.resc' && cd -\""
alias start_renode_machine_linux="cd $SCRIPT_DIR/../renode && python3 generate.py && renode -e 'start @test.resc' && cd -"

alias build_libcanard_tool_in_docker="docker run --rm -t --net=host -v \"$SCRIPT_DIR\"/..:\"$DOCKER_WORKINGDIR\" --entrypoint=/bin/bash \"$DOCKER_IMAGE_NAME\" -c \"cd $DOCKER_WORKINGDIR/tools/libcanard_listener && rm -rf build && mkdir build && cd build && cmake .. && make && cd -\""
alias build_libcanard_tool_in_linux="cd $SCRIPT_DIR/../tools/libcanard_listener && rm -rf build && mkdir build && cd build && cmake .. && make && cd -"

# ==========================
# Welcome Message
# ==========================
echo "Custom aliases loaded."
