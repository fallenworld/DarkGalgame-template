#!/bin/bash

ANDROID_BUILD=#{*ANDROID_BUILD*}#
API_VERSION=#{*API_VERSION*}#
PATH=$ANDROID_BUILD/bin:$PATH
SYSROOT=$ANDROID_BUILD/sysroot
HOST=arm-linux-androideabi
CFLAGS="-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -D__ANDROID_API__=$API_VERSION"
CXXFLAGS="-std=c++11"
LIBDIRS="-L$ANDROID_BUILD/arm-linux-androideabi/lib"
LDFLAGS="-march=armv7-a -Wl,--fix-cortex-a8 $LIBDIRS"
CONFLAGS="--prefix=${SYSROOT}/usr --host=$HOST"
PKG_CONFIG_PATH="$SYSROOT/usr/lib/pkgconfig"

#configure
PKG_CONFIG_PATH=$PKG_CONFIG_PATH CFLAGS="$CFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" ./configure $CONFLAGS &&

#make & install
make && make install
