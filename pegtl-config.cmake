########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(pegtl_FIND_QUIETLY)
    set(pegtl_MESSAGE_MODE VERBOSE)
else()
    set(pegtl_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/pegtlTargets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${taocpp-pegtl_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(pegtl_VERSION_STRING "3.2.6")
set(pegtl_INCLUDE_DIRS ${taocpp-pegtl_INCLUDE_DIRS_DEBUG} )
set(pegtl_INCLUDE_DIR ${taocpp-pegtl_INCLUDE_DIRS_DEBUG} )
set(pegtl_LIBRARIES ${taocpp-pegtl_LIBRARIES_DEBUG} )
set(pegtl_DEFINITIONS ${taocpp-pegtl_DEFINITIONS_DEBUG} )


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${taocpp-pegtl_BUILD_MODULES_PATHS_DEBUG} )
    message(${pegtl_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


