#!/bin/bash


SCRIPT_DIR=$(cd `dirname $0`; pwd)

. $SCRIPT_DIR/../../LIBS_VER

ANDROID_BUILD=/home/fallenworld/dev/android/projects/DarkGalgame-dev/build/android
LIB_DIR=$ANDROID_BUILD/sysroot/usr/lib
JNI_LIBS=(
    libglib-2.0.so
    libgthread-2.0.so
    libiconv.so
    libintl.so
    libpcre.so
)


case $1 in

build)
    #build external libraries
    {
    cd $SCRIPT_DIR/src/$LIBPNG_VER;
    cp -fv ../../build-scripts/libpng-android-build.sh ./;
    ./libpng-android-build.sh;
    } &&
    {
    cd $SCRIPT_DIR/src/$LIBICONV_VER;
    cp -fv ../../build-scripts/libiconv-android-build.sh ./;
    ./libiconv-android-build.sh;
    } &&
    {
    cd $SCRIPT_DIR/src/$LIBFFI_VER;
    cp -fv ../../build-scripts/libffi-android-build.sh ./;
    ./libffi-android-build.sh;
    } &&
    {
    cd $SCRIPT_DIR/src/$PCRE_VER;
    cp -fv ../../build-scripts/pcre-android-build.sh ./;
    ./pcre-android-build.sh;
    } &&
    {
    cd $SCRIPT_DIR/src/$GETTEXT_VER;
    cp -fv ../../build-scripts/gettext-android-build.sh ./;
    ./gettext-android-build.sh;
    } &&
    {
    cd $SCRIPT_DIR/src/$GLIB_VER;
    cp -fv ../../build-scripts/glib-android-build.sh ./;
    ./glib-android-build.sh;
    } &&
    #copy .so files to output directory
    for sharedLib in ${JNI_LIBS[@]}
    do
        cp -fv $LIB_DIR/$sharedLib $SCRIPT_DIR/../output
    done
;;

clean)
    cd $SCRIPT_DIR/src/$LIBFFI_VER
    make clean
    cd $SCRIPT_DIR/src/$LIBICONV_VER
    make clean
    cd $SCRIPT_DIR/src/$LIBPNG_VER
    make clean
    cd $SCRIPT_DIR/src/$PCRE_VER
    make clean
    cd $SCRIPT_DIR/src/$GETTEXT_VER
    make clean && make distclean
    cd $SCRIPT_DIR/src/$GLIB_VER
    make clean
    echo -e "\
    glib_cv_long_long_format=ll\n\
    glib_cv_stack_grows=no\n\
    glib_cv_uscore=no\n\
    ac_cv_func_posix_getpwuid_r=no\n\
    ac_cv_func_posix_getgrgid_r=no\
    " > android.cache
;;

esac

