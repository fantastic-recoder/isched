cmake_minimum_required(VERSION 3.22.6)
project(isched)
message("Building with CMake version: ${CMAKE_VERSION}")

set(CMAKE_CXX_STANDARD 23)

find_package(pegtl)
add_executable(isched src/main/cpp/isched/main.cpp)
target_link_libraries(isched PRIVATE taocpp::pegtl)
