#!/bin/bash

current_path=$PWD

cd $WORKSPACE_PATH/tools/libcsp_listener/rust-server/libsocketcan
./autogen.sh
./configure
make
make install
cp -rf ./src/.libs/libsocketcan.a $current_path/../artifacts
cd -
