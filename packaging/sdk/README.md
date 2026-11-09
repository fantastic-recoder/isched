# isched C++ SDK dependencies

`isched-devel` installs `libisched.a`, the supported `isched/...` public header
closure, and a relocatable CMake package that exports `isched::isched`. It does
not include or require `isched-runtime`; SDK consumers can use the library
without installing the server executable package.

The archive is static and is not self-contained. Consumers resolve the exact
Conan dependency graph described by `conanfile.txt` and `conan.lock` with a
compiler and standard-library ABI compatible with the target package. The
manifest deliberately excludes Catch2 and producer-only CMake/Ninja tool
requirements. `requirements-conan.lock` pins the isolated Conan 2.34.0 CLI
environment; create a virtual environment rather than changing system Python.
The static archive's Conan link closure is PEGTL, cpp-httplib, OpenSSL,
spdlog/fmt, nlohmann_json, jwt-cpp, SQLite, Boost URL/system/thread/
program_options/filesystem, and platformfolders. `conan.lock` records their
resolved transitive recipes (including compression/backtrace dependencies);
the target toolchain supplies its platform loader and thread libraries.

## Configure a consumer

Install only `isched-devel` using the target's native package manager. In the
consumer project, provision the pinned library dependencies with the shipped
Conan tool lock and generate CMakeDeps files in the consumer's own build
directory. Use the matching shipped target profile for the package (`ubuntu-26.04` or
`fedora-45`) or a separately reviewed profile that matches the compiler and
ABI used to build the package:

```sh
python3 -m venv .venv/isched-sdk
.venv/isched-sdk/bin/python -m pip install --require-hashes \
  -r /usr/share/isched/developer/requirements-conan.lock
.venv/isched-sdk/bin/conan profile detect --force
.venv/isched-sdk/bin/conan install /usr/share/isched/developer/conanfile.txt \
  --lockfile=/usr/share/isched/developer/conan.lock \
  --profile:host=/usr/share/isched/developer/profiles/ubuntu-26.04 \
  --profile:build=default \
  --output-folder=build/conan --build=missing
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake \
  -DCMAKE_PREFIX_PATH=build/conan
cmake --build build
```

Select the Fedora profile on Fedora 45. The shipped profiles identify the
producer ABI; they do not install or activate compilers for the consumer. Use
the target distribution's compiler packages and the package's documented
Conan profile. `find_package(isched CONFIG REQUIRED)` locates the installed
package through the system prefix, and downstream code links `isched::isched`
and includes `<isched/isched.hpp>`.

Do not add `isched-runtime` as a dependency to resolve static-library link
requirements. Runtime shared-object requirements belong to the separate
runtime package. Package identity and license/third-party notices are installed
alongside the SDK files; review those notices for redistribution obligations.
SDK notices are under `/usr/share/doc/isched-devel/`. The static SDK may have
no native package dependencies: its third-party link dependencies are resolved
through the shipped Conan inputs, not an invented dependency on the runtime.
