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

target_include_directories(hello_plugin PRIVATE
    "${CMAKE_SOURCE_DIR}/src/main/cpp"
)

target_link_libraries(hello_plugin PRIVATE
    Boost::headers
    Boost::filesystem
    nlohmann_json::nlohmann_json
)

set_target_properties(hello_plugin PROPERTIES
    OUTPUT_NAME "hello_plugin"
)

