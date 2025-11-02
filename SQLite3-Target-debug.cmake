# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(sqlite3_FRAMEWORKS_FOUND_DEBUG "") # Will be filled later
conan_find_apple_frameworks(sqlite3_FRAMEWORKS_FOUND_DEBUG "${sqlite3_FRAMEWORKS_DEBUG}" "${sqlite3_FRAMEWORK_DIRS_DEBUG}")

set(sqlite3_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET sqlite3_DEPS_TARGET)
    add_library(sqlite3_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET sqlite3_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:Debug>:${sqlite3_FRAMEWORKS_FOUND_DEBUG}>
             $<$<CONFIG:Debug>:${sqlite3_SYSTEM_LIBS_DEBUG}>
             $<$<CONFIG:Debug>:>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### sqlite3_DEPS_TARGET to all of them
conan_package_library_targets("${sqlite3_LIBS_DEBUG}"    # libraries
                              "${sqlite3_LIB_DIRS_DEBUG}" # package_libdir
                              "${sqlite3_BIN_DIRS_DEBUG}" # package_bindir
                              "${sqlite3_LIBRARY_TYPE_DEBUG}"
                              "${sqlite3_IS_HOST_WINDOWS_DEBUG}"
                              sqlite3_DEPS_TARGET
                              sqlite3_LIBRARIES_TARGETS  # out_libraries_targets
                              "_DEBUG"
                              "sqlite3"    # package_name
                              "${sqlite3_NO_SONAME_MODE_DEBUG}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${sqlite3_BUILD_DIRS_DEBUG} ${CMAKE_MODULE_PATH})

########## COMPONENTS TARGET PROPERTIES Debug ########################################

    ########## COMPONENT SQLite::SQLite3 #############

        set(sqlite3_SQLite_SQLite3_FRAMEWORKS_FOUND_DEBUG "")
        conan_find_apple_frameworks(sqlite3_SQLite_SQLite3_FRAMEWORKS_FOUND_DEBUG "${sqlite3_SQLite_SQLite3_FRAMEWORKS_DEBUG}" "${sqlite3_SQLite_SQLite3_FRAMEWORK_DIRS_DEBUG}")

        set(sqlite3_SQLite_SQLite3_LIBRARIES_TARGETS "")

        ######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
        if(NOT TARGET sqlite3_SQLite_SQLite3_DEPS_TARGET)
            add_library(sqlite3_SQLite_SQLite3_DEPS_TARGET INTERFACE IMPORTED)
        endif()

        set_property(TARGET sqlite3_SQLite_SQLite3_DEPS_TARGET
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:Debug>:${sqlite3_SQLite_SQLite3_FRAMEWORKS_FOUND_DEBUG}>
                     $<$<CONFIG:Debug>:${sqlite3_SQLite_SQLite3_SYSTEM_LIBS_DEBUG}>
                     $<$<CONFIG:Debug>:${sqlite3_SQLite_SQLite3_DEPENDENCIES_DEBUG}>
                     )

        ####### Find the libraries declared in cpp_info.component["xxx"].libs,
        ####### create an IMPORTED target for each one and link the 'sqlite3_SQLite_SQLite3_DEPS_TARGET' to all of them
        conan_package_library_targets("${sqlite3_SQLite_SQLite3_LIBS_DEBUG}"
                              "${sqlite3_SQLite_SQLite3_LIB_DIRS_DEBUG}"
                              "${sqlite3_SQLite_SQLite3_BIN_DIRS_DEBUG}" # package_bindir
                              "${sqlite3_SQLite_SQLite3_LIBRARY_TYPE_DEBUG}"
                              "${sqlite3_SQLite_SQLite3_IS_HOST_WINDOWS_DEBUG}"
                              sqlite3_SQLite_SQLite3_DEPS_TARGET
                              sqlite3_SQLite_SQLite3_LIBRARIES_TARGETS
                              "_DEBUG"
                              "sqlite3_SQLite_SQLite3"
                              "${sqlite3_SQLite_SQLite3_NO_SONAME_MODE_DEBUG}")


        ########## TARGET PROPERTIES #####################################
        set_property(TARGET SQLite::SQLite3
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:Debug>:${sqlite3_SQLite_SQLite3_OBJECTS_DEBUG}>
                     $<$<CONFIG:Debug>:${sqlite3_SQLite_SQLite3_LIBRARIES_TARGETS}>
                     )

        if("${sqlite3_SQLite_SQLite3_LIBS_DEBUG}" STREQUAL "")
            # If the component is not declaring any "cpp_info.components['foo'].libs" the system, frameworks etc are not
            # linked to the imported targets and we need to do it to the global target
            set_property(TARGET SQLite::SQLite3
                         APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                         sqlite3_SQLite_SQLite3_DEPS_TARGET)
        endif()

        set_property(TARGET SQLite::SQLite3 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                     $<$<CONFIG:Debug>:${sqlite3_SQLite_SQLite3_LINKER_FLAGS_DEBUG}>)
        set_property(TARGET SQLite::SQLite3 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                     $<$<CONFIG:Debug>:${sqlite3_SQLite_SQLite3_INCLUDE_DIRS_DEBUG}>)
        set_property(TARGET SQLite::SQLite3 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                     $<$<CONFIG:Debug>:${sqlite3_SQLite_SQLite3_LIB_DIRS_DEBUG}>)
        set_property(TARGET SQLite::SQLite3 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                     $<$<CONFIG:Debug>:${sqlite3_SQLite_SQLite3_COMPILE_DEFINITIONS_DEBUG}>)
        set_property(TARGET SQLite::SQLite3 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                     $<$<CONFIG:Debug>:${sqlite3_SQLite_SQLite3_COMPILE_OPTIONS_DEBUG}>)

    ########## AGGREGATED GLOBAL TARGET WITH THE COMPONENTS #####################
    set_property(TARGET SQLite::SQLite3 APPEND PROPERTY INTERFACE_LINK_LIBRARIES SQLite::SQLite3)

########## For the modules (FindXXX)
set(sqlite3_LIBRARIES_DEBUG SQLite::SQLite3)
