FROM ubuntu:20.04
WORKDIR ../../../docker-build
# Put your own Dockfile commands here

RUN apt-get update \
    && apt-get -y install build-essential \
    && echo 'tzdata tzdata/Areas select Europe' | debconf-set-selections \
    && echo 'tzdata tzdata/Zones/Europe select Berlin' | debconf-set-selections \
    && DEBIAN_FRONTEND="noninteractive" apt install -y tzdata
RUN apt-get -y install python3 python3-pip python3-dev cmake ninja-build git \
    && python3 -m pip install conan2 \
    && git clone --recursive https://github.com/gogoba/isched.git \
    && cd isched \
    && python3 configure.py



# Put your own Dockfile commands here
CMD ["/bin/bash"]
