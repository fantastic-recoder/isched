# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(cpp-httplib_FRAMEWORKS_FOUND_DEBUG "") # Will be filled later
conan_find_apple_frameworks(cpp-httplib_FRAMEWORKS_FOUND_DEBUG "${cpp-httplib_FRAMEWORKS_DEBUG}" "${cpp-httplib_FRAMEWORK_DIRS_DEBUG}")

set(cpp-httplib_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET cpp-httplib_DEPS_TARGET)
    add_library(cpp-httplib_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET cpp-httplib_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:Debug>:${cpp-httplib_FRAMEWORKS_FOUND_DEBUG}>
             $<$<CONFIG:Debug>:${cpp-httplib_SYSTEM_LIBS_DEBUG}>
             $<$<CONFIG:Debug>:>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### cpp-httplib_DEPS_TARGET to all of them
conan_package_library_targets("${cpp-httplib_LIBS_DEBUG}"    # libraries
                              "${cpp-httplib_LIB_DIRS_DEBUG}" # package_libdir
                              "${cpp-httplib_BIN_DIRS_DEBUG}" # package_bindir
                              "${cpp-httplib_LIBRARY_TYPE_DEBUG}"
                              "${cpp-httplib_IS_HOST_WINDOWS_DEBUG}"
                              cpp-httplib_DEPS_TARGET
                              cpp-httplib_LIBRARIES_TARGETS  # out_libraries_targets
                              "_DEBUG"
                              "cpp-httplib"    # package_name
                              "${cpp-httplib_NO_SONAME_MODE_DEBUG}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${cpp-httplib_BUILD_DIRS_DEBUG} ${CMAKE_MODULE_PATH})

########## GLOBAL TARGET PROPERTIES Debug ########################################
    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 $<$<CONFIG:Debug>:${cpp-httplib_OBJECTS_DEBUG}>
                 $<$<CONFIG:Debug>:${cpp-httplib_LIBRARIES_TARGETS}>
                 )

    if("${cpp-httplib_LIBS_DEBUG}" STREQUAL "")
        # If the package is not declaring any "cpp_info.libs" the package deps, system libs,
        # frameworks etc are not linked to the imported targets and we need to do it to the
        # global target
        set_property(TARGET httplib::httplib
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     cpp-httplib_DEPS_TARGET)
    endif()

    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                 $<$<CONFIG:Debug>:${cpp-httplib_LINKER_FLAGS_DEBUG}>)
    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 $<$<CONFIG:Debug>:${cpp-httplib_INCLUDE_DIRS_DEBUG}>)
    # Necessary to find LINK shared libraries in Linux
    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                 $<$<CONFIG:Debug>:${cpp-httplib_LIB_DIRS_DEBUG}>)
    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                 $<$<CONFIG:Debug>:${cpp-httplib_COMPILE_DEFINITIONS_DEBUG}>)
    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                 $<$<CONFIG:Debug>:${cpp-httplib_COMPILE_OPTIONS_DEBUG}>)

########## For the modules (FindXXX)
set(cpp-httplib_LIBRARIES_DEBUG httplib::httplib)
