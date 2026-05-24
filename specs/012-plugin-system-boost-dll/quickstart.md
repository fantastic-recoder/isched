# Quickstart: Implementing the SP-012 Plugin System Gap Closure

**Branch**: `012-plugin-system-boost-dll`

This guide is for contributors implementing the five gap areas identified in `plan.md`.
The core plugin system is already working — these are targeted additions.

---

## Prerequisites

```bash
cd /home/groby/dev/isched
git checkout 012-plugin-system-boost-dll

# Build and confirm baseline passes
cmake --preset conan-release -S . -B build/release
cmake --build build/release --parallel
cd build/release && ctest --output-on-failure  # expect 53/53 passing
```

---

## Implementation Order

Follow the phased sequence in `plan.md §Implementation Sequence`. Phases A and B are independent
additive helpers — work on them first. Phase C has the highest coupling. Phase D is a 4-line change.

---

## Phase A+B — ABI versioning, OpenAPI validation, template routing

### Step 1: Add `abi_version` to `PluginManifest`

In `isched_PluginSystem.hpp`, add to the struct and add the two constants:

```cpp
// In PluginManifest:
String abi_version{"1.0.0"};

// Module scope (before the struct):
inline constexpr std::string_view PLUGIN_ABI_VERSION = "1.0.0";
inline constexpr int PLUGIN_ABI_MAJOR = 1;
```

### Step 2: Add helpers in `isched_PluginSystem.cpp` anonymous namespace

```cpp
// Parse the major component from a dotted semver string.
// Returns nullopt if the string is empty or does not start with a valid integer.
std::optional<int> extract_semver_major(std::string_view sv) {
    if (sv.empty()) return std::nullopt;
    const auto dot = sv.find('.');
    const auto major_str = dot == std::string_view::npos ? sv : sv.substr(0, dot);
    // parse major_str as integer
    ...
}

// Returns error string on bad fragment, nullopt on OK.
std::optional<String> validate_openapi_fragment(const String& json_str) {
    if (json_str.empty()) return std::nullopt;
    const auto parsed = json::parse(json_str, nullptr, false);
    if (parsed.is_discarded()) return "Plugin openApiFragmentJson is not valid JSON";
    return std::nullopt;
}

// Template-aware route functions
bool is_template_segment(std::string_view seg);
std::vector<std::string_view> split_path_segments(std::string_view path);
bool routes_overlap(const PluginRoute& a, const PluginRoute& b);
bool route_matches_request(const PluginRoute& r, std::string_view method, std::string_view target);
```

### Step 3: Wire into load functions

In `load_request_plugin()` and `load_resolver_plugin()` after `manifest = plugin->manifest()`:

```cpp
// ABI check
if (const auto err = validate_abi_version(manifest); err) {
    result.errors.push_back(*err);
    return result;
}
// OpenAPI check (request only)
if (const auto err = validate_openapi_fragment(manifest.openApiFragmentJson); err) {
    result.errors.push_back(*err);
    return result;
}
```

Update `route_conflicts_with_loaded` to call `routes_overlap` and `dispatch_request` to call `route_matches_request`.

---

## Phase C — Crash containment + IResolverRegistrar

### Step 1: Add `IResolverRegistrar` to the header

See `data-model.md §New Interface` for the full declaration.

### Step 2: Update `ResolverPlugin` signatures

```cpp
virtual void registerResolvers(IResolverRegistrar& registrar) = 0;
virtual void unregisterResolvers(IResolverRegistrar& registrar) = 0;
```

### Step 3: Update `StubResolverPlugin` in tests

Change `registerResolvers(GqlExecutor& executor)` → `registerResolvers(IResolverRegistrar& registrar)`.
The body still calls `registrar.register_resolver(...)` — the call site is unchanged.

### Step 4: Add `CrashSafeRegistrar` in `isched_PluginSystem.cpp`

```cpp
// In anonymous namespace
class CrashSafeRegistrar final : public IResolverRegistrar {
    GqlExecutor& m_executor;
    String m_plugin_id;
    PluginManager& m_manager;
public:
    CrashSafeRegistrar(GqlExecutor& ex, String id, PluginManager& mgr)
        : m_executor(ex), m_plugin_id(std::move(id)), m_manager(mgr) {}

    void register_resolver(const ResolverPath& scope,
                           const std::string& field,
                           ResolverFunction fn) override {
        String pid = m_plugin_id;
        m_executor.register_resolver(scope, field,
            [fn = std::move(fn), pid, &mgr = m_manager]
            (const json& parent, const json& args, const ResolverCtx& ctx) -> json {
                try {
                    return fn(parent, args, ctx);
                } catch (const std::exception& ex) {
                    spdlog::error("Plugin '{}' resolver threw: {}", pid, ex.what());
                    mgr.crash_unload_resolver_plugin(pid, ex);
                    return json{{"errors", json::array({json{{"message",
                        std::string("Plugin error: ") + ex.what()}}})}};
                }
            });
    }
    void unregister_resolver(const ResolverPath& scope,
                             const std::string& field) override {
        m_executor.unregister_resolver(scope, field);
    }
};
```

### Step 5: Make `dispatch_request` non-const, add request crash guard

```cpp
// Wrap handleRequest call:
try {
    if (auto response = request_plugin->handleRequest(request)) {
        return response;
    }
} catch (const std::exception& ex) {
    return crash_unload_request_plugin(loaded.manifest.id, ex);
}
```

---

## Phase D — Authorization

In `isched_GqlExecutor.cpp`, in `setup_builtin_resolvers()`, after the four plugin-related
`register_resolver(…)` calls:

```cpp
require_roles("plugins",      {std::string(Role::PLATFORM_ADMIN)});
require_roles("plugin",       {std::string(Role::PLATFORM_ADMIN)});
require_roles("loadPlugin",   {std::string(Role::PLATFORM_ADMIN)});
require_roles("unloadPlugin", {std::string(Role::PLATFORM_ADMIN)});
```

---

## Running Tests

```bash
cd build/release
ctest --output-on-failure -L plugin   # run plugin-tagged tests only
ctest --output-on-failure             # full suite — target: 53+ passing
```

---

## Coverage Check

```bash
# (Requires a coverage build preset — see CMakePresets.json)
cmake --preset conan-coverage -S . -B build/coverage
cmake --build build/coverage --parallel
cd build/coverage && ctest
# Check lcov / gcovr report for isched_PluginSystem.cpp ≥ 80% line + branch
```

