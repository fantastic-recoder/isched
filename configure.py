import os
import pathlib

myBuildDir = pathlib.Path("cmake-build-debug")
if not myBuildDir.exists():
    myBuildDir.mkdir()
os.system("conan install . -of cmake-build-debug -s build_type=Debug --build=missing")
os.system("cmake . -B ./cmake-build-debug -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake-build-debug/conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_BUILD_TYPE=Debug")
os.system("cmake --build ./cmake-build-debug/")