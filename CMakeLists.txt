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

# Clang-tidy integration for C++ Core Guidelines compliance (temporarily disabled)
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(FALSE AND CLANG_TIDY_EXE AND NOT DISABLE_CLANG_TIDY)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
else()
    message(STATUS "clang-tidy disabled or not found - static analysis will be skipped")
    message(STATUS "To install clang-tidy: sudo dnf install clang-tools-extra (Fedora/RHEL) or sudo apt install clang-tidy (Ubuntu/Debian)")
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
find_package(spdlog)
find_package(httplib)
find_package(nlohmann_json REQUIRED)
find_package(jwt-cpp REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(platform_folders REQUIRED)
# Find Boost package
find_package(Boost REQUIRED COMPONENTS url system thread)

include_directories("src/main/cpp")

# ── Angular UI build + asset embedding (T-UI-B-001 through T-UI-B-004) ──────
# isched_ui_build: compile the Angular subproject into dist/isched-ui/browser/
# isched_ui_embed: run embed_ui_assets.py to generate isched_ui_assets.hpp
# The isched library target depends on isched_ui_embed so the header is always
# present when the library is compiled.

find_program(PNPM_EXE NAMES "pnpm")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(GLOB_RECURSE UI_SOURCES "${CMAKE_SOURCE_DIR}/src/ui/src/*")

if(PNPM_EXE)
    add_custom_target(isched_ui_build
        COMMAND ${PNPM_EXE} --filter isched-ui run build
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS ${UI_SOURCES}
        BYPRODUCTS "${CMAKE_SOURCE_DIR}/src/ui/dist/isched-ui/browser/index.html"
        COMMENT "Building Angular Admin UI (pnpm --filter isched-ui run build)"
        VERBATIM)

    add_custom_target(isched_ui_embed
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/embed_ui_assets.py"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS isched_ui_build
        BYPRODUCTS "${CMAKE_SOURCE_DIR}/src/main/cpp/isched/backend/isched_ui_assets.hpp"
        COMMENT "Embedding Angular UI assets into C++ header"
        VERBATIM)
    message(STATUS "isched_ui_build and isched_ui_embed targets configured (pnpm: ${PNPM_EXE})")
else()
    # pnpm not found — create stub targets that warn but do not error
    add_custom_target(isched_ui_build
        COMMAND ${CMAKE_COMMAND} -E echo
            "WARNING: pnpm not found — UI build skipped. isched_ui_assets.hpp will not be regenerated."
        COMMENT "pnpm not found — isched_ui_build skipped")
    add_custom_target(isched_ui_embed
        DEPENDS isched_ui_build
        COMMAND ${CMAKE_COMMAND} -E echo
            "WARNING: pnpm not found — UI embed skipped."
        COMMENT "pnpm not found — isched_ui_embed skipped")
    message(WARNING "pnpm not found — Admin UI will not be automatically built or embedded.")
    message(STATUS "Install pnpm: npm install -g pnpm")
endif()

add_subdirectory("src/main/cpp/isched")
add_subdirectory("src/test/cpp/isched")

# Intentional load-bearing dependency: the isched library must not be compiled
# until the generated isched_ui_assets.hpp header exists (produced by isched_ui_embed).
add_dependencies(isched isched_ui_embed)

# T056: Security scan target using clang-tidy with a security-focused check set.
# Run: cmake --build ./cmake-build-debug/ --target security_scan
find_program(CLANG_TIDY_EXE NAMES "clang-tidy-18" "clang-tidy")
find_program(RUN_CLANG_TIDY_EXE NAMES "run-clang-tidy-18" "run-clang-tidy")
if(CLANG_TIDY_EXE AND RUN_CLANG_TIDY_EXE)
    file(GLOB_RECURSE ISCHED_SECURITY_SOURCES
        "${CMAKE_SOURCE_DIR}/src/main/cpp/isched/backend/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/main/cpp/isched/shared/*.cpp")

    add_custom_target(security_scan
        COMMAND ${RUN_CLANG_TIDY_EXE}
            -clang-tidy-binary ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            -header-filter "^${CMAKE_SOURCE_DIR}/src/"
            -extra-arg=-isystem/usr/include/c++/13
            -extra-arg=-isystem/usr/include/x86_64-linux-gnu/c++/13
            # clang-18 defines __cpp_concepts=201907L but GCC-13 libstdc++ <expected>
            # guards with >= 202002L; override so that std::expected resolves correctly.
            "-extra-arg=-D__cpp_concepts=202002L"
            ${ISCHED_SECURITY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy security scan on isched library sources"
        VERBATIM)
    message(STATUS "security_scan target configured (clang-tidy: ${CLANG_TIDY_EXE})")
else()
    message(STATUS "clang-tidy not found — security_scan target not available")
    message(STATUS "Install: sudo apt install clang-tidy (Ubuntu/Debian) or sudo dnf install clang-tools-extra (Fedora/RHEL)")
endif()

