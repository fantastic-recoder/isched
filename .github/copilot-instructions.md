# isched Development Guidelines

Updated: 2026-03-12

## Active Technologies

- **Language**: C++23 with strict C++ Core Guidelines compliance
- **Build system**: CMake 3.22.6 (provided by Conan `[tool_requires]`) + Ninja 1.12.1
- **Dependency manager**: Conan 2.x — `conanfile.txt` declares all dependencies
- **Architecture**: GraphQL-only HTTP/WebSocket backend, single-process, multi-tenant
- **Key runtime dependencies**: `taocpp-pegtl`, `nlohmann_json`, `spdlog`, `jwt-cpp`, `sqlite3`, `boost/1.84.0` (Boost.URL), `cpp-httplib` (sole HTTP/WebSocket transport), `openssl`, `platformfolders`
- **Testing**: Catch2 3.x

## Project Structure

```text
src/
  main/cpp/isched/        # library sources
    backend/              # Server, GqlExecutor, TenantManager, DatabaseManager, …
    shared/               # config, fs utils, exceptions
  test/cpp/
    isched/               # unit tests
    integration/          # integration tests
specs/
  001-universal-backend/  # feature specification docs
configure.py              # one-shot configure + build script
conanfile.txt             # Conan dependency manifest
CMakeLists.txt            # root CMake project
```

## Commands

### First-time setup (run once per machine)

```bash
conan profile detect
```

### Configure and build (Linux)

Use `configure.py` — it fully automates Conan install + CMake configure + build:

```bash
python3 configure.py
```

What that script runs internally:

```bash
# Install Conan deps and generate CMake integration files into cmake-build-debug/
conan install . -of cmake-build-debug -s build_type=Debug --build=missing

# Configure with CMake using the Conan-generated toolchain
cmake . -B ./cmake-build-debug \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake-build-debug/conan_toolchain.cmake \
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
  -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build ./cmake-build-debug/
```

### Run tests

```bash
cd cmake-build-debug && ctest --output-on-failure
```

### Build a specific target

```bash
cmake --build ./cmake-build-debug/ --target isched_graphql_tests
```

### Regenerate docs

```bash
cmake --build ./cmake-build-debug/ --target docs
```

## Code Style

- **Ownership**: mandatory smart pointers (`std::unique_ptr`, `std::shared_ptr`), no raw `new`/`delete`
- **RAII**: all resources managed through RAII; no manual cleanup in destructors
- **Naming**: `isched_` prefix for all library files; `PascalCase` for types, `camelCase` for members
- **GraphQL-only transport**: no REST, no IPC, no scripting interfaces — all external access is via `/graphql`
- C++ Core Guidelines enforced; `-Wall -Wextra -Wpedantic` enabled

## Recent Changes

- 001-universal-backend: Architecture pivoted to GraphQL-only HTTP/WebSocket, IPC/scripting removed
- 001-universal-backend: Build commands documented from `configure.py`

<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
