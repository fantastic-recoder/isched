# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(nextsilicon-cpp-subprocess_FRAMEWORKS_FOUND_DEBUG "") # Will be filled later
conan_find_apple_frameworks(nextsilicon-cpp-subprocess_FRAMEWORKS_FOUND_DEBUG "${nextsilicon-cpp-subprocess_FRAMEWORKS_DEBUG}" "${nextsilicon-cpp-subprocess_FRAMEWORK_DIRS_DEBUG}")

set(nextsilicon-cpp-subprocess_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET nextsilicon-cpp-subprocess_DEPS_TARGET)
    add_library(nextsilicon-cpp-subprocess_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET nextsilicon-cpp-subprocess_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:Debug>:${nextsilicon-cpp-subprocess_FRAMEWORKS_FOUND_DEBUG}>
             $<$<CONFIG:Debug>:${nextsilicon-cpp-subprocess_SYSTEM_LIBS_DEBUG}>
             $<$<CONFIG:Debug>:>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### nextsilicon-cpp-subprocess_DEPS_TARGET to all of them
conan_package_library_targets("${nextsilicon-cpp-subprocess_LIBS_DEBUG}"    # libraries
                              "${nextsilicon-cpp-subprocess_LIB_DIRS_DEBUG}" # package_libdir
                              "${nextsilicon-cpp-subprocess_BIN_DIRS_DEBUG}" # package_bindir
                              "${nextsilicon-cpp-subprocess_LIBRARY_TYPE_DEBUG}"
                              "${nextsilicon-cpp-subprocess_IS_HOST_WINDOWS_DEBUG}"
                              nextsilicon-cpp-subprocess_DEPS_TARGET
                              nextsilicon-cpp-subprocess_LIBRARIES_TARGETS  # out_libraries_targets
                              "_DEBUG"
                              "nextsilicon-cpp-subprocess"    # package_name
                              "${nextsilicon-cpp-subprocess_NO_SONAME_MODE_DEBUG}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${nextsilicon-cpp-subprocess_BUILD_DIRS_DEBUG} ${CMAKE_MODULE_PATH})

########## GLOBAL TARGET PROPERTIES Debug ########################################
    set_property(TARGET nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess
                 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 $<$<CONFIG:Debug>:${nextsilicon-cpp-subprocess_OBJECTS_DEBUG}>
                 $<$<CONFIG:Debug>:${nextsilicon-cpp-subprocess_LIBRARIES_TARGETS}>
                 )

    if("${nextsilicon-cpp-subprocess_LIBS_DEBUG}" STREQUAL "")
        # If the package is not declaring any "cpp_info.libs" the package deps, system libs,
        # frameworks etc are not linked to the imported targets and we need to do it to the
        # global target
        set_property(TARGET nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     nextsilicon-cpp-subprocess_DEPS_TARGET)
    endif()

    set_property(TARGET nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess
                 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                 $<$<CONFIG:Debug>:${nextsilicon-cpp-subprocess_LINKER_FLAGS_DEBUG}>)
    set_property(TARGET nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess
                 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 $<$<CONFIG:Debug>:${nextsilicon-cpp-subprocess_INCLUDE_DIRS_DEBUG}>)
    # Necessary to find LINK shared libraries in Linux
    set_property(TARGET nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess
                 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                 $<$<CONFIG:Debug>:${nextsilicon-cpp-subprocess_LIB_DIRS_DEBUG}>)
    set_property(TARGET nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess
                 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                 $<$<CONFIG:Debug>:${nextsilicon-cpp-subprocess_COMPILE_DEFINITIONS_DEBUG}>)
    set_property(TARGET nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess
                 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                 $<$<CONFIG:Debug>:${nextsilicon-cpp-subprocess_COMPILE_OPTIONS_DEBUG}>)

########## For the modules (FindXXX)
set(nextsilicon-cpp-subprocess_LIBRARIES_DEBUG nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess)
