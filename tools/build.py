#!/usr/bin/env python


import os
from optparse import OptionParser


SCRIPT_DIR = os.path.split(os.path.realpath(__file__))[0]
HOSTWINE_BUILD_CMD = "./configure --without-x --without-freetype && make -j6"

#{*LIBS_VER*}#


def buildExternal(clean):
    os.chdir(os.path.join(SCRIPT_DIR, "../build/external"))
    if clean:
        os.system("./external.sh clean")
    ret = os.system("./external.sh build") >> 8
    if ret != 0:
        print "build external libraries failed"
        print "\nfailed!"
        exit(-1)
    os.chdir(SCRIPT_DIR)
    

def buildQemu(make, clean):
    os.chdir(os.path.join(SCRIPT_DIR, "../src", QEMU_VER))
    if clean:
        os.system("make clean")
        os.system("make distclean")
    if make:
        ret = os.system("make -j6") >> 8
        if ret != 0:
            print "build qemu failed"
            print "\nfailed!"
            exit(-1)
    else:
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
    #copy libqemu.so to output directory
    os.system("cp -fv i386-linux-user/qemu-i386 ../../build/output/")
    os.chdir(SCRIPT_DIR)


def buildWine(make, clean):
    os.chdir(os.path.join(SCRIPT_DIR, "../src", WINE_VER))
    if clean:
        os.system("make clean")
        os.system("make distclean")
    if make:
        ret = os.system("make -j6") >> 8
        if ret != 0:
            print "build wine failed"
            print "\nfailed!"
            exit(-1)
    else:
        #copy build script to source directory
        ret = os.system("cp -fv ../../build/build-scripts/wine-android-build.sh ./") >> 8
        if ret != 0:
            print "cannot copy wine build script"
            print "\nfailed!"
            exit(-1)
        #build
        ret = os.system("./wine-android-build.sh")
        if ret != 0:
            print "build wine failed"
            print "\nfailed!"
            exit(-1) 
    os.chdir(SCRIPT_DIR)
    

def buildHostWine(make, clean):
    if not os.path.exists(os.path.join(SCRIPT_DIR, "../build/wine-host")):
        #copy wine source
        print "copying wine source to build/wine-host"
        wineSrcDir = os.path.join(SCRIPT_DIR, "../src", WINE_VER)
        wineHostDir = os.path.join(SCRIPT_DIR, "../build/wine-host")
        os.makedirs(wineHostDir)
        ret = os.system("cp -fr %s/* %s" % (wineSrcDir, wineHostDir)) >> 8
        if ret != 0:
            print "cannot copy wine source files to build/wine-host"
            print "\nfailed!"
            exit(-1)
    
    os.chdir(os.path.join(SCRIPT_DIR, "../build/wine-host"))
    if clean:
        os.system("make clean")
        os.system("make distclean")
    if make:
        ret = os.system("make -j6") >> 8
        if ret != 0:
            print "build host wine failed"
            print "\nfailed!"
            exit(-1)
    else:
        #configure & make
        ret = os.system(HOSTWINE_BUILD_CMD)
        if ret != 0:
            print "build host wine failed"
            print "\nfailed!"
            exit(-1) 
    os.chdir(SCRIPT_DIR)
    
    
def main():
    origDir = os.getcwd()
    os.chdir(SCRIPT_DIR)
    
    parser = OptionParser()
    parser.add_option("-t", "--target", dest="target", metavar="TARGET",
                      help="target of your build, available value: all, main, external, qemu, wine, wine-host") 
    parser.add_option("-m", "--make", dest="make", action="store_true", default=False,
                      help="do not run the android-build script, just make the project") 
    parser.add_option("-c", "--clean", dest="clean", action="store_true", default=False, 
                      help="clean the project")
    (options, args) = parser.parse_args()

    if options.target == "all":
        buildExternal(options.clean)
        buildQemu(options.make, options.clean)
        buildHostWine(options.make, options.clean)
        buildWine(options.make, options.clean)
    elif options.target == "main":
        buildQemu(options.make, options.clean)
        buildWine(options.make, options.clean)
    elif options.target == "external":
        buildExternal(options.clean)
    elif options.target == "qemu":
        buildQemu(options.make, options.clean)
    elif options.target == "wine":
        buildWine(options.make, options.clean)
    elif options.target == "wine-host":
        buildHostWine(options.make, options.clean)
    else:
        parser.error("invalid target name: %s" % options.target)
        
    print "\nsuccess!"
    
    os.chdir(origDir)
    

if __name__ == "__main__":
    main()
