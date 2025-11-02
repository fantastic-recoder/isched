# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/nextsilicon-cpp-subprocess-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${nextsilicon-cpp-subprocess_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${nextsilicon-cpp-subprocess_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess)
    add_library(nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess INTERFACE IMPORTED)
    message(${nextsilicon-cpp-subprocess_MESSAGE_MODE} "Conan: Target declared 'nextsilicon-cpp-subprocess::nextsilicon-cpp-subprocess'")
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/nextsilicon-cpp-subprocess-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()