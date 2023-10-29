FROM ubuntu:20.04
# Put your own Dockfile commands here

RUN apt-get update && apt-get upgrade \
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
        && printf "conan ALL= NOPASSWD: ALL\\n" >> /etc/sudoers \
        && mkdir -p -v /home/conan/.conan2/profiles/ && chown -R conan: /home/conan


USER conan
WORKDIR /home/conan


RUN git clone --recursive https://github.com/gogoba/isched.git

COPY default /home/conan/.conan2/profiles/default

RUN pip install conan && . ~/.profile \
        && cd isched && python3 configure.py

#RUN cd isched/cmake-build-debug && ninja build && ninja test

# Put your own Dockfile commands here
CMD ["/bin/bash","-i"]
