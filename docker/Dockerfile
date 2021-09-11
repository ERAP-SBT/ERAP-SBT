FROM debian:unstable

# We want meson >=57.2 for clang-format-check
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -qq -y \
        wget \
        build-essential \
        clang-format \
        git \
        gcovr \
        g++-riscv64-linux-gnu \
        meson>=57.2 \
        libmpfr-dev \
        libmpc-dev \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

COPY setup_toolchain.sh downloads.sha256sum /
RUN /setup_toolchain.sh && rm -f setup_toolchain.sh downloads.sha256sum

# Add link for convinience
RUN ln -s /toolchain/sysroot/bin/riscv64-linux-gnu-gcc /toolchain/gcc
