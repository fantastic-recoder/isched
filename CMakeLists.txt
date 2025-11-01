cmake_minimum_required(VERSION 3.16.3)
project(isched)
message("Building with CMake version: ${CMAKE_VERSION}")

# Set C++23 standard for Universal Application Server Backend
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable C++ Core Guidelines compliance checks
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

enable_testing()

# Doxygen documentation generation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
endif()


find_package(pegtl)
find_package(Catch2 3 REQUIRED)
find_package(restbed)
find_package(spdlog)
find_package(nextsilicon-cpp-subprocess)
find_package(httplib)
find_package(nlohmann_json REQUIRED)
find_package(jwt-cpp REQUIRED)

include_directories("src/main/cpp")
add_subdirectory("src/main/cpp/isched")
add_subdirectory("src/test/cpp/isched")

