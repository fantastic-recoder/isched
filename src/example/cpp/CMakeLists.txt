# SPDX-License-Identifier: MPL-2.0
#
# Example plugins for the isched runtime.
#
# The hello_plugin target is a minimal GraphQL resolver plugin that can be
# loaded through the runtime plugin manager after authenticating as a platform
# administrator.

add_library(hello_plugin MODULE
    hello_plugin.cpp
)

add_library(request_demo_plugin MODULE
    request_demo_plugin.cpp
)

target_include_directories(hello_plugin PRIVATE
    "${CMAKE_SOURCE_DIR}/src/main/cpp"
)

target_link_libraries(hello_plugin PRIVATE
    Boost::headers
    Boost::filesystem
    nlohmann_json::nlohmann_json
)

target_include_directories(request_demo_plugin PRIVATE
    "${CMAKE_SOURCE_DIR}/src/main/cpp"
)

target_link_libraries(request_demo_plugin PRIVATE
    Boost::headers
    Boost::filesystem
    nlohmann_json::nlohmann_json
)

set_target_properties(hello_plugin PROPERTIES
    OUTPUT_NAME "hello_plugin"
)

set_target_properties(request_demo_plugin PROPERTIES
    OUTPUT_NAME "request_demo_plugin"
)

