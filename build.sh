#!/bin/bash

set -e

source `~/config-dir.sh`/bash-util
prepend-android-sdk

cd `dirname $0`
ndk=`echo $* | args.py ndk setting=android-ndk-lab126 required=1`
android_src=`echo $* | args.py fire setting=fire required=1`
export PATH=$PATH:$ndk/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/bin/
android_product=`echo $* | args.py android-product default=blaze`

cc_prog=arm-linux-androideabi-gcc
cpp_prog=arm-linux-androideabi-g++
link_prog=arm-linux-androideabi-g++
c_defs="-DANDROID"
inc_dirs=
inc_dirs+=" -I$ndk/sources/cxx-stl/stlport/stlport"
inc_dirs+=" -I$ndk/sources/cxx-stl/gabi++/include"
inc_dirs+=" -I$ndk/platforms/android-9/arch-arm/usr/include"
inc_dirs+=" -I$android_src/frameworks/base/include"
inc_dirs+=" -I$android_src/hardware/libhardware/include"
inc_dirs+=" -I$android_src/system/core/include"
libs=$android_src/out/target/product/$android_product/symbols/system/lib/libui.so
libs+=" $ndk/sources/cxx-stl/stlport/libs/armeabi-v7a/libstlport_static.a"
compiler_opts="-fno-rtti -fno-exceptions"

echo $cpp_prog -g host.cpp -o host $compiler_opts $c_defs $inc_dirs $libs
$cpp_prog -g host.cpp -o host $compiler_opts $c_defs $inc_dirs $libs

echo $cpp_prog -g renderer.cpp -o renderer $compiler_opts $c_defs $inc_dirs $libs
$cpp_prog -g renderer.cpp -o renderer $compiler_opts $c_defs $inc_dirs $libs

echo Copying to device
adb push host /data/local/banshee
adb push renderer /data/local/banshee
