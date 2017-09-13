#!/usr/bin/env python

import os
import sys


EXTERNAL_LIB_DIFF_DIR = "build/external/src"
EXTERNAL_LIB_PATCH_FILE = "build/external/external.patch"
EXTERNAL_LIB_DIFF_FILES = [
    "gettext-0.19.8.1/gettext-tools/libgrep/nl_langinfo.c",
    "glib-2.54.0/android.cache"]


def makePatch(templateDir, patchDir, fileToPatchList, patchFileName):
    newSrcDir = "../" + patchDir
    oldSrcDir = templateDir + "/" + patchDir
    
    #create patch file
    patchFile = templateDir + "/" + patchFileName
    tmpFd = open(patchFile, "w+")
    tmpFd.truncate()
    tmpFd.close()
    
    #make patch
    for fileToPatch in fileToPatchList:
        newFile = newSrcDir + "/" + fileToPatch
        oldFile = oldSrcDir + "/" + fileToPatch
        os.system("diff -urN %s %s >> %s" % (oldFile, newFile, patchFile))


def main():
    #check aruments
    if len(sys.argv) != 2:
        print "usage: make-template.py TEMPLATE_DIR"
        exit(-1)
    if not os.path.exists(sys.argv[1]):
        print "usage: make-template.py TEMPLATE_DIR"
        print "error: %s not exists" % (sys.argv[1])
        exit(-1)
    templateDir = sys.argv[1]
    
    #make patch for external libraries
    os.system("../build/external/clean.sh")
    makePatch(templateDir, EXTERNAL_LIB_DIFF_DIR, EXTERNAL_LIB_DIFF_FILES, EXTERNAL_LIB_PATCH_FILE)
  
  
if __name__ == "__main__":
    main()
    
