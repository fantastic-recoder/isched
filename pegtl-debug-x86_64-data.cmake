########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

list(APPEND taocpp-pegtl_COMPONENT_NAMES taocpp::pegtl)
list(REMOVE_DUPLICATES taocpp-pegtl_COMPONENT_NAMES)
if(DEFINED taocpp-pegtl_FIND_DEPENDENCY_NAMES)
  list(APPEND taocpp-pegtl_FIND_DEPENDENCY_NAMES )
  list(REMOVE_DUPLICATES taocpp-pegtl_FIND_DEPENDENCY_NAMES)
else()
  set(taocpp-pegtl_FIND_DEPENDENCY_NAMES )
endif()

########### VARIABLES #######################################################################
#############################################################################################
set(taocpp-pegtl_PACKAGE_FOLDER_DEBUG "/home/groby/.conan2/p/taocp6eb15311ff75e/p")
set(taocpp-pegtl_BUILD_MODULES_PATHS_DEBUG )


set(taocpp-pegtl_INCLUDE_DIRS_DEBUG "${taocpp-pegtl_PACKAGE_FOLDER_DEBUG}/include")
set(taocpp-pegtl_RES_DIRS_DEBUG )
set(taocpp-pegtl_DEFINITIONS_DEBUG )
set(taocpp-pegtl_SHARED_LINK_FLAGS_DEBUG )
set(taocpp-pegtl_EXE_LINK_FLAGS_DEBUG )
set(taocpp-pegtl_OBJECTS_DEBUG )
set(taocpp-pegtl_COMPILE_DEFINITIONS_DEBUG )
set(taocpp-pegtl_COMPILE_OPTIONS_C_DEBUG )
set(taocpp-pegtl_COMPILE_OPTIONS_CXX_DEBUG )
set(taocpp-pegtl_LIB_DIRS_DEBUG "${taocpp-pegtl_PACKAGE_FOLDER_DEBUG}/lib")
set(taocpp-pegtl_BIN_DIRS_DEBUG )
set(taocpp-pegtl_LIBRARY_TYPE_DEBUG UNKNOWN)
set(taocpp-pegtl_IS_HOST_WINDOWS_DEBUG 0)
set(taocpp-pegtl_LIBS_DEBUG )
set(taocpp-pegtl_SYSTEM_LIBS_DEBUG )
set(taocpp-pegtl_FRAMEWORK_DIRS_DEBUG )
set(taocpp-pegtl_FRAMEWORKS_DEBUG )
set(taocpp-pegtl_BUILD_DIRS_DEBUG )
set(taocpp-pegtl_NO_SONAME_MODE_DEBUG FALSE)


# COMPOUND VARIABLES
set(taocpp-pegtl_COMPILE_OPTIONS_DEBUG
    "$<$<COMPILE_LANGUAGE:CXX>:${taocpp-pegtl_COMPILE_OPTIONS_CXX_DEBUG}>"
    "$<$<COMPILE_LANGUAGE:C>:${taocpp-pegtl_COMPILE_OPTIONS_C_DEBUG}>")
set(taocpp-pegtl_LINKER_FLAGS_DEBUG
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${taocpp-pegtl_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${taocpp-pegtl_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${taocpp-pegtl_EXE_LINK_FLAGS_DEBUG}>")


set(taocpp-pegtl_COMPONENTS_DEBUG taocpp::pegtl)
########### COMPONENT taocpp::pegtl VARIABLES ############################################

set(taocpp-pegtl_taocpp_pegtl_INCLUDE_DIRS_DEBUG "${taocpp-pegtl_PACKAGE_FOLDER_DEBUG}/include")
set(taocpp-pegtl_taocpp_pegtl_LIB_DIRS_DEBUG "${taocpp-pegtl_PACKAGE_FOLDER_DEBUG}/lib")
set(taocpp-pegtl_taocpp_pegtl_BIN_DIRS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_LIBRARY_TYPE_DEBUG UNKNOWN)
set(taocpp-pegtl_taocpp_pegtl_IS_HOST_WINDOWS_DEBUG 0)
set(taocpp-pegtl_taocpp_pegtl_RES_DIRS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_DEFINITIONS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_OBJECTS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_COMPILE_DEFINITIONS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_COMPILE_OPTIONS_C_DEBUG "")
set(taocpp-pegtl_taocpp_pegtl_COMPILE_OPTIONS_CXX_DEBUG "")
set(taocpp-pegtl_taocpp_pegtl_LIBS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_SYSTEM_LIBS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_FRAMEWORK_DIRS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_FRAMEWORKS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_DEPENDENCIES_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_SHARED_LINK_FLAGS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_EXE_LINK_FLAGS_DEBUG )
set(taocpp-pegtl_taocpp_pegtl_NO_SONAME_MODE_DEBUG FALSE)

# COMPOUND VARIABLES
set(taocpp-pegtl_taocpp_pegtl_LINKER_FLAGS_DEBUG
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${taocpp-pegtl_taocpp_pegtl_SHARED_LINK_FLAGS_DEBUG}>
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${taocpp-pegtl_taocpp_pegtl_SHARED_LINK_FLAGS_DEBUG}>
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${taocpp-pegtl_taocpp_pegtl_EXE_LINK_FLAGS_DEBUG}>
)
set(taocpp-pegtl_taocpp_pegtl_COMPILE_OPTIONS_DEBUG
    "$<$<COMPILE_LANGUAGE:CXX>:${taocpp-pegtl_taocpp_pegtl_COMPILE_OPTIONS_CXX_DEBUG}>"
    "$<$<COMPILE_LANGUAGE:C>:${taocpp-pegtl_taocpp_pegtl_COMPILE_OPTIONS_C_DEBUG}>")