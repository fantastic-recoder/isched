# SP-012 Coverage Report (T068)

Date: 2026-05-25

## Coverage build and test commands

```bash
cd /home/groby/dev/isched
conan install . -of build/coverage -s build_type=Debug --build=missing
cmake -S . -B build/coverage -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/home/groby/dev/isched/build/coverage/conan_toolchain.cmake \
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='--coverage -O0 -g' \
  -DCMAKE_CXX_FLAGS='--coverage -O0 -g' \
  -DCMAKE_EXE_LINKER_FLAGS='--coverage' \
  -DCMAKE_SHARED_LINKER_FLAGS='--coverage'
cmake --build build/coverage --parallel --target \
  isched_plugin_system_tests \
  isched_gql_executor_auth_resolvers_tests \
  isched_plugin_system_subscription_integration_tests
cd build/coverage
ctest --output-on-failure -R "isched_plugin_system_tests|isched_gql_executor_auth_resolvers_tests|isched_plugin_system_subscription_integration_tests"
```

## Coverage measurement command

```bash
cd /home/groby/dev/isched
gcovr -r /home/groby/dev/isched build/coverage \
  --filter 'src/main/cpp/isched/backend/isched_PluginSystem.cpp' \
  --filter 'src/test/cpp/isched/isched_plugin_system.*\.cpp' \
  --txt
```

## Coverage results

| Scope | Lines | Executed | Coverage |
|---|---:|---:|---:|
| `src/main/cpp/isched/backend/isched_PluginSystem.cpp` | 748 | 550 | 73% |
| `src/test/cpp/isched/isched_plugin_system_subscription_integration_tests.cpp` | 154 | 151 | 98% |
| `src/test/cpp/isched/isched_plugin_system_tests.cpp` | 1173 | 1166 | 99% |
| **Scoped total** | **2075** | **1867** | **90%** |

## Gate statement

Scoped plugin-system coverage evidence is **90% line coverage**, satisfying the **>=80%** target for task T068.

