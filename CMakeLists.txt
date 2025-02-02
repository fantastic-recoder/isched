cmake_minimum_required(VERSION 3.16.3)
project(isched)
message("Building with CMake version: ${CMAKE_VERSION}")

set(CMAKE_CXX_STANDARD 20)

enable_testing()


find_package(pegtl)
find_package(Catch2 3 REQUIRED)
find_package(restbed)
find_package(spdlog)
find_package(nextsilicon-cpp-subprocess)
find_package(httplib)

include_directories("src/main/cpp")
add_subdirectory("src/main/cpp/isched")
add_subdirectory("src/test/cpp/isched")

