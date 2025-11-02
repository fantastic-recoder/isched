########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

list(APPEND sqlite3_COMPONENT_NAMES SQLite::SQLite3)
list(REMOVE_DUPLICATES sqlite3_COMPONENT_NAMES)
if(DEFINED sqlite3_FIND_DEPENDENCY_NAMES)
  list(APPEND sqlite3_FIND_DEPENDENCY_NAMES )
  list(REMOVE_DUPLICATES sqlite3_FIND_DEPENDENCY_NAMES)
else()
  set(sqlite3_FIND_DEPENDENCY_NAMES )
endif()

########### VARIABLES #######################################################################
#############################################################################################
set(sqlite3_PACKAGE_FOLDER_DEBUG "/home/groby/.conan2/p/b/sqlitd93147fc3e050/p")
set(sqlite3_BUILD_MODULES_PATHS_DEBUG )


set(sqlite3_INCLUDE_DIRS_DEBUG "${sqlite3_PACKAGE_FOLDER_DEBUG}/include")
set(sqlite3_RES_DIRS_DEBUG )
set(sqlite3_DEFINITIONS_DEBUG )
set(sqlite3_SHARED_LINK_FLAGS_DEBUG )
set(sqlite3_EXE_LINK_FLAGS_DEBUG )
set(sqlite3_OBJECTS_DEBUG )
set(sqlite3_COMPILE_DEFINITIONS_DEBUG )
set(sqlite3_COMPILE_OPTIONS_C_DEBUG )
set(sqlite3_COMPILE_OPTIONS_CXX_DEBUG )
set(sqlite3_LIB_DIRS_DEBUG "${sqlite3_PACKAGE_FOLDER_DEBUG}/lib")
set(sqlite3_BIN_DIRS_DEBUG )
set(sqlite3_LIBRARY_TYPE_DEBUG STATIC)
set(sqlite3_IS_HOST_WINDOWS_DEBUG 0)
set(sqlite3_LIBS_DEBUG sqlite3)
set(sqlite3_SYSTEM_LIBS_DEBUG pthread dl m)
set(sqlite3_FRAMEWORK_DIRS_DEBUG )
set(sqlite3_FRAMEWORKS_DEBUG )
set(sqlite3_BUILD_DIRS_DEBUG )
set(sqlite3_NO_SONAME_MODE_DEBUG FALSE)


# COMPOUND VARIABLES
set(sqlite3_COMPILE_OPTIONS_DEBUG
    "$<$<COMPILE_LANGUAGE:CXX>:${sqlite3_COMPILE_OPTIONS_CXX_DEBUG}>"
    "$<$<COMPILE_LANGUAGE:C>:${sqlite3_COMPILE_OPTIONS_C_DEBUG}>")
set(sqlite3_LINKER_FLAGS_DEBUG
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${sqlite3_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${sqlite3_SHARED_LINK_FLAGS_DEBUG}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${sqlite3_EXE_LINK_FLAGS_DEBUG}>")


set(sqlite3_COMPONENTS_DEBUG SQLite::SQLite3)
########### COMPONENT SQLite::SQLite3 VARIABLES ############################################

set(sqlite3_SQLite_SQLite3_INCLUDE_DIRS_DEBUG "${sqlite3_PACKAGE_FOLDER_DEBUG}/include")
set(sqlite3_SQLite_SQLite3_LIB_DIRS_DEBUG "${sqlite3_PACKAGE_FOLDER_DEBUG}/lib")
set(sqlite3_SQLite_SQLite3_BIN_DIRS_DEBUG )
set(sqlite3_SQLite_SQLite3_LIBRARY_TYPE_DEBUG STATIC)
set(sqlite3_SQLite_SQLite3_IS_HOST_WINDOWS_DEBUG 0)
set(sqlite3_SQLite_SQLite3_RES_DIRS_DEBUG )
set(sqlite3_SQLite_SQLite3_DEFINITIONS_DEBUG )
set(sqlite3_SQLite_SQLite3_OBJECTS_DEBUG )
set(sqlite3_SQLite_SQLite3_COMPILE_DEFINITIONS_DEBUG )
set(sqlite3_SQLite_SQLite3_COMPILE_OPTIONS_C_DEBUG "")
set(sqlite3_SQLite_SQLite3_COMPILE_OPTIONS_CXX_DEBUG "")
set(sqlite3_SQLite_SQLite3_LIBS_DEBUG sqlite3)
set(sqlite3_SQLite_SQLite3_SYSTEM_LIBS_DEBUG pthread dl m)
set(sqlite3_SQLite_SQLite3_FRAMEWORK_DIRS_DEBUG )
set(sqlite3_SQLite_SQLite3_FRAMEWORKS_DEBUG )
set(sqlite3_SQLite_SQLite3_DEPENDENCIES_DEBUG )
set(sqlite3_SQLite_SQLite3_SHARED_LINK_FLAGS_DEBUG )
set(sqlite3_SQLite_SQLite3_EXE_LINK_FLAGS_DEBUG )
set(sqlite3_SQLite_SQLite3_NO_SONAME_MODE_DEBUG FALSE)

# COMPOUND VARIABLES
set(sqlite3_SQLite_SQLite3_LINKER_FLAGS_DEBUG
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${sqlite3_SQLite_SQLite3_SHARED_LINK_FLAGS_DEBUG}>
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${sqlite3_SQLite_SQLite3_SHARED_LINK_FLAGS_DEBUG}>
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${sqlite3_SQLite_SQLite3_EXE_LINK_FLAGS_DEBUG}>
)
set(sqlite3_SQLite_SQLite3_COMPILE_OPTIONS_DEBUG
    "$<$<COMPILE_LANGUAGE:CXX>:${sqlite3_SQLite_SQLite3_COMPILE_OPTIONS_CXX_DEBUG}>"
    "$<$<COMPILE_LANGUAGE:C>:${sqlite3_SQLite_SQLite3_COMPILE_OPTIONS_C_DEBUG}>")