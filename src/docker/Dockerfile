FROM ubuntu:20.04
# Put your own Dockfile commands here

RUN apt-get update \
    && apt-get -y install build-essential \
    && echo 'tzdata tzdata/Areas select Europe' | debconf-set-selections \
    && echo 'tzdata tzdata/Zones/Europe select Berlin' | debconf-set-selections \
    && DEBIAN_FRONTEND="noninteractive" apt install -y tzdata

RUN apt-get -y install python3 python3-pip python3-dev cmake ninja-build git \
    && pip install conan2 pandas matplotlib openpyxl

RUN groupadd -f conan-1001 -g 1001 \
        && groupadd -f conan-1000 -g 1000 \
        && groupadd -f conan-2000 -g 2000 \
        && groupadd -f conan-999 -g 999 \
        && useradd -ms /bin/bash conan -g 1001 -G 1000,2000,999 \
        && printf "conan:conan" | chpasswd \
        && adduser conan sudo \
        && printf "conan ALL= NOPASSWD: ALL\\n" >> /etc/sudoers

RUN git clone --recursive https://github.com/gogoba/isched.git \
    && cd isched \
    && python3 configure.py \

RUN cd isched/cmake-build-debug \
    && ninja build \
    && ninja test

USER conan
WORKDIR /home/conan

# Put your own Dockfile commands here
CMD ["/bin/bash"]
