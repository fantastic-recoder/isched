cmake_minimum_required(VERSION 3.22.6)
project(isched)
message("Building with CMake version: ${CMAKE_VERSION}")

set(CMAKE_CXX_STANDARD 23)

enable_testing()

find_package(pegtl)
find_package(Catch2 3 REQUIRED)
find_package(restbed)
find_package(spdlog)

include_directories("src/main/cpp")
add_subdirectory("src/main/cpp/isched")
add_subdirectory("src/test/cpp/isched")

