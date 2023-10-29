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

- Build Isched
```bash
git clone --recursive https://github.com/gogoba/isched.git
cd isched/src/docker
docker build -t isched .
docker run -i isched
```
