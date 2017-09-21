#!/usr/bin/env python


import os
import sys


SCRIPT_DIR = os.path.split(os.path.realpath(__file__))[0]

TEMPLATE_DIR_NAME = "DarkGalgame"
INSTANCE_DIR_NAME = "DarkGalgame-dev"

#{*LIBS_VER*}#


EXTERNAL_SRC_DIR = "build/external/src"
EXTERNAL_PATCH_DIR = "build/external/patches"
MAIN_SRC_DIR = "src"
MAIN_PATCHES_DIR = "src/patches"

GETTEXT_SRC_DIR =  os.path.join(EXTERNAL_SRC_DIR, GETTEXT_VER)
GETTEXT_PATCH_FILE = os.path.join(EXTERNAL_PATCH_DIR, "gettext.patch")
GETTEXT_DIFF_FILES = [ os.path.join(GETTEXT_SRC_DIR, "gettext-tools/libgrep/nl_langinfo.c") ]

GLIB_SRC_DIR = os.path.join(EXTERNAL_SRC_DIR, GLIB_VER)
GLIB_PATCH_FILE = os.path.join(EXTERNAL_PATCH_DIR, "glib.patch")
GLIB_DIFF_FILES = [ os.path.join(GLIB_SRC_DIR, "android.cache") ]

QEMU_SRC_DIR = os.path.join(MAIN_SRC_DIR, QEMU_VER)
QEMU_PATCH_FILE = os.path.join(MAIN_PATCHES_DIR, "qemu.patch")
QEMU_DIFF_FILES = [os.path.join(QEMU_SRC_DIR, "configure"), 
                   os.path.join(QEMU_SRC_DIR, "Makefile"),
                   os.path.join(QEMU_SRC_DIR, "linux-user/syscall.c")]


def createPatch(fileToPatchList, patchFileName):
    #create patch file
    os.chdir(os.path.join(SCRIPT_DIR, "../.."))
    patchFile = os.path.join(TEMPLATE_DIR_NAME, patchFileName)
    patchFd = open(patchFile, "w+")
    patchFd.truncate()
    patchFd.close()
    #make patch
    for fileToPatch in fileToPatchList:
        newFile = os.path.join(INSTANCE_DIR_NAME, fileToPatch)
        oldFile = os.path.join(TEMPLATE_DIR_NAME, fileToPatch)
        os.system("diff -urN %s %s >> %s" % (oldFile, newFile, patchFile))
    os.chdir(SCRIPT_DIR)


def createAllPatches():
    os.system(os.path.join(SCRIPT_DIR, "../build/external/external.sh clean"))
    createPatch(GETTEXT_DIFF_FILES, GETTEXT_PATCH_FILE)
    createPatch(GLIB_DIFF_FILES, GLIB_PATCH_FILE)
    createPatch(QEMU_DIFF_FILES, QEMU_PATCH_FILE)
    
    
def applyAllPatches():
    projDir = os.path.join(SCRIPT_DIR, "..")
    #patch gettext
    os.chdir(os.path.join(projDir, GETTEXT_SRC_DIR))
    ret = os.system("patch -p5 < ../../patches/gettext.patch") >> 8
    if ret != 0:
        print "failed to patch gettext"
        exit(-1)
    #patch glib
    os.chdir(os.path.join(projDir, GLIB_SRC_DIR))
    ret = os.system("patch -p5 < ../../patches/glib.patch") >> 8
    if ret != 0:
        print "failed to patch glib"
        exit(-1)
    #patch qemu
    os.chdir(os.path.join(projDir, QEMU_SRC_DIR))
    ret = os.system("patch -p3 < ../patches/qemu.patch") >> 8
    if ret != 0:
        print "failed to patch qemu"
        exit(-1)
    os.chdir(SCRIPT_DIR)


def main():
    origDir = os.getcwd()
    os.chdir(SCRIPT_DIR)
    
    #parse command
    usageStr = "usage: patch.py <create|apply>\n"
    if len(sys.argv) != 2 or (sys.argv[1] != "create" and sys.argv[1] != "apply"):
        print usageStr
        exit(-1)
        
    if sys.argv[1] == "create":
        createAllPatches()
    elif sys.argv[1] == "apply":
        applyAllPatches()
  
    os.chdir(origDir)
  
  
if __name__ == "__main__":
    main()
    
