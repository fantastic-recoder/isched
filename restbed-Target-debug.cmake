# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(restbed_FRAMEWORKS_FOUND_DEBUG "") # Will be filled later
conan_find_apple_frameworks(restbed_FRAMEWORKS_FOUND_DEBUG "${restbed_FRAMEWORKS_DEBUG}" "${restbed_FRAMEWORK_DIRS_DEBUG}")

set(restbed_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET restbed_DEPS_TARGET)
    add_library(restbed_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET restbed_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:Debug>:${restbed_FRAMEWORKS_FOUND_DEBUG}>
             $<$<CONFIG:Debug>:${restbed_SYSTEM_LIBS_DEBUG}>
             $<$<CONFIG:Debug>:openssl::openssl>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### restbed_DEPS_TARGET to all of them
conan_package_library_targets("${restbed_LIBS_DEBUG}"    # libraries
                              "${restbed_LIB_DIRS_DEBUG}" # package_libdir
                              "${restbed_BIN_DIRS_DEBUG}" # package_bindir
                              "${restbed_LIBRARY_TYPE_DEBUG}"
                              "${restbed_IS_HOST_WINDOWS_DEBUG}"
                              restbed_DEPS_TARGET
                              restbed_LIBRARIES_TARGETS  # out_libraries_targets
                              "_DEBUG"
                              "restbed"    # package_name
                              "${restbed_NO_SONAME_MODE_DEBUG}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${restbed_BUILD_DIRS_DEBUG} ${CMAKE_MODULE_PATH})

########## GLOBAL TARGET PROPERTIES Debug ########################################
    set_property(TARGET restbed::restbed
                 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 $<$<CONFIG:Debug>:${restbed_OBJECTS_DEBUG}>
                 $<$<CONFIG:Debug>:${restbed_LIBRARIES_TARGETS}>
                 )

    if("${restbed_LIBS_DEBUG}" STREQUAL "")
        # If the package is not declaring any "cpp_info.libs" the package deps, system libs,
        # frameworks etc are not linked to the imported targets and we need to do it to the
        # global target
        set_property(TARGET restbed::restbed
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     restbed_DEPS_TARGET)
    endif()

    set_property(TARGET restbed::restbed
                 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                 $<$<CONFIG:Debug>:${restbed_LINKER_FLAGS_DEBUG}>)
    set_property(TARGET restbed::restbed
                 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 $<$<CONFIG:Debug>:${restbed_INCLUDE_DIRS_DEBUG}>)
    # Necessary to find LINK shared libraries in Linux
    set_property(TARGET restbed::restbed
                 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                 $<$<CONFIG:Debug>:${restbed_LIB_DIRS_DEBUG}>)
    set_property(TARGET restbed::restbed
                 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                 $<$<CONFIG:Debug>:${restbed_COMPILE_DEFINITIONS_DEBUG}>)
    set_property(TARGET restbed::restbed
                 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                 $<$<CONFIG:Debug>:${restbed_COMPILE_OPTIONS_DEBUG}>)

########## For the modules (FindXXX)
set(restbed_LIBRARIES_DEBUG restbed::restbed)
