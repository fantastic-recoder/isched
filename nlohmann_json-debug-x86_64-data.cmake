########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

set(nlohmann_json_COMPONENT_NAMES "")
if(DEFINED nlohmann_json_FIND_DEPENDENCY_NAMES)
  list(APPEND nlohmann_json_FIND_DEPENDENCY_NAMES )
  list(REMOVE_DUPLICATES nlohmann_json_FIND_DEPENDENCY_NAMES)
else()
  set(nlohmann_json_FIND_DEPENDENCY_NAMES )
endif()

########### VARIABLES #######################################################################
#############################################################################################
set(nlohmann_json_PACKAGE_FOLDER_DEBUG "/home/groby/.conan2/p/nlohm0567ffc90cfc1/p")
set(nlohmann_json_BUILD_MODULES_PATHS_DEBUG )


set(nlohmann_json_INCLUDE_DIRS_DEBUG "${nlohmann_json_PACKAGE_FOLDER_DEBUG}/include")
set(nlohmann_json_RES_DIRS_DEBUG )
set(nlohmann_json_DEFINITIONS_DEBUG )
set(nlohmann_json_SHARED_LINK_FLAGS_DEBUG )
set(nlohmann_json_EXE_LINK_FLAGS_DEBUG )
set(nlohmann_json_OBJECTS_DEBUG )
set(nlohmann_json_COMPILE_DEFINITIONS_DEBUG )
set(nlohmann_json_COMPILE_OPTIONS_C_DEBUG )
set(nlohmann_json_COMPILE_OPTIONS_CXX_DEBUG )
set(nlohmann_json_LIB_DIRS_DEBUG )
set(nlohmann_json_BIN_DIRS_DEBUG )
set(nlohmann_json_LIBRARY_TYPE_DEBUG UNKNOWN)
set(nlohmann_json_IS_HOST_WINDOWS_DEBUG 0)
set(nlohmann_json_LIBS_DEBUG )
set(nlohmann_json_SYSTEM_LIBS_DEBUG )
set(nlohmann_json_FRAMEWORK_DIRS_DEBUG )
set(nlohmann_json_FRAMEWORKS_DEBUG )
set(nlohmann_json_BUILD_DIRS_DEBUG )
set(nlohmann_json_NO_SONAME_MODE_DEBUG FALSE)


# COMPOUND VARIABLES
set(nlohmann_json_COMPILE_OPTIONS_DEBUG
    "$<$<COMPILE_LANGUAGE:CXX>:${nlohmann_json_COMPILE_OPTIONS_CXX_DEBUG}>"
    "$<$<COMPILE_LANGUAGE:C>:${nlohmann_json_COMPILE_OPTIONS_C_DEBUG}>")
set(nlohmann_json_LINKER_FLAGS_DEBUG
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${nlohmann_json_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${nlohmann_json_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${nlohmann_json_EXE_LINK_FLAGS_DEBUG}>")


set(nlohmann_json_COMPONENTS_DEBUG )