FROM ubuntu:20.04

RUN apt-get update && apt-get upgrade \
    && apt-get -y install build-essential sudo \
    && echo 'tzdata tzdata/Areas select Europe' | debconf-set-selections \
    && echo 'tzdata tzdata/Zones/Europe select Berlin' | debconf-set-selections \
    && DEBIAN_FRONTEND="noninteractive" apt install -y tzdata

RUN apt-get -y install python3 python3-pip python3-dev ninja-build git emacs wget build-essential \
    && pip install conan

RUN wget https://github.com/Kitware/CMake/releases/download/v3.28.1/cmake-3.28.1-Linux-x86_64.sh \
          -q -O /tmp/cmake-install.sh \
          && chmod u+x /tmp/cmake-install.sh \
          && mkdir /opt/cmake-3.28.1 \
          && /tmp/cmake-install.sh --skip-license --prefix=/opt/cmake-3.28.1 \
          && rm /tmp/cmake-install.sh \
          && ln -s /opt/cmake-3.28.1/bin/* /usr/local/bin

RUN groupadd -f conan-1001 -g 1001 \
        && groupadd -f conan-1000 -g 1000 \
        && groupadd -f conan-2000 -g 2000 \
        && groupadd -f conan-999 -g 999 \
        && useradd -ms /bin/bash conan -g 1001 -G 1000,2000,999 \
        && printf "conan:conan" | chpasswd \
        && adduser conan sudo \
        && printf "conan ALL= NOPASSWD: ALL\\n" >> /etc/sudoers \
        && mkdir -p -v /home/conan/.conan2/profiles/ && chown -R conan: /home/conan

RUN mkdir -p /results  \
    && chown -R conan /results  \
    && chmod 777 /results

USER conan
WORKDIR /home/conan

RUN git clone --recursive https://github.com/gogoba/isched.git

COPY default /home/conan/.conan2/profiles/default

RUN pip install conan && . ~/.profile \
        && cd isched && python3 configure.py

RUN cp /home/conan/isched/cmake-build-debug/src/test/cpp/isched/rest_hello_world /results

# Put your own Dockfile commands here
CMD ["/bin/bash"]
