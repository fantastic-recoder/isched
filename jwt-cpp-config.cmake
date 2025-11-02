########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(jwt-cpp_FIND_QUIETLY)
    set(jwt-cpp_MESSAGE_MODE VERBOSE)
else()
    set(jwt-cpp_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/jwt-cppTargets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${jwt-cpp_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(jwt-cpp_VERSION_STRING "0.7.0")
set(jwt-cpp_INCLUDE_DIRS ${jwt-cpp_INCLUDE_DIRS_DEBUG} )
set(jwt-cpp_INCLUDE_DIR ${jwt-cpp_INCLUDE_DIRS_DEBUG} )
set(jwt-cpp_LIBRARIES ${jwt-cpp_LIBRARIES_DEBUG} )
set(jwt-cpp_DEFINITIONS ${jwt-cpp_DEFINITIONS_DEBUG} )


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${jwt-cpp_BUILD_MODULES_PATHS_DEBUG} )
    message(${jwt-cpp_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


