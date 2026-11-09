# SPDX-License-Identifier: MPL-2.0
# Supported public umbrella-header closure from src/main/cpp/isched/isched.hpp.
# Paths are relative to the isched source include root; install under include/isched.
# Private backend, generated schema, and generated UI headers are intentionally excluded.
set(ISCHED_PUBLIC_HEADER_RELATIVE_PATHS
    isched.hpp
    backend/isched_Server.hpp
    backend/isched_common.hpp
    shared/config/isched_config.hpp
)

# Public non-standard include reachable through the umbrella header.
set(ISCHED_PUBLIC_DEPENDENCY_TARGETS
    Boost::url
)
