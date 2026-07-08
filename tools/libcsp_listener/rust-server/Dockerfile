FROM rust:1-slim-bullseye AS base

RUN apt-get update && apt-get install -y \
    build-essential \
    libclang-dev \
    python3 \
    python3-pip \
    pkg-config \
    git \
    autoconf \
    automake \
    libtool \
    libzmq3-dev \
    && rm -rf /var/lib/apt/lists/*

RUN rustup component add rustfmt
RUN rustup component add clippy

CMD ["/bin/bash"]
