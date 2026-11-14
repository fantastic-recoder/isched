# SPDX-License-Identifier: MPL-2.0
cmake_minimum_required(VERSION 3.22)
project(isched_sdk_consumer LANGUAGES CXX)

find_package(isched CONFIG REQUIRED)

add_executable(isched_sdk_consumer main.cpp)
target_compile_features(isched_sdk_consumer PRIVATE cxx_std_23)
target_link_libraries(isched_sdk_consumer PRIVATE isched::isched)
