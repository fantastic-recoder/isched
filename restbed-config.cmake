########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(restbed_FIND_QUIETLY)
    set(restbed_MESSAGE_MODE VERBOSE)
else()
    set(restbed_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/restbedTargets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${restbed_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(restbed_VERSION_STRING "4.8")
set(restbed_INCLUDE_DIRS ${restbed_INCLUDE_DIRS_DEBUG} )
set(restbed_INCLUDE_DIR ${restbed_INCLUDE_DIRS_DEBUG} )
set(restbed_LIBRARIES ${restbed_LIBRARIES_DEBUG} )
set(restbed_DEFINITIONS ${restbed_DEFINITIONS_DEBUG} )


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${restbed_BUILD_MODULES_PATHS_DEBUG} )
    message(${restbed_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


