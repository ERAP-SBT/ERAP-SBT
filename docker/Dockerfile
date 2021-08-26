FROM debian:unstable

WORKDIR /

# We want meson >=57.2 for clang-format-check, but currently it is only available in experimental
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=988219
RUN echo "deb http://deb.debian.org/debian/ experimental main" > /etc/apt/sources.list.d/experimental.list
# Install build-tools
RUN apt-get update && apt-get install -y wget build-essential clang-format git gcovr g++-riscv64-linux-gnu && apt-get install -y -t experimental meson
# Install build-tools for toolchain
RUN apt-get install -y libmpfr-dev libmpc-dev zlib1g zlib1g-dev

# Setup Script for Toolchain
ADD setup_toolchain.sh /
ADD downloads.sha256sum /
ADD cleanup_toolchain.sh /
RUN chmod +x /setup_toolchain.sh && chmod +x /cleanup_toolchain.sh && bash -c /setup_toolchain.sh && bash -c /cleanup_toolchain.sh
# Add link for convinience
RUN ln -s /toolchain/sysroot/bin/riscv64-linux-gnu-gcc /toolchain/gcc
