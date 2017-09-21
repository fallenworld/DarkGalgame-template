#!/usr/bin/env python


import os
import fileinput
from optparse import OptionParser


SCRIPT_DIR = os.path.split(os.path.realpath(__file__))[0]

ANDROID_BUILD_TOOLS_DIR = "build/android"
ANDROID_MAKE_TOOLS_SCRIPT = "build/tools/make_standalone_toolchain.py"

MAIN_BUILD_SCRIPTS_DIR = "build/build-scripts"
EXTERNAL_BUILD_SCRIPTS_DIR = "build/external/build-scripts"

ALL_BUILD_SCRIPTS = [ os.path.join(MAIN_BUILD_SCRIPTS_DIR, "qemu-android-build.sh"),
                      os.path.join(MAIN_BUILD_SCRIPTS_DIR, "wine-android-build.sh"),
                      os.path.join(EXTERNAL_BUILD_SCRIPTS_DIR, "gettext-android-build.sh"), 
                      os.path.join(EXTERNAL_BUILD_SCRIPTS_DIR, "glib-android-build.sh"),
                      os.path.join(EXTERNAL_BUILD_SCRIPTS_DIR, "libffi-android-build.sh"),
                      os.path.join(EXTERNAL_BUILD_SCRIPTS_DIR, "libiconv-android-build.sh"),
                      os.path.join(EXTERNAL_BUILD_SCRIPTS_DIR, "libpng-android-build.sh"),
                      os.path.join(EXTERNAL_BUILD_SCRIPTS_DIR, "pcre-android-build.sh") ]


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
    
    
def setupAllToolScripts(projDir, args):
    setupLibsVer(projDir, os.path.join(projDir, "tools/patch.py"))
    setupLibsVer(projDir, os.path.join(projDir, "tools/build.py"))
    fileReplace(os.path.join(projDir, "build/external/external.sh"), "#{*ANDROID_BUILD*}#", args["buildDir"])


def setupAllBuildScripts(projDir, scriptList, args):
    for scriptFile in scriptList:
        fullFileName = os.path.join(projDir, scriptFile)
        fileReplace(fullFileName, "#{*ANDROID_BUILD*}#", args["buildDir"])
        fileReplace(fullFileName, "#{*API_VERSION*}#", args["apiVer"])
        fileReplace(fullFileName, "#{*GRADLE_BIN_PATH*}#", args["gradlePath"])
        fileReplace(fullFileName, "#{*ANDROID_SDK_PATH*}#", args["sdkDir"])


def main():  
    #enter py script file path
    os.chdir(os.path.split(os.path.realpath(__file__))[0])
    
    #parse command options
    parser = OptionParser()
    parser.add_option("-a", "--api", dest="api", help="target android API version", metavar="API") 
    parser.add_option("-n", "--ndk", dest="ndk", help="your android NDK directory", metavar="NDK_DIR") 
    parser.add_option("-s", "--sdk", dest="sdk", help="your android SDK directory", metavar="SDK_DIR") 
    parser.add_option("-g", "--gradle", dest="gradle", help="your gradle bin path", metavar="GRADLE_PATH") 
    parser.add_option("-o", "--output", dest="output", help="directory to output the project files", 
                      metavar="OUTPUT_DIR")
    (options, args) = parser.parse_args()
    #check arguments
    if options.api == None:
        parser.error("api version not set")
    if options.ndk == None:
        parser.error("android NDK directory not set")
    if options.sdk == None:
        parser.error("android SDK directory not set")
    if options.gradle == None:
        parser.error("gradle bin path not set")
    if options.output == None:
        parser.error("output directory not set")
    if not options.api.isdigit():
        parser.error("api version %s is not a vaild number" % options.api)
    if not os.path.isdir(options.ndk):
        parser.error("ndk directory %s not exists" % options.ndk)
    if not os.path.isdir(options.sdk):
        parser.error("sdk directory %s not exists" % options.sdk)
    if not os.path.isdir(options.gradle):
        parser.error("gradle bin path %s not exists" % options.gradle)
    if not os.path.isdir(options.output):
        parser.error("output directory %s not exists" % options.output)
    if options.ndk[len(options.ndk) - 1] == '/':
        options.ndk = options.ndk[:(len(options.ndk) - 1)]
    if options.sdk[len(options.sdk) - 1] == '/':
        options.sdk = options.sdk[:(len(options.sdk) - 1)]
    if options.gradle[len(options.gradle) - 1] == '/':
        options.gradle = options.gradle[:(len(options.gradle) - 1)]
    if options.output[len(options.output) - 1] == '/':
        options.output = options.output[:(len(options.output) - 1)]
        
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
    args = {"apiVer": options.api, 
            "buildDir": androidBuildDir, 
            "sdkDir": options.sdk, 
            "gradlePath": options.gradle}
    setupAllToolScripts(projDir, args)
    setupAllBuildScripts(projDir, ALL_BUILD_SCRIPTS, args)
    
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


