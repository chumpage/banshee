#!/bin/bash

set -e

source `~/config-dir.sh`/bash-util
prepend-android-sdk

cd `dirname $0`
ndk=`echo $* | args.py ndk setting=android-ndk-lab126 required=1`
android_src=`echo $* | args.py fire setting=fire required=1`
export PATH=$PATH:$ndk/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/bin/
android_product=`echo $* | args.py android-product default=blaze`
android_target=`echo $* | args.py android-target default=android-10`
out_dir=out

cc_prog=arm-linux-androideabi-gcc
cpp_prog=arm-linux-androideabi-g++
link_prog=arm-linux-androideabi-g++
c_defs="-DANDROID -DHAVE_PTHREADS -DEGL_EGLEXT_PROTOTYPES -DGL_GLEXT_PROTOTYPES"
inc_dirs=
inc_dirs+=" -I$ndk/sources/cxx-stl/stlport/stlport"
inc_dirs+=" -I$ndk/sources/cxx-stl/gabi++/include"
inc_dirs+=" -I$android_src/frameworks/base/include"
inc_dirs+=" -I$android_src/hardware/libhardware/include"
inc_dirs+=" -I$android_src/system/core/include"
libs=$android_src/out/target/product/$android_product/symbols/system/lib/libui.so
libs+=" $android_src/out/target/product/$android_product/symbols/system/lib/libutils.so"
libs+=" $android_src/out/target/product/$android_product/symbols/system/lib/libcutils.so"
libs+=" $ndk/sources/cxx-stl/stlport/libs/armeabi-v7a/libstlport_static.a"
libs+=" -llog -landroid -lEGL -lGLESv2"
compiler_opts="-fno-rtti -fno-exceptions -Wno-multichar"

mkdir -p $out_dir

# cmd="$cpp_prog -g host.cpp common.cpp -o $out_dir/host $c_defs $compiler_opts $inc_dirs $libs"
# echo $cmd
# $cmd

cmd="$cpp_prog -g renderer.cpp common.cpp -o $out_dir/renderer $c_defs $compiler_opts $inc_dirs $libs"
echo $cmd
$cmd

echo Deploying to device
# adb push $out_dir/host /data/local/banshee/host
adb push $out_dir/renderer /data/local/banshee/renderer

cd host
$ndk/ndk-build APP_OPTIM=debug ANDROID_SRC=$android_src ANDROID_PRODUCT=$android_product
if [ ! -f build.xml ]; then
    android update project --target $android_target --path . --name BansheeHost
fi
# Clean before building, to work around this bug:
#   http://code.google.com/p/android/issues/detail?id=22948
# If your Android SDK isn't affected by that bug, don't do the clean.
ant -q debug clean
ant -q debug
adb -d install -r bin/BansheeHost-debug.apk
