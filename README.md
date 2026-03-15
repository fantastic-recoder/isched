# Isched
Project [Isched](https://de.wikipedia.org/wiki/Isched-Baum) is a high performance GraphQL server suited for massive 
parallel operation running from cloud server hardware down to embedded hardware.

## Dependencies
### Direct dependencies
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) — HTTP and WebSocket transport
- [nlohmann/json](https://github.com/nlohmann/json) — JSON serialisation
- [spdlog](https://github.com/gabime/spdlog) — structured logging
- [Catch2](https://github.com/catchorg/Catch2) — test framework
- [SQLite3](https://www.sqlite.org/) — per-tenant embedded database
- [jwt-cpp](https://github.com/Thalhammer/jwt-cpp) — JWT authentication
- [Boost.URL / Boost.Asio / Boost.Beast](https://www.boost.org/) — URL parsing and WebSocket utilities
- [taocpp-pegtl](https://github.com/taocpp/PEGTL) — GraphQL grammar parsing

## Documentation

### Doxygen Documentation Specification
This project follows comprehensive Doxygen documentation standards to ensure high-quality, maintainable code. All public APIs, classes, and functions must include detailed Doxygen comments following these conventions:

- **Class Documentation**: All classes must include `@brief` descriptions, detailed explanations, usage examples, and `@author` information
- **Function Documentation**: All public functions require `@brief` descriptions, `@param` documentation for each parameter, `@return` descriptions for non-void functions, and `@throw` specifications for exception-throwing functions
- **Code Examples**: Complex APIs should include `@code` blocks demonstrating proper usage patterns
- **Cross-References**: Use `@see`, `@ref`, and `@link` tags to create comprehensive documentation cross-references
- **Version Information**: Critical components should include `@since` and `@version` tags for API evolution tracking
- **GraphQL Grammar**: See [GraphQL Grammar Protocol](docs/isched_gql_grammar_readme.md) for details on the grammar implementation, spec compliance, and AST tools.
- **GraphQL Directives**: See [Using Directives](docs/guides/directives.md) for a guide on using and defining directives.

The documentation is generated using Doxygen and follows C++ Core Guidelines documentation practices. This ensures that all code is self-documenting and provides clear guidance for developers working with the isched Universal Application Server Backend.

#### Generating Documentation
To generate the complete Doxygen documentation, run the following command from the project root:

```bash
doxygen Doxyfile
```

This will create HTML documentation in the `docs/api/html/` directory. Open `docs/api/html/index.html` in your browser to view the complete API documentation.

You can also build the documentation automatically as part of the build process:

```bash
cmake --build cmake-build-debug --target docs
```

## Building
- Get decent C++ compiler
- Get python3
- Get conan 2.0
- Get Ninja
- Run the ./configure.py script

Build on Ubuntu with Docker
---------------------------

Make sure you have [Docker installed](https://docs.docker.com/engine/install/) and
[git](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git) on your system.

```bash
git clone --recursive https://github.com/gogoba/isched.git
cd isched/src/docker
docker build -t isched .
docker run --name isched001 --rm -p 8080:8080 -t isched:latest
```

Once running, send GraphQL queries over HTTP:
```bash
curl -s -X POST http://localhost:8080/graphql \
  -H 'Content-Type: application/json' \
  -d '{"query": "{ hello }"}'
```

Build on Ubuntu 20.04
---------------------

Please see the RUN commands in the [Dokefile](src/docker/Dockerfile).

Build with CLion
----------------

When building with **CLion** You start on command line and on **CLion**
start fill-in in the [CMake options dialogue](doc/clion_cmake_options.png)
the CMake options used in the [configure.py script](configure.py). The line
is marked with commentary ```# CMake options after "cmake```. 

## Security

Isched enforces security through static analysis as part of the development workflow.

### Static Analysis — `security_scan` target

A dedicated CMake target runs [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) with a security-focused check set over all library sources:

```bash
cmake --build ./cmake-build-debug/ --target security_scan
```

Checks enabled: `cert-*` (CERT C/C++ Secure Coding Standard), `bugprone-*`, `cppcoreguidelines-*`, and `clang-analyzer-security.*`.

The check configuration lives in [.clang-tidy](.clang-tidy) at the repository root. Each suppressed check is documented inline with its rationale (C API compatibility, false positives, intentional design decisions).

**Prerequisites** (Ubuntu/Debian):

```bash
sudo apt install clang-tidy
```

The target is configured automatically during CMake configure if `clang-tidy` is found on `PATH`; it is silently skipped when the tool is absent so that CI environments without the tool are not broken.

## Usefull links
- [Doxygen](https://www.doxygen.nl/index.html)
- [Conan](https://conan.io/)
- [Ninja](https://ninja-build.org/)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib)
- [CppCoreGuidelines](https://github.com/isocpp/CppCoreGuidelines)
- [CMake](https://cmake.org/)
- [Docker](https://www.docker.com/)
- [Git](https://git-scm.com/)
- [Python](https://www.python.org/)
- [CLion](https://www.jetbrains.com/clion/)
- [Dot Viewer](https://dreampuf.github.io/GraphvizOnline/?engine=dot)

---

## Comparable Benchmark

`tools/comparable_benchmark/` is a TypeScript developer tool that benchmarks isched
side-by-side against an equivalent Apollo Server 4 reference implementation and emits a
Markdown results table.

### Prerequisites

| Tool | Minimum version |
|------|----------------|
| Node.js | 22 |
| pnpm | 10 |
| isched binary | built (via `python3 configure.py`) — binary at `cmake-build-debug/src/main/cpp/isched/isched_srv` |

### Build isched

```bash
python3 configure.py
```

### Install Node dependencies (one-time)

```bash
pnpm install
```

### Run the full benchmark suite

```bash
pnpm run benchmark:compare
```

The harness:
1. Starts the Apollo reference server on port 18100 (HTTP + WS).
2. Starts the compiled isched binary on port 18092 (HTTP) / 18093 (WS).
3. Runs four benchmark scenarios against each server.
4. Writes JSON result files to `tools/comparable_benchmark/results/` (git-ignored).
5. Overwrites `docs/comparable-benchmark-results.md` with a Markdown comparison table.
6. Exits non-zero if isched is more than 10 % slower than Apollo on HTTP throughput scenarios.

### Override the isched binary path

```bash
ISCHED_BINARY=cmake-build-debug/src/main/cpp/isched/isched_srv pnpm run benchmark:compare
```

### Validate setup without running benchmarks

```bash
pnpm run benchmark:compare -- --dry-run
```

Checks that the binary exists and all required ports (18100, 18092, 18093) are free, then exits 0.

### Re-generate the report from existing JSON results

```bash
pnpm --filter comparable_benchmark run report
```

### Adjust the regression threshold

```bash
REGRESSION_THRESHOLD_PCT=20 pnpm run benchmark:compare
```

Default is `10` (%).  Set to `0` to make the threshold check purely informational.

### Interpreting the output table

| Column | Meaning |
|--------|---------|
| Apollo req/s | Requests per second for the Apollo reference server |
| isched req/s | Requests per second for isched |
| isched / Apollo ratio | Values > 1 mean isched is faster; < 1 means Apollo is faster |
| p95 isched (ms) | 95th-percentile latency for isched HTTP (or WS fan-out elapsed ms) |
