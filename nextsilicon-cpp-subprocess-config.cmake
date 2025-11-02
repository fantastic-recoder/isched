########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(nextsilicon-cpp-subprocess_FIND_QUIETLY)
    set(nextsilicon-cpp-subprocess_MESSAGE_MODE VERBOSE)
else()
    set(nextsilicon-cpp-subprocess_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/nextsilicon-cpp-subprocessTargets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${nextsilicon-cpp-subprocess_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(nextsilicon-cpp-subprocess_VERSION_STRING "2.0.2")
set(nextsilicon-cpp-subprocess_INCLUDE_DIRS ${nextsilicon-cpp-subprocess_INCLUDE_DIRS_DEBUG} )
set(nextsilicon-cpp-subprocess_INCLUDE_DIR ${nextsilicon-cpp-subprocess_INCLUDE_DIRS_DEBUG} )
set(nextsilicon-cpp-subprocess_LIBRARIES ${nextsilicon-cpp-subprocess_LIBRARIES_DEBUG} )
set(nextsilicon-cpp-subprocess_DEFINITIONS ${nextsilicon-cpp-subprocess_DEFINITIONS_DEBUG} )


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${nextsilicon-cpp-subprocess_BUILD_MODULES_PATHS_DEBUG} )
    message(${nextsilicon-cpp-subprocess_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


