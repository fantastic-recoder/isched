########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

set(cpp-httplib_COMPONENT_NAMES "")
if(DEFINED cpp-httplib_FIND_DEPENDENCY_NAMES)
  list(APPEND cpp-httplib_FIND_DEPENDENCY_NAMES )
  list(REMOVE_DUPLICATES cpp-httplib_FIND_DEPENDENCY_NAMES)
else()
  set(cpp-httplib_FIND_DEPENDENCY_NAMES )
endif()

########### VARIABLES #######################################################################
#############################################################################################
set(cpp-httplib_PACKAGE_FOLDER_DEBUG "/home/groby/.conan2/p/cpp-ha374c5007c641/p")
set(cpp-httplib_BUILD_MODULES_PATHS_DEBUG )


set(cpp-httplib_INCLUDE_DIRS_DEBUG "${cpp-httplib_PACKAGE_FOLDER_DEBUG}/include"
			"${cpp-httplib_PACKAGE_FOLDER_DEBUG}/include/httplib")
set(cpp-httplib_RES_DIRS_DEBUG )
set(cpp-httplib_DEFINITIONS_DEBUG )
set(cpp-httplib_SHARED_LINK_FLAGS_DEBUG )
set(cpp-httplib_EXE_LINK_FLAGS_DEBUG )
set(cpp-httplib_OBJECTS_DEBUG )
set(cpp-httplib_COMPILE_DEFINITIONS_DEBUG )
set(cpp-httplib_COMPILE_OPTIONS_C_DEBUG )
set(cpp-httplib_COMPILE_OPTIONS_CXX_DEBUG )
set(cpp-httplib_LIB_DIRS_DEBUG )
set(cpp-httplib_BIN_DIRS_DEBUG )
set(cpp-httplib_LIBRARY_TYPE_DEBUG UNKNOWN)
set(cpp-httplib_IS_HOST_WINDOWS_DEBUG 0)
set(cpp-httplib_LIBS_DEBUG )
set(cpp-httplib_SYSTEM_LIBS_DEBUG pthread)
set(cpp-httplib_FRAMEWORK_DIRS_DEBUG )
set(cpp-httplib_FRAMEWORKS_DEBUG )
set(cpp-httplib_BUILD_DIRS_DEBUG )
set(cpp-httplib_NO_SONAME_MODE_DEBUG FALSE)


# COMPOUND VARIABLES
set(cpp-httplib_COMPILE_OPTIONS_DEBUG
    "$<$<COMPILE_LANGUAGE:CXX>:${cpp-httplib_COMPILE_OPTIONS_CXX_DEBUG}>"
    "$<$<COMPILE_LANGUAGE:C>:${cpp-httplib_COMPILE_OPTIONS_C_DEBUG}>")
set(cpp-httplib_LINKER_FLAGS_DEBUG
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${cpp-httplib_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${cpp-httplib_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${cpp-httplib_EXE_LINK_FLAGS_DEBUG}>")


set(cpp-httplib_COMPONENTS_DEBUG )