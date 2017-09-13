#!/bin/bash

SCRIPT_DIR=$(cd `dirname $0`; pwd)

cd $SCRIPT_DIR/src

cd gettext-0.19.8.1
make clean && make distclean
cd ..

cd glib-2.54.0
make clean
echo -e "\
    glib_cv_long_long_format=ll\n\
    glib_cv_stack_grows=no\n\
    glib_cv_uscore=no\n\
    ac_cv_func_posix_getpwuid_r=no\n\
    ac_cv_func_posix_getgrgid_r=no\
" > android.cache
cd ..

cd libffi-3.2.1
make clean
cd ..

cd libiconv-1.15
make clean
cd ..

cd libpng-1.6.32
make clean
cd ..

cd pcre-8.41
make clean
cd ..

