// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_common.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Common definitions and namespace declarations for isched Universal Application Server Backend
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-01
 * 
 * This file contains common definitions, smart pointer aliases, and namespace declarations
 * used throughout the isched Universal Application Server Backend implementation.
 * 
 * @note This implementation follows C++ Core Guidelines with mandatory smart pointer usage.
 * Raw pointers are prohibited except for non-owning observer scenarios.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>

/**
 * @namespace isched
 * @brief Root namespace for isched application framework
 */
namespace isched {

/**
 * @namespace isched::v0_0_1
 * @brief Version-specific namespace for API stability
 */
namespace v0_0_1 {

/**
 * @namespace isched::v0_0_1::backend
 * @brief Universal Application Server Backend implementation namespace
 * 
 * Contains all classes and functions related to the Universal Application Server Backend,
 * including server management, tenant isolation, GraphQL processing, and CLI coordination.
 */
namespace backend {

// Forward declarations for core classes
class Server;
class TenantManager;
class Database;
class GqlExecutor;
class ResolverSystem;
class CLICoordinator;
class AuthManager;
class ConfigurationManager;

/**
 * @brief Smart pointer type aliases for consistent memory management
 * 
 * These aliases enforce the use of smart pointers throughout the codebase
 * as required by C++ Core Guidelines and FR-021.
 */
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

/**
 * @brief Common type aliases for convenience
 */
using String = std::string;
using StringVector = std::vector<std::string>;
using StringMap = std::map<std::string, std::string>;
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::milliseconds;

/**
 * @brief Error callback function type
 * @param error_message Human-readable error description
 * @param error_code Application-specific error code
 * @param context Additional context for debugging
 */
using ErrorCallback = std::function<void(const String& error_message, 
                                       int error_code, 
                                       const StringMap& context)>;

/**
 * @brief Configuration update callback function type
 * @param config_json JSON configuration string
 * @param version Configuration version identifier
 */
using ConfigUpdateCallback = std::function<void(const String& config_json, 
                                               const String& version)>;

} // namespace backend

/**
 * @namespace isched::v0_0_1::runtime
 * @brief Shared runtime library namespace
 * 
 * Contains shared functionality used by both the main server and CLI executables.
 */
namespace runtime {

// Forward declarations for runtime classes
class IPC;
class PluginAPI;
class Runtime;

} // namespace runtime

} // namespace v0_0_1
} // namespace isched