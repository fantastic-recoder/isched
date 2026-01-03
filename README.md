# Isched
Project [Isched](https://de.wikipedia.org/wiki/Isched-Baum) is a high performance GraphQL server suited for massive 
parallel operation running from cloud server hardware down to embedded hardware.

## Dependencies
### Direct dependencies
- [Restbed](https://github.com/Corvusoft/restbed)

## Documentation

### Doxygen Documentation Specification
This project follows comprehensive Doxygen documentation standards to ensure high-quality, maintainable code. All public APIs, classes, and functions must include detailed Doxygen comments following these conventions:

- **Class Documentation**: All classes must include `@brief` descriptions, detailed explanations, usage examples, and `@author` information
- **Function Documentation**: All public functions require `@brief` descriptions, `@param` documentation for each parameter, `@return` descriptions for non-void functions, and `@throw` specifications for exception-throwing functions
- **Code Examples**: Complex APIs should include `@code` blocks demonstrating proper usage patterns
- **Cross-References**: Use `@see`, `@ref`, and `@link` tags to create comprehensive documentation cross-references
- **Version Information**: Critical components should include `@since` and `@version` tags for API evolution tracking
- **GraphQL Grammar**: See [GraphQL Grammar Protocol](docs/isched_gql_grammar_readme.md) for details on the grammar implementation, spec compliance, and AST tools.

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

- Build Isched via Docker
```bash
git clone --recursive https://github.com/gogoba/isched.git
cd isched/src/docker
docker build -t isched .
docker run -d --name isched001 --rm -v isched:/opt/isched -p 1980 -t isched:latest
docker cp isched001:/opt/isched/rest_hello_world .
docker cp isched001:/opt/isched/isched_srv .
docker stop isched001
```
Now we can run the resulting binary and send to it a message with curl:
```bash
./rest_hello_world & 
curl  --data Groby localhost:1984/resource
```
- or you can run isched server out of container:
```
docker run --name isched001 --rm -v isched:/opt/isched -t isched:latest /opt/isched/isched_srv
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

## Usefull links
- [Doxygen](https://www.doxygen.nl/index.html)
- [Conan](https://conan.io/)
- [Ninja](https://ninja-build.org/)
- [Restbed](https://github.com/Corvusoft/restbed)
- [CppCoreGuidelines](https://github.com/isocpp/CppCoreGuidelines)
- [CMake](https://cmake.org/)
- [Docker](https://www.docker.com/)
- [Git](https://git-scm.com/)
- [Python](https://www.python.org/)
- [CLion](https://www.jetbrains.com/clion/)
- [Dot Viewer](https://dreampuf.github.io/GraphvizOnline/?engine=dot)
