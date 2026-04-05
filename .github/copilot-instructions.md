# isched Development Guidelines

Updated: 2026-04-05

## Active Technologies

- **Language**: C++23 with strict C++ Core Guidelines compliance
- **Frontend**: Angular 21 with standalone APIs, signals, and strict TypeScript
- **Build system**: CMake 3.22.6 (provided by Conan `[tool_requires]`) + Ninja 1.12.1
- **Dependency manager**: Conan 2.x — `conanfile.txt` declares all dependencies
- **Architecture**: GraphQL-only HTTP/WebSocket backend, single-process, multi-tenant, plus Angular WebUI
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

### Monitor long-running scripts

When redirecting output to a pipe and reading it with `tail`, use `tee` in between to enable real-time monitoring:

```bash
# ✓ Good: Monitor output while tailing
./long_running_script.sh 2>&1 | tee build.log | tail -f

# ✗ Poor: Output buffered, cannot monitor
./long_running_script.sh > build.log 2>&1 | tail -f
```

This pattern allows you to:
- View real-time output as the script runs
- Preserve complete output in the log file
- Switch between live monitoring and final results

## Code Style

- **Ownership**: mandatory smart pointers (`std::unique_ptr`, `std::shared_ptr`), no raw `new`/`delete`
- **RAII**: all resources managed through RAII; no manual cleanup in destructors
- **Naming**: `isched_` prefix for all library files; `PascalCase` for types, `camelCase` for members
- **GraphQL-only transport**: no REST, no IPC, no scripting interfaces — all external access is via `/graphql`
- C++ Core Guidelines enforced; `-Wall -Wextra -Wpedantic` enabled
- For `src/ui/`: signal-first state, standalone components/directives/pipes, `@if/@for/@switch`, typed reactive forms, strict TS/template checks, and zoneless/`OnPush`-compatible patterns where feasible
- For `src/ui/`: app-owned template state must be signal-backed; do not use async-pipe-driven state from component-owned observables except when bridging immutable third-party stream contracts with explicit documentation
- For `src/ui/`: templates and styles MUST be in separate files — use `templateUrl` and `styleUrl`, never inline `template` or `styles`; each component gets a `.html` and `.scss` file alongside its `.ts` file
- For `src/ui/`: CSS framework is Tailwind CSS 3.x + DaisyUI 4.x — use DaisyUI component classes (`btn`, `card`, `alert`, `modal`) for consistent styling; extend with Tailwind utilities as needed
- Browser JWT handling must avoid persistent token storage (`localStorage`/`sessionStorage`/IndexedDB); prefer secure cookie-based flows
- Local Angular development should use proxy routing for `/graphql` (HTTP + WS), not hard-coded backend origins

## Workflow Rules

- **Commit after every task**: Once a task is complete and `ctest` is fully green, create a Git commit before moving to the next task. Never leave completed work uncommitted.
- **Green before commit**: `ctest --output-on-failure` MUST pass 100% as a pre-commit gate. A commit with a failing test suite is not allowed.
- **Commit message format**: `feat|fix|refactor|test(<scope>): <summary>` followed by a body listing which task IDs were addressed and any notable bugs fixed.

## Recent Changes

- 001-universal-backend: Architecture pivoted to GraphQL-only HTTP/WebSocket, IPC/scripting removed
- 001-universal-backend: Build commands documented from `configure.py`
- 001-universal-backend: Commit-after-each-task rule added
- Monitoring: Added `tee` pattern for real-time script output monitoring while preserving logs
- 004-add-isched-webui: Added Tailwind CSS 3.x + DaisyUI 4.x with `corporate` theme
- 004-add-isched-webui: Enforced separate template/style files convention (`templateUrl`/`styleUrl`)

<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
