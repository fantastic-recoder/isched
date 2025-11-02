########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

set(restbed_COMPONENT_NAMES "")
if(DEFINED restbed_FIND_DEPENDENCY_NAMES)
  list(APPEND restbed_FIND_DEPENDENCY_NAMES OpenSSL)
  list(REMOVE_DUPLICATES restbed_FIND_DEPENDENCY_NAMES)
else()
  set(restbed_FIND_DEPENDENCY_NAMES OpenSSL)
endif()
set(OpenSSL_FIND_MODE "NO_MODULE")

########### VARIABLES #######################################################################
#############################################################################################
set(restbed_PACKAGE_FOLDER_DEBUG "/home/groby/.conan2/p/b/restbcfa8a280206eb/p")
set(restbed_BUILD_MODULES_PATHS_DEBUG )


set(restbed_INCLUDE_DIRS_DEBUG "${restbed_PACKAGE_FOLDER_DEBUG}/include")
set(restbed_RES_DIRS_DEBUG )
set(restbed_DEFINITIONS_DEBUG )
set(restbed_SHARED_LINK_FLAGS_DEBUG )
set(restbed_EXE_LINK_FLAGS_DEBUG )
set(restbed_OBJECTS_DEBUG )
set(restbed_COMPILE_DEFINITIONS_DEBUG )
set(restbed_COMPILE_OPTIONS_C_DEBUG )
set(restbed_COMPILE_OPTIONS_CXX_DEBUG )
set(restbed_LIB_DIRS_DEBUG "${restbed_PACKAGE_FOLDER_DEBUG}/lib")
set(restbed_BIN_DIRS_DEBUG )
set(restbed_LIBRARY_TYPE_DEBUG STATIC)
set(restbed_IS_HOST_WINDOWS_DEBUG 0)
set(restbed_LIBS_DEBUG restbed)
set(restbed_SYSTEM_LIBS_DEBUG dl m)
set(restbed_FRAMEWORK_DIRS_DEBUG )
set(restbed_FRAMEWORKS_DEBUG )
set(restbed_BUILD_DIRS_DEBUG )
set(restbed_NO_SONAME_MODE_DEBUG FALSE)


# COMPOUND VARIABLES
set(restbed_COMPILE_OPTIONS_DEBUG
    "$<$<COMPILE_LANGUAGE:CXX>:${restbed_COMPILE_OPTIONS_CXX_DEBUG}>"
    "$<$<COMPILE_LANGUAGE:C>:${restbed_COMPILE_OPTIONS_C_DEBUG}>")
set(restbed_LINKER_FLAGS_DEBUG
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${restbed_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${restbed_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${restbed_EXE_LINK_FLAGS_DEBUG}>")


set(restbed_COMPONENTS_DEBUG )