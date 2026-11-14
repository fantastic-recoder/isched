# SPDX-License-Identifier: MPL-2.0

if(NOT Python3_EXECUTABLE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()

add_test(
    NAME isched_package_contract_tests
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*.py" -v
)
set_tests_properties(isched_package_contract_tests PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS "packaging;host-safe"
)

# These entry points are host-safe and skip unless ISCHED_RUN_PACKAGE_CONTAINERS=1
# and the caller supplies the target package paths. They consume completed packages;
# they never invoke build_local.py or start a nested package build.
add_test(
    NAME isched_package_runtime_container_tests
    COMMAND "${Python3_EXECUTABLE}" -m unittest
            tests.package.test_runtime_install -v
)
set_tests_properties(isched_package_runtime_container_tests PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS "packaging;target-container"
)

add_test(
    NAME isched_package_sdk_container_tests
    COMMAND "${Python3_EXECUTABLE}" -m unittest
            tests.package.test_sdk_install -v
)
set_tests_properties(isched_package_sdk_container_tests PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS "packaging;target-container"
)
