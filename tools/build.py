#!/usr/bin/env python


import os


SCRIPT_DIR = os.path.split(os.path.realpath(__file__))[0]

QEMU_VER="qemu-2.10.0"
WINE_VER=""
GETTEXT_VER="gettext-0.19.8.1"
GLIB_VER="glib-2.54.0"
LIBFFI_VER="libffi-3.2.1"
LIBICONV_VER="libiconv-1.15"
LIBPNG_VER="libpng-1.6.32"
PCRE_VER="pcre-8.41"


def buildQemu():
    os.chdir(os.path.join(SCRIPT_DIR, "../src", QEMU_VER))
    #copy build script to source directory
    ret = os.system("cp -fv ../../build/build-scripts/qemu-android-build.sh ./") >> 8
    if ret != 0:
        print "cannot copy qemu build script"
        print "\nfailed!"
        exit(-1)
    #build
    ret = os.system("./qemu-android-build.sh")
    if ret != 0:
        print "build qemu failed"
        print "\nfailed!"
        exit(-1) 
    os.chdir(SCRIPT_DIR)


def main():
    origDir = os.getcwd()
    os.chdir(SCRIPT_DIR)
    
    buildQemu()
    print "\nsuccess"
    
    os.chdir(origDir)
    

if __name__ == "__main__":
    main()
