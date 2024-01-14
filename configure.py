#!/usr/bin/env python3
import os
import pathlib
import shutil

if __name__ == '__main__':
    os.system("conan profile detect")
    myBuildDir = pathlib.Path("cmake-build-debug")
    if myBuildDir.exists():
        shutil.rmtree(myBuildDir)
    #myBuildFile = pathlib.Path("CMakeLists.txt")
    #if myBuildFile.exists():
    #    os.remove(myBuildFile)
    myBuildDir.mkdir()
    myRetVal = os.system("conan install . -of cmake-build-debug -s build_type=Debug --build=missing")
    if myRetVal != 0:
        myBuildDir.rmdir()
        exit(myRetVal)
    # CMake options after "cmake . -B ./cmake-build-debug "
    os.system("cmake . -B ./cmake-build-debug -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake-build-debug/conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_BUILD_TYPE=Debug")
    os.system("cmake --build ./cmake-build-debug/")
