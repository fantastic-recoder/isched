#!/usr/bin/env python3
import os
import pathlib
import shutil

if __name__ == '__main__':
    os.system("conan profile detect")
    myBuildDir = pathlib.Path("cmake-build-debug")
    if myBuildDir.exists():
        shutil.rmtree(myBuildDir)
    myBuildDir.mkdir()
    myRetVal = os.system("conan install . -of cmake-build-debug -s build_type=Debug --build=missing")
    if myRetVal != 0:
        myBuildDir.rmdir()
        exit(myRetVal)
    # CMake options after "cmake . -B ./cmake-build-debug "
    print("OS name: ", os.name);
    if os.name == "nt":
        os.system("cmake . -B ./cmake-build-debug -DCMAKE_TOOLCHAIN_FILE=cmake-build-debug/conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_BUILD_TYPE=Debug")
    else:
        os.system("cmake . -B ./cmake-build-debug -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake-build-debug/conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_BUILD_TYPE=Debug")
    os.system("cmake --build ./cmake-build-debug/")
