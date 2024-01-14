# Isched
Project [Isched](https://de.wikipedia.org/wiki/Isched-Baum) is a high performance GraphQL server suited for massive 
parallel operation running from cloud server hardware down to embedded hardware.

## Dependencies
### Direct dependencies
- [Restbed](https://github.com/Corvusoft/restbed)
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

