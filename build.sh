#!/bin/sh

export PATH=$PATH:/home/gxh/new_openbmc/openbmc/build/evb-ast2600/tmp/work/evb_ast2600-openbmc-linux-gnueabi/linux-aspeed/6.6.54+git/recipe-sysroot-native/usr/bin/arm-openbmc-linux-gnueabi/
CMAKE=$(which cmake)
rm -rf target
${CMAKE} -DCMAKE_TOOLCHAIN_FILE=$(pwd)/build/arch/arm.cmake -G Ninja -S $(pwd) -B $(pwd)/target
${CMAKE} --build $(pwd)/target --target virt_temp -j $(nproc)