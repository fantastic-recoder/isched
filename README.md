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
docker run -d --name isched001 --rm -v results:/results -t isched:latest
docker cp isched001:/results/rest_hello_world .
docker stop isched001
```
Now we can run the resulting binary and send to it a message with curl:
```bash
./rest_hello_world & 
curl  --data Groby localhost:1984/resource
```
