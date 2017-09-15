#!/bin/bash

ANDROID_BUILD=#{*ANDROID_BUILD*}#
API_VERSION=#{*API_VERSION*}#
PATH=$ANDROID_BUILD/bin:$PATH
SYSROOT=$ANDROID_BUILD/sysroot
HOST=arm-linux-androideabi
CC=$HOST-gcc
CFLAGS="-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -D__ANDROID_API__=$API_VERSION -fPIE -pie -fPIC"
CXXFLAGS="-std=c++11"
LIBDIRS="-L$ANDROID_BUILD/arm-linux-androideabi/lib"
LDFLAGS="-march=armv7-a -Wl,--fix-cortex-a8,-soname,libqemu.so $LIBDIRS -fPIE -pie -fPIC -shared"
CONFLAGS="--prefix=$SYSROOT/usr --cross-prefix=$HOST- --host-cc=$CC --target-list=i386-linux-user --cpu=arm --disable-system --disable-bsd-user --disable-tools --disable-zlib-test --disable-guest-agent --disable-nettle --enable-debug"
PKG_CONFIG_PATH="$SYSROOT/usr/lib/pkgconfig"

ln -s /usr/bin/pkg-config $ANDROID_BUILD/bin/arm-linux-androideabi-pkg-config

#configure
PKG_CONFIG_PATH=$PKG_CONFIG_PATH CFLAGS="$CFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" ./configure $CONFLAGS &&

#make & install
make -j6 && make install
