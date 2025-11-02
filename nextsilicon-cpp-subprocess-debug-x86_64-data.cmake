########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

set(nextsilicon-cpp-subprocess_COMPONENT_NAMES "")
if(DEFINED nextsilicon-cpp-subprocess_FIND_DEPENDENCY_NAMES)
  list(APPEND nextsilicon-cpp-subprocess_FIND_DEPENDENCY_NAMES )
  list(REMOVE_DUPLICATES nextsilicon-cpp-subprocess_FIND_DEPENDENCY_NAMES)
else()
  set(nextsilicon-cpp-subprocess_FIND_DEPENDENCY_NAMES )
endif()

########### VARIABLES #######################################################################
#############################################################################################
set(nextsilicon-cpp-subprocess_PACKAGE_FOLDER_DEBUG "/home/groby/.conan2/p/nexts99caf2f120b15/p")
set(nextsilicon-cpp-subprocess_BUILD_MODULES_PATHS_DEBUG )


set(nextsilicon-cpp-subprocess_INCLUDE_DIRS_DEBUG "${nextsilicon-cpp-subprocess_PACKAGE_FOLDER_DEBUG}/include")
set(nextsilicon-cpp-subprocess_RES_DIRS_DEBUG )
set(nextsilicon-cpp-subprocess_DEFINITIONS_DEBUG )
set(nextsilicon-cpp-subprocess_SHARED_LINK_FLAGS_DEBUG )
set(nextsilicon-cpp-subprocess_EXE_LINK_FLAGS_DEBUG )
set(nextsilicon-cpp-subprocess_OBJECTS_DEBUG )
set(nextsilicon-cpp-subprocess_COMPILE_DEFINITIONS_DEBUG )
set(nextsilicon-cpp-subprocess_COMPILE_OPTIONS_C_DEBUG )
set(nextsilicon-cpp-subprocess_COMPILE_OPTIONS_CXX_DEBUG )
set(nextsilicon-cpp-subprocess_LIB_DIRS_DEBUG )
set(nextsilicon-cpp-subprocess_BIN_DIRS_DEBUG )
set(nextsilicon-cpp-subprocess_LIBRARY_TYPE_DEBUG UNKNOWN)
set(nextsilicon-cpp-subprocess_IS_HOST_WINDOWS_DEBUG 0)
set(nextsilicon-cpp-subprocess_LIBS_DEBUG )
set(nextsilicon-cpp-subprocess_SYSTEM_LIBS_DEBUG )
set(nextsilicon-cpp-subprocess_FRAMEWORK_DIRS_DEBUG )
set(nextsilicon-cpp-subprocess_FRAMEWORKS_DEBUG )
set(nextsilicon-cpp-subprocess_BUILD_DIRS_DEBUG )
set(nextsilicon-cpp-subprocess_NO_SONAME_MODE_DEBUG FALSE)


# COMPOUND VARIABLES
set(nextsilicon-cpp-subprocess_COMPILE_OPTIONS_DEBUG
    "$<$<COMPILE_LANGUAGE:CXX>:${nextsilicon-cpp-subprocess_COMPILE_OPTIONS_CXX_DEBUG}>"
    "$<$<COMPILE_LANGUAGE:C>:${nextsilicon-cpp-subprocess_COMPILE_OPTIONS_C_DEBUG}>")
set(nextsilicon-cpp-subprocess_LINKER_FLAGS_DEBUG
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${nextsilicon-cpp-subprocess_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${nextsilicon-cpp-subprocess_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${nextsilicon-cpp-subprocess_EXE_LINK_FLAGS_DEBUG}>")


set(nextsilicon-cpp-subprocess_COMPONENTS_DEBUG )