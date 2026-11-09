# SPDX-License-Identifier: MPL-2.0
# CPack component metadata and the narrow package payload allowlist.

if(NOT ISCHED_ENABLE_PACKAGING)
    message(FATAL_ERROR "packaging/cmake/Package.cmake requires ISCHED_ENABLE_PACKAGING=ON")
endif()
foreach(required_variable IN ITEMS
        ISCHED_SOURCE_REVISION ISCHED_PACKAGE_VERSION ISCHED_PACKAGE_IDENTITY_FILE ISCHED_PACKAGE_TARGET
        ISCHED_PACKAGE_FORMAT ISCHED_PACKAGE_LICENSE_OUTPUT ISCHED_PACKAGE_OUTPUT_DIR)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required for package generation")
    endif()
endforeach()
if(NOT EXISTS "${ISCHED_PACKAGE_IDENTITY_FILE}")
    message(FATAL_ERROR "package identity input does not exist: ${ISCHED_PACKAGE_IDENTITY_FILE}")
endif()
if(NOT ISCHED_PACKAGE_FORMAT STREQUAL "deb" AND NOT ISCHED_PACKAGE_FORMAT STREQUAL "rpm")
    message(FATAL_ERROR "unsupported native package format: ${ISCHED_PACKAGE_FORMAT}")
endif()

file(READ "${ISCHED_PACKAGE_IDENTITY_FILE}" _isched_identity_json)
string(JSON _isched_identity_count ERROR_VARIABLE _isched_json_error LENGTH "${_isched_identity_json}")
if(_isched_json_error OR NOT _isched_identity_count EQUAL 2)
    message(FATAL_ERROR "package identity input must contain exactly two role records")
endif()

function(_isched_get_identity wanted_role output_prefix)
    set(found FALSE)
    math(EXPR last_index "${_isched_identity_count} - 1")
    foreach(index RANGE 0 ${last_index})
        string(JSON role GET "${_isched_identity_json}" ${index} role)
        if(role STREQUAL wanted_role)
            set(found TRUE)
            foreach(field IN ITEMS role package_name version native_version native_release
                    source_revision target distribution distribution_version architecture
                    native_architecture format artifact_name package_filename)
                string(JSON value ERROR_VARIABLE field_error GET "${_isched_identity_json}" ${index} ${field})
                if(field_error)
                    message(FATAL_ERROR "identity for ${wanted_role} lacks ${field}")
                endif()
                if(value STREQUAL "" AND NOT (field STREQUAL "native_release"
                        AND ISCHED_PACKAGE_FORMAT STREQUAL "deb"))
                    message(FATAL_ERROR "identity for ${wanted_role} lacks ${field}")
                endif()
                set(${output_prefix}_${field} "${value}" PARENT_SCOPE)
            endforeach()
            string(JSON identity_object GET "${_isched_identity_json}" ${index})
            set(${output_prefix}_json "${identity_object}" PARENT_SCOPE)
        endif()
    endforeach()
    if(NOT found)
        message(FATAL_ERROR "package identity has no ${wanted_role} record")
    endif()
endfunction()

_isched_get_identity(runtime _isched_runtime)
_isched_get_identity(developer _isched_developer)
foreach(prefix IN ITEMS _isched_runtime _isched_developer)
    string(LENGTH "${${prefix}_source_revision}" revision_length)
    if(NOT "${${prefix}_target}" STREQUAL "${ISCHED_PACKAGE_TARGET}"
            OR NOT "${${prefix}_format}" STREQUAL "${ISCHED_PACKAGE_FORMAT}"
            OR NOT revision_length EQUAL 40
            OR NOT "${${prefix}_source_revision}" MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${prefix} package identity disagrees with selected target/format/revision")
    endif()
    if(NOT "${${prefix}_source_revision}" STREQUAL "${ISCHED_SOURCE_REVISION}"
            OR NOT "${${prefix}_version}" STREQUAL "${ISCHED_PACKAGE_VERSION}")
        message(FATAL_ERROR "${prefix} package identity disagrees with explicit source revision/version")
    endif()
endforeach()
if(NOT _isched_runtime_source_revision STREQUAL _isched_developer_source_revision
        OR NOT _isched_runtime_version STREQUAL _isched_developer_version
        OR NOT _isched_runtime_native_version STREQUAL _isched_developer_native_version
        OR NOT _isched_runtime_native_release STREQUAL _isched_developer_native_release
        OR NOT _isched_runtime_native_architecture STREQUAL _isched_developer_native_architecture
        OR NOT _isched_runtime_target STREQUAL _isched_developer_target)
    message(FATAL_ERROR "runtime/developer package identities do not describe the same build")
endif()
if(NOT _isched_runtime_package_name STREQUAL "isched-runtime"
        OR NOT _isched_developer_package_name STREQUAL "isched-devel"
        OR NOT _isched_runtime_role STREQUAL "runtime"
        OR NOT _isched_developer_role STREQUAL "developer"
        OR NOT _isched_runtime_architecture STREQUAL "x86_64"
        OR NOT _isched_developer_architecture STREQUAL "x86_64")
    message(FATAL_ERROR "package identity role/name mapping is invalid")
endif()
if((ISCHED_PACKAGE_TARGET STREQUAL "ubuntu-26.04"
            AND (NOT ISCHED_PACKAGE_FORMAT STREQUAL "deb"
                OR NOT _isched_runtime_native_architecture STREQUAL "amd64"))
        OR (ISCHED_PACKAGE_TARGET STREQUAL "fedora-45"
            AND (NOT ISCHED_PACKAGE_FORMAT STREQUAL "rpm"
                OR NOT _isched_runtime_native_architecture STREQUAL "x86_64"))
        OR (NOT ISCHED_PACKAGE_TARGET STREQUAL "ubuntu-26.04"
            AND NOT ISCHED_PACKAGE_TARGET STREQUAL "fedora-45"))
    message(FATAL_ERROR "unsupported or mismatched initial target identity")
endif()

set(_isched_expected_extension "${ISCHED_PACKAGE_FORMAT}")
if(NOT _isched_runtime_package_filename STREQUAL "${_isched_runtime_artifact_name}.${_isched_expected_extension}"
        OR NOT _isched_developer_package_filename STREQUAL "${_isched_developer_artifact_name}.${_isched_expected_extension}")
    message(FATAL_ERROR "package filenames do not match the immutable identity records")
endif()

set(_isched_identity_output "${CMAKE_BINARY_DIR}/packaging/identity")
file(MAKE_DIRECTORY "${_isched_identity_output}")
file(WRITE "${_isched_identity_output}/runtime.json" "${_isched_runtime_json}\n")
file(WRITE "${_isched_identity_output}/developer.json" "${_isched_developer_json}\n")

install(FILES "${_isched_identity_output}/runtime.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/isched/runtime"
        RENAME package-identity.json COMPONENT runtime)
install(FILES "${_isched_identity_output}/developer.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/isched/developer"
        RENAME package-identity.json COMPONENT devel)

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE.md" "${PROJECT_SOURCE_DIR}/README.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/isched-runtime" COMPONENT runtime)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/isched-devel" COMPONENT devel)
install(FILES "${PROJECT_SOURCE_DIR}/packaging/sdk/README.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/isched-devel" COMPONENT devel)
install(FILES
        "${PROJECT_SOURCE_DIR}/packaging/sdk/conanfile.txt"
        "${PROJECT_SOURCE_DIR}/packaging/sdk/conan.lock"
        "${PROJECT_SOURCE_DIR}/packaging/requirements-conan.lock"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/isched/developer" COMPONENT devel)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/packaging/conan/profiles/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/isched/developer/profiles"
        COMPONENT devel)
install(DIRECTORY "${ISCHED_PACKAGE_LICENSE_OUTPUT}/runtime/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/isched-runtime" COMPONENT runtime)
install(DIRECTORY "${ISCHED_PACKAGE_LICENSE_OUTPUT}/developer/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/isched-devel" COMPONENT devel)

set(CPACK_PACKAGE_NAME "isched")
set(CPACK_PACKAGE_VENDOR "isched contributors")
set(CPACK_PACKAGE_CONTACT "isched contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "isched GraphQL application server and C++ SDK")
set(CPACK_PACKAGE_VERSION "${_isched_runtime_native_version}")
set(CPACK_PACKAGE_FILE_NAME "${_isched_runtime_artifact_name}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "isched")
set(CPACK_PACKAGE_DIRECTORY "${ISCHED_PACKAGE_OUTPUT_DIR}")
set(CPACK_COMPONENTS_ALL runtime devel)
set(CPACK_COMPONENTS_GROUPING IGNORE)
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)

set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "isched runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Embedded-WebUI isched server executable")
set(CPACK_COMPONENT_DEVEL_DISPLAY_NAME "isched developer SDK")
set(CPACK_COMPONENT_DEVEL_DESCRIPTION "Static library, headers, CMake config and Conan dependency inputs")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "isched contributors")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/fantastic-recoder/isched")
set(CPACK_DEBIAN_PACKAGE_VERSION "${_isched_runtime_native_version}")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${_isched_runtime_native_architecture}")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "${_isched_runtime_package_name}")
set(CPACK_DEBIAN_DEVEL_PACKAGE_NAME "${_isched_developer_package_name}")
set(CPACK_DEBIAN_RUNTIME_FILE_NAME "${_isched_runtime_package_filename}")
set(CPACK_DEBIAN_DEVEL_FILE_NAME "${_isched_developer_package_filename}")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_DEVEL_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_DEVEL_PACKAGE_DEPENDS "")

set(CPACK_RPM_PACKAGE_VERSION "${_isched_runtime_native_version}")
set(CPACK_RPM_PACKAGE_RELEASE "${_isched_runtime_native_release}")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "${_isched_runtime_native_architecture}")
set(CPACK_RPM_PACKAGE_LICENSE "MPL-2.0")
set(CPACK_RPM_RUNTIME_PACKAGE_NAME "${_isched_runtime_package_name}")
set(CPACK_RPM_DEVEL_PACKAGE_NAME "${_isched_developer_package_name}")
set(CPACK_RPM_RUNTIME_FILE_NAME "${_isched_runtime_package_filename}")
set(CPACK_RPM_DEVEL_FILE_NAME "${_isched_developer_package_filename}")
set(CPACK_RPM_RUNTIME_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_RUNTIME_PACKAGE_AUTOREQPROV ON)
set(CPACK_RPM_DEVEL_PACKAGE_REQUIRES "")

if(ISCHED_PACKAGE_FORMAT STREQUAL "deb")
    set(CPACK_GENERATOR "DEB")
else()
    set(CPACK_GENERATOR "RPM")
endif()

include(CPack)
