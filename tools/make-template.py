#!/usr/bin/env python

import os
import sys

TEMPLATE_DIR_NAME = "DarkGalgame"
INSTANCE_DIR_NAME = "DarkGalgame-dev"

EXTERNAL_LIB_SRC_DIR = "build/external/src"
EXTERNAL_LIB_PATCH_FILE = "build/external/external.patch"
EXTERNAL_LIB_DIFF_FILES = [
    EXTERNAL_LIB_SRC_DIR + "/gettext-0.19.8.1/gettext-tools/libgrep/nl_langinfo.c",
    EXTERNAL_LIB_SRC_DIR + "/glib-2.54.0/android.cache"]


def makePatch(fileToPatchList, patchFileName):
    #create patch file
    origDir = os.getcwd()
    os.chdir("../..")
    patchFile = os.path.join(TEMPLATE_DIR_NAME, patchFileName)
    patchFd = open(patchFile, "w+")
    patchFd.truncate()
    patchFd.close()
    
    #make patch
    for fileToPatch in fileToPatchList:
        newFile = os.path.join(INSTANCE_DIR_NAME, fileToPatch)
        oldFile = os.path.join(TEMPLATE_DIR_NAME, fileToPatch)
        os.system("diff -urN %s %s >> %s" % (oldFile, newFile, patchFile))


def main():
    #make patch for external libraries
    os.system("../build/external/external.sh clean")
    makePatch(EXTERNAL_LIB_DIFF_FILES, EXTERNAL_LIB_PATCH_FILE)
  
  
if __name__ == "__main__":
    main()
    
