# SPDX-License-Identifier: MPL-2.0
include(CMakePackageConfigHelpers)
set(_ISCHED_SDK_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(isched_install_sdk_config)
    if(NOT DEFINED ISCHED_PACKAGE_VERSION OR ISCHED_PACKAGE_VERSION STREQUAL "")
        message(FATAL_ERROR "ISCHED_PACKAGE_VERSION is required to generate the isched SDK package config")
    endif()

    string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" ISCHED_CMAKE_PACKAGE_VERSION "${ISCHED_PACKAGE_VERSION}")
    if(ISCHED_CMAKE_PACKAGE_VERSION STREQUAL "")
        message(FATAL_ERROR "ISCHED_PACKAGE_VERSION must begin with MAJOR.MINOR.PATCH: ${ISCHED_PACKAGE_VERSION}")
    endif()

    set(_isched_cmake_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/isched")
    configure_package_config_file(
        "${_ISCHED_SDK_CMAKE_DIR}/ischedConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ischedConfig.cmake"
        INSTALL_DESTINATION "${_isched_cmake_package_dir}")
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ischedConfigVersion.cmake"
        VERSION "${ISCHED_CMAKE_PACKAGE_VERSION}"
        COMPATIBILITY SameMajorVersion)

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ischedConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ischedConfigVersion.cmake"
        DESTINATION "${_isched_cmake_package_dir}"
        COMPONENT devel)
endfunction()
