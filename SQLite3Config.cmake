########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(SQLite3_FIND_QUIETLY)
    set(SQLite3_MESSAGE_MODE VERBOSE)
else()
    set(SQLite3_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/SQLite3Targets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${sqlite3_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(SQLite3_VERSION_STRING "3.44.0")
set(SQLite3_INCLUDE_DIRS ${sqlite3_INCLUDE_DIRS_DEBUG} )
set(SQLite3_INCLUDE_DIR ${sqlite3_INCLUDE_DIRS_DEBUG} )
set(SQLite3_LIBRARIES ${sqlite3_LIBRARIES_DEBUG} )
set(SQLite3_DEFINITIONS ${sqlite3_DEFINITIONS_DEBUG} )


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${sqlite3_BUILD_MODULES_PATHS_DEBUG} )
    message(${SQLite3_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


