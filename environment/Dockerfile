FROM ubuntu:22.04
ARG DEBIAN_FRONTEND=noninteractive

USER root

RUN apt-get update && \
    apt-get -y upgrade && \
    apt-get -y --no-install-recommends install \
        build-essential \
        git \
        python3 \
        python3-pip \
        vim \
        wget \
        can-utils \
        gdb-multiarch \
        graphviz \
        gcc-multilib \
        g++-multilib \
        can-utils \
        openocd \
        valgrind \
        libncurses5 \
        libncurses5-dev \
        cmake \
        libsocketcan-dev \
        pkgconf \
        libzmq3-dev \
        gdb && \
    apt-get -y clean

RUN git config --global --add safe.directory /workspace

RUN ln -s /usr/bin/python3 /usr/bin/python

RUN wget https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi.tar.xz && \
    tar -xf arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi.tar.xz && \
    rm arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi.tar.xz
ENV PATH="/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/bin:${PATH}"

# Install Renode
ARG RENODE_VERSION=1.16.0
RUN wget https://github.com/renode/renode/releases/download/v${RENODE_VERSION}/renode_${RENODE_VERSION}_amd64.deb && \
    apt-get update && \
    apt-get install -y --no-install-recommends ./renode_${RENODE_VERSION}_amd64.deb python3-dev && \
    rm ./renode_${RENODE_VERSION}_amd64.deb && \
    rm -rf /var/lib/apt/lists/*
RUN pip3 install -r /opt/renode/tests/requirements.txt --no-cache-dir

CMD ["/bin/bash"]

WORKDIR /workspace/
