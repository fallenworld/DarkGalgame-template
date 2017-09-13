#!/usr/bin/env python

import os
import fileinput
from optparse import OptionParser

EXTERNAL_BUILD_SCRIPTS_DIR = "%s/build/libs/build-scripts"
ANDROID_BUILD_TOOLS_DIR = "%s/build/android"
ANDROID_MAKE_TOOLS_SCRIPT = "%s/build/tools/make_standalone_toolchain.py"

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
    

def main():  
    #parse command options
    parser = OptionParser()
    parser.add_option("-a", "--api", dest="api", help="target android API version", metavar="API") 
    parser.add_option("-n", "--ndk", dest="ndk", help="your android NDK directory", metavar="NDK_DIR") 
    parser.add_option("-o", "--output", dest="output", help="directory to output the project files", 
                      metavar="OUTPUT_DIR")
    (options, args) = parser.parse_args()
    if options.api == None:
        parser.error("api version not set")
    if options.ndk == None:
        parser.error("android NDK directory not set")
    if options.output == None:
        parser.error("output directory not set")
    options.projectName = "DarkGalgame"
        
    #copy original files to output directory
    print "copy project files"
    projectDir = os.path.abspath(options.output + "/" + options.projectName)
    if not os.path.exists(projectDir):
        os.makedirs(projectDir)
    os.system("cp -fr .. %s" % (projectDir))
     
    #set up android build tools
    print "make android build standalone toolchain"
    androidBuildDir = ANDROID_BUILD_TOOLS_DIR % (projectDir)
    makeToolsScript = ANDROID_MAKE_TOOLS_SCRIPT % (options.ndk)
    os.system("%s --unified-headers --arch arm --api %s --install-dir %s" 
              % (makeToolsScript, options.api, androidBuildDir))

    #setup external library build scripts
    print "set up build scripts"
    for scriptFiles in EXTERNAL_BUILD_SCRIPTS:
        fullFileName = EXTERNAL_BUILD_SCRIPTS_DIR % (projectDir) + "/" + scriptFiles
        fileReplace(fullFileName, "{*API_VERSION*}", options.api)
        fileReplace(fullFileName, "{*ANDROID_BUILD*}", androidBuildDir)
    

if __name__ == "__main__":
    main()


