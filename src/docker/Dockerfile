FROM ubuntu:20.04
WORKDIR ../../../docker-build
# Put your own Dockfile commands here

RUN apt-get update \
    && apt-get -y install build-essential \
    && echo 'tzdata tzdata/Areas select Europe' | debconf-set-selections \
    && echo 'tzdata tzdata/Zones/Europe select Berlin' | debconf-set-selections \
    && DEBIAN_FRONTEND="noninteractive" apt install -y tzdata
RUN apt-get -y install python3 python3-pip python3-dev cmake ninja-build git \
    && pip install conan2 pandas matplotlib openpyxl
RUN git clone --recursive https://github.com/gogoba/isched.git \
    && cd isched \
    && python3 configure.py
RUN cd isched/cmake-build-debug \
    && ninja build \
    && ninja test
# Put your own Dockfile commands here
CMD ["/bin/bash"]
