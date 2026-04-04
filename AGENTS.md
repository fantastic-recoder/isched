# isched Agent Guidelines

This file provides development guidelines for AI coding agents operating in this repository.

## Project Overview

- **Language**: C++23
- **Build system**: CMake + Ninja
- **Dependency manager**: Conan 2.x
- **Test framework**: Catch2 3.x
- **Architecture**: GraphQL-only HTTP/WebSocket backend, single-process, multi-tenant

## Project Structure

```
src/
  main/cpp/isched/
    backend/              # Server, GqlExecutor, TenantManager, DatabaseManager
    shared/               # config, fs utils, exceptions
  test/cpp/isched/        # unit tests (catch2)
specs/001-universal-backend/  # feature specification docs
```

## Build Commands

### First-time setup
```bash
conan profile detect
python3 configure.py
```

### Configure + build (or just run configure.py)
```bash
python3 configure.py
```

### Run all tests
```bash
cd cmake-build-debug && ctest --output-on-failure
```

### Run a single test (by test name)
```bash
cd cmake-build-debug && ctest -R isched_graphql_tests --output-on-failure
```

### Run a test executable directly
```bash
./cmake-build-debug/src/test/cpp/isched/isched_graphql_tests
```

### Build a specific target
```bash
cmake --build ./cmake-build-debug/ --target isched_graphql_tests
```

### Security scan (clang-tidy with security checks)
```bash
cmake --build ./cmake-build-debug/ --target security_scan
```

### Generate documentation (Doxygen)
```bash
cmake --build cmake-build-debug --target docs
```

### List all available tests
```bash
cd cmake-build-debug && ctest -N
```

## Code Style Guidelines

### File Naming
- All library source files use `isched_` prefix (e.g., `isched_Server.hpp`, `isched_Server.cpp`)
- Tests follow pattern: `isched_<feature>_tests.cpp`
- Headers in `.hpp`, implementation in `.cpp`

### Naming Conventions
- **Types/Classes**: PascalCase (e.g., `Server`, `GqlExecutor`, `Configuration`)
- **Functions/Methods**: camelCase (e.g., `create()`, `execute()`)
- **Member variables**: camelCase (e.g., `port`, `max_threads`)
- **Enums**: PascalCase values (e.g., `Status::Running`)

### Type Aliases
Use these instead of raw std:: types:
```cpp
using String = std::string;
using Duration = std::chrono::milliseconds;
template<typename T> using UniquePtr = std::unique_ptr<T>;
template<typename T> using SharedPtr = std::shared_ptr<T>;
```

### Includes Order
1. Associated header (e.g., in `.cpp` file, include its `.hpp` first)
2. Project headers: `<isched/backend/...>`, `<isched/shared/...>`
3. Standard library: `<memory>`, `<string>`, `<vector>`, etc.
4. Third-party: `<nlohmann/json.hpp>`, `<catch2/...>`

### Ownership and RAII
- **Mandatory**: Use smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- **Never**: Raw `new`/`delete`, raw owning pointers
- Factory methods return `UniquePtr<T>` or `SharedPtr<T>`

### Documentation (Doxygen)
All public APIs must have Doxygen documentation:
- Classes: `@brief`, detailed explanation, `@author`, usage examples
- Functions: `@brief`, `@param`, `@return`, `@throw`
- Use `@code` blocks for examples, `@see`, `@ref` for cross-references

```cpp
/**
 * @class Server
 * @brief Main server class for Universal Application Server Backend
 * @author isched Development Team
 */
class Server {
public:
    /**
     * @brief Factory method to create server instance
     * @param config Optional configuration (uses defaults if not provided)
     * @return Unique pointer to configured server instance
     * @throw std::runtime_error if server creation fails
     */
    static UniquePtr<Server> create(const Configuration& config = Configuration{});
};
```

### Error Handling
- Use `std::optional` for functions that may fail without an error message
- Use exceptions for catastrophic failures (file not found, cannot bind port)
- Log errors with spdlog before throwing
- Test error paths with CHECK_THROWS, CHECK_FALSE

### Testing (Catch2)
- Use SECTION for sub-tests within a TEST_CASE
- Use tags: `[graphql][executor]`, `[database]`, `[auth]`
- Use REQUIRE for assertions that must pass, CHECK for graceful failures

```cpp
TEST_CASE("GraphQL Executor Basic Functionality", "[graphql][executor]") {
    auto database = std::make_shared<DatabaseManager>(config);
    GqlExecutor executor(database);
    SECTION("Execute hello resolver") {
        auto result = executor.execute("query {hello}");
        REQUIRE(result.is_success());
        REQUIRE(result.data.contains("hello"));
    }
}
```

### License Header
All source files must include the MPL-2.0 license header:
```cpp
// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_Server.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief Core server class for isched Universal Application Server Backend
 * @author isched Development Team
 * @version 1.0.0
 * @date 2025-11-01
 */
```

### Namespace Usage
```cpp
namespace isched::v0_0_1::backend {
    class Server { /* ... */ };
}
```

## Dependencies (with links)

- [cpp-httplib](https://github.com/yhirose/cpp-httplib) — HTTP/WebSocket transport
- [nlohmann/json](https://github.com/nlohmann/json) — JSON serialization
- [spdlog](https://github.com/gabime/spdlog) — structured logging
- [Catch2](https://github.com/catchorg/Catch2) — test framework
- [SQLite3](https://www.sqlite.org/) — per-tenant embedded database
- [jwt-cpp](https://github.com/Thalhammer/jwt-cpp) — JWT authentication
- [Boost](https://www.boost.org/) (URL, Asio, Beast) — URL parsing, WebSocket
- [taocpp-pegtl](https://github.com/taocpp/PEGTL) — GraphQL grammar parsing
- [platformfolders](https://github.com/sago35/platformfolders-cpp) — platform directories

## Workflow Rules

1. **Green before commit**: All tests must pass before creating a commit
   ```bash
   cd cmake-build-debug && ctest --output-on-failure
   ```

2. **Commit after task completion**: Create a commit once a task is complete and all tests pass

3. **Run security scan before push**:
   ```bash
   cmake --build ./cmake-build-debug/ --target security_scan
   ```

## Available Test Names

- `isched_grammar_tests`, `isched_graphql_tests`, `isched_gql_executor_tests`
- `isched_srv_test`, `basic_server_test`, `tenant_manager_test`
- `isched_resolver_tests`, `database_test`, `isched_auth_tests`
- `isched_subscription_broker_tests`, `isched_config_tests`
- `test_server_startup`, `test_builtin_schema`, `test_health_queries`
- `isched_fs_utils_tests`, `isched_ast_node_tests`, `isched_multi_dim_map_tests`
- `test_configuration_snapshots`, `test_schema_activation`, `test_configuration_rollback`
- `test_graphql_websocket`, `test_graphql_subscriptions`, `test_client_compatibility`
- `test_user_management`, `test_session_revocation`
- `isched_rest_datasource_tests`, `isched_tenant_thread_pool_tests`
- `isched_metrics_tests`, `benchmark_suite`

## Additional Documentation References

- [GraphQL Grammar Protocol](docs/isched_gql_grammar_readme.md) — grammar implementation, spec compliance, AST tools
- [Using Directives](docs/guides/directives.md) — guide on using and defining directives