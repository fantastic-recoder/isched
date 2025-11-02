# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/jwt-cpp-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${jwt-cpp_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${jwt-cpp_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET jwt-cpp::jwt-cpp)
    add_library(jwt-cpp::jwt-cpp INTERFACE IMPORTED)
    message(${jwt-cpp_MESSAGE_MODE} "Conan: Target declared 'jwt-cpp::jwt-cpp'")
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/jwt-cpp-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()