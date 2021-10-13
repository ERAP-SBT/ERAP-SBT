# This Dockerfile builds a container that is very close to time-x
# just to avoid any trouble that could come from building on a different system

FROM ubuntu:20.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        meson \
        libmpfr-dev libmpc-dev zlib1g zlib1g-dev \
        gawk bison \
        linux-libc-dev-riscv64-cross \
        wget ca-certificates \
        git \
        autoconf automake autotools-dev curl python3 libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev \
    && rm -rf /var/lib/apt/lists/*
