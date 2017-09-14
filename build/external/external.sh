#!/bin/bash

SCRIPT_DIR=$(cd `dirname $0`; pwd)

case $1 in

build)
    cd $SCRIPT_DIR/../..
    patch -p1 < $SCRIPT_DIR/external.patch &&
    {
    cd $SCRIPT_DIR/src/libpng-1.6.32;
    cp -fv ../../build-scripts/libpng-android-build.sh ./;
    ./libpng-android-build.sh;
    } &&
    {
    cd ../libiconv-1.15;
    cp -fv ../../build-scripts/libiconv-android-build.sh ./;
    ./libiconv-android-build.sh;
    } &&
    {
    cd ../libffi-3.2.1;
    cp -fv ../../build-scripts/libffi-android-build.sh ./;
    ./libffi-android-build.sh;
    } &&
    {
    cd ../pcre-8.41;
    cp -fv ../../build-scripts/pcre-android-build.sh ./;
    ./pcre-android-build.sh;
    } &&
    {
    cd ../gettext-0.19.8.1;
    cp -fv ../../build-scripts/gettext-android-build.sh ./;
    ./gettext-android-build.sh;
    } &&
    {
    cd ../glib-2.54.0;
    cp -fv ../../build-scripts/glib-android-build.sh ./;
    ./glib-android-build.sh;
    }
;;

clean)
    cd $SCRIPT_DIR/src/libffi-3.2.1
    make clean
    cd ../libiconv-1.15
    make clean
    cd ../libpng-1.6.32
    make clean
    cd ../pcre-8.41
    make clean
    cd ../gettext-0.19.8.1
    make clean && make distclean
    cd ../glib-2.54.0
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

