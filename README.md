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
