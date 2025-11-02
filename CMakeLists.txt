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
find_package(Doxygen OPTIONAL_COMPONENTS dot)

if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE} (version ${DOXYGEN_VERSION})")
    
    # Check for optional components
    if(DOXYGEN_DOT_FOUND)
        message(STATUS "GraphViz dot found: ${DOXYGEN_DOT_EXECUTABLE} - enabling call graphs and class diagrams")
        set(DOXYGEN_HAVE_DOT YES)
    else()
        # Try to find dot manually if CMake didn't find it
        find_program(DOT_EXECUTABLE dot)
        if(DOT_EXECUTABLE)
            message(STATUS "GraphViz dot found manually: ${DOT_EXECUTABLE} - enabling call graphs and class diagrams")
            set(DOXYGEN_HAVE_DOT YES)
            set(DOXYGEN_DOT_EXECUTABLE ${DOT_EXECUTABLE})
        else()
            message(STATUS "GraphViz dot not found - documentation will be generated without graphs")
            set(DOXYGEN_HAVE_DOT NO)
        endif()
    endif()
    
    # Verify Doxyfile exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/Doxyfile")
        message(STATUS "Doxyfile found at: ${CMAKE_SOURCE_DIR}/Doxyfile")
        
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM)
        
        # Build documentation automatically after isched library is built
        add_dependencies(docs isched)
        
        # Optional: Add docs to the default build target
        # Uncomment the following line to build documentation by default
        # add_custom_target(build_all ALL DEPENDS isched docs)
        
        message(STATUS "Documentation target 'docs' configured - use 'cmake --build . --target docs' to generate")
    else()
        message(WARNING "Doxyfile not found at ${CMAKE_SOURCE_DIR}/Doxyfile - documentation target not available")
    endif()
else()
    message(STATUS "Doxygen not found - documentation will not be generated")
    message(STATUS "To install Doxygen: sudo dnf install doxygen graphviz (Fedora/RHEL) or sudo apt install doxygen graphviz (Ubuntu/Debian)")
endif()


find_package(pegtl)
find_package(Catch2 3 REQUIRED)
find_package(restbed)
find_package(spdlog)
find_package(nextsilicon-cpp-subprocess)
find_package(httplib)
find_package(nlohmann_json REQUIRED)
find_package(jwt-cpp REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories("src/main/cpp")
add_subdirectory("src/main/cpp/isched")
add_subdirectory("src/test/cpp/isched")

