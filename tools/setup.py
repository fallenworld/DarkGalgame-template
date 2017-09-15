#!/usr/bin/env python


import os
import fileinput
from optparse import OptionParser


SCRIPT_DIR = os.path.split(os.path.realpath(__file__))[0]

ANDROID_BUILD_TOOLS_DIR = "build/android"
ANDROID_MAKE_TOOLS_SCRIPT = "build/tools/make_standalone_toolchain.py"

MAIN_BUILD_SCRIPTS_DIR = "build/build-scripts"
MAIN_BUILD_SCRIPTS = ["qemu-android-build.sh", 
                      "wine-android-build.sh"]

EXTERNAL_BUILD_SCRIPTS_DIR = "build/external/build-scripts"
EXTERNAL_BUILD_SCRIPTS = ["gettext-android-build.sh", 
                          "glib-android-build.sh", 
                          "libffi-android-build.sh", 
                          "libiconv-android-build.sh", 
                          "libpng-android-build.sh", 
                          "pcre-android-build.sh"]


def fileReplace(fileName, rawStr, replaceStr):
    targetFile = open(fileName, "r+")
    fileContent = targetFile.read()
    targetFile.seek(0)
    targetFile.write(fileContent.replace(rawStr, replaceStr))
    targetFile.truncate()
    targetFile.close()


def setupLibsVer(projDir, fileName):
    libsVerFile = open(os.path.join(projDir, "LIBS_VER"), "r+")
    libsVerStr = libsVerFile.read()
    libsVerFile.close()
    fileReplace(fileName, "#{*LIBS_VER*}#", libsVerStr)
    

def setupBuildScripts(projDir, scriptsDir, scriptList, apiVersion, androidBuildDir):
    for scriptFile in scriptList:
        fullFileName = os.path.join(projDir, scriptsDir, scriptFile)
        fileReplace(fullFileName, "#{*API_VERSION*}#", apiVersion)
        fileReplace(fullFileName, "#{*ANDROID_BUILD*}#", androidBuildDir)


def setupExternalScripts(projDir, apiVersion, androidBuildDir):
    setupBuildScripts(projDir, EXTERNAL_BUILD_SCRIPTS_DIR, EXTERNAL_BUILD_SCRIPTS, apiVersion, androidBuildDir)
    fileReplace(os.path.join(projDir, "build/external/external.sh"), "#{*ANDROID_BUILD*}#", androidBuildDir)
    
    
def setupMainScripts(projDir, apiVersion, androidBuildDir):
    setupLibsVer(projDir, os.path.join(projDir, "tools/patch.py"))
    setupLibsVer(projDir, os.path.join(projDir, "tools/build.py"))
    setupBuildScripts(projDir, MAIN_BUILD_SCRIPTS_DIR, MAIN_BUILD_SCRIPTS, apiVersion, androidBuildDir)


def main():  
    #enter py script file path
    os.chdir(os.path.split(os.path.realpath(__file__))[0])
    
    #parse command options
    parser = OptionParser()
    parser.add_option("-a", "--api", dest="api", help="target android API version", metavar="API") 
    parser.add_option("-n", "--ndk", dest="ndk", help="your android NDK directory", metavar="NDK_DIR") 
    parser.add_option("-o", "--output", dest="output", help="directory to output the project files", 
                      metavar="OUTPUT_DIR")
    (options, args) = parser.parse_args()
    #check arguments
    if options.api == None:
        parser.error("api version not set")
    if options.ndk == None:
        parser.error("android NDK directory not set")
    if options.output == None:
        parser.error("output directory not set")
    if options.ndk[len(options.ndk) - 1] == '/':
        options.ndk = options.ndk[:(len(options.ndk) - 1)]
    if options.output[len(options.output) - 1] == '/':
        options.output = options.output[:(len(options.output) - 1)]
    if not options.api.isdigit():
        parser.error("api version %s is not a vaild number" % options.api)
    if not os.path.isdir(options.ndk):
        parser.error("ndk directory %s not exists" % options.ndk)
    if not os.path.isdir(options.output):
        parser.error("output directory %s not exists" % options.output)
        
    options.projectName = "DarkGalgame-dev"
        
    
    #copy original files to output directory
    print "copying project files ..."
    projDir = os.path.abspath(os.path.join(options.output, options.projectName))
    if not os.path.exists(projDir):
        os.makedirs(projDir)
    else:
        overwrite = raw_input("%s already exists, do you want to overwrite? [y/n] " % options.projectName).strip()
        if (overwrite != "y"):
            exit(1)
    ret = os.system("cp -fr .. %s" % (projDir)) >> 8
    if ret != 0:
        print "cannot copy project files"
        print "\nfailed!"
        exit(-1)
     
    #set up android build tools
    print "creating android build standalone toolchain ..."
    androidBuildDir = os.path.join(projDir, ANDROID_BUILD_TOOLS_DIR)
    makeToolsScript = os.path.join(options.ndk, ANDROID_MAKE_TOOLS_SCRIPT)
    ret = os.system("%s --force --unified-headers --arch arm --api %s --install-dir %s" 
                    % (makeToolsScript, options.api, androidBuildDir)) >> 8
    if ret != 0:
        print "cannot create android build standalone toolchain"
        print "\nfailed!"
        exit(-1)
        
    #build scripts
    print "setting up build scripts ..."
    setupExternalScripts(projDir, options.api, androidBuildDir)
    setupMainScripts(projDir, options.api, androidBuildDir)
    
    #patch
    print "patching source code ..."
    ret = os.system("%s apply" % os.path.join(projDir, "tools/patch.py")) >> 8
    if ret != 0:
        print "cannot patch source code"
        print "\nfailed!"
        exit(-1)
        
    print "\nsuccess!"


if __name__ == "__main__":
    main()


