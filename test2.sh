#!/bin/bash
# You need the following dependencies:
# sudo apt-get install -y libmpfr-dev libmpc-dev zlib1g zlib1g-dev linux-libc-dev-riscv64-cross gawk bison

set -e
set -x
set -u
set -o pipefail

JOBS="-j$(nproc)"

mkdir toolchain
pushd toolchain

# Download dependencies

wget "https://ftp.gnu.org/gnu/binutils/binutils-2.35.2.tar.xz"

wget "https://ftp.gnu.org/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.xz"

#wget "https://musl.libc.org/releases/musl-1.2.2.tar.gz"

wget "https://ftp.gnu.org/gnu/libc/glibc-2.34.tar.xz"

# Verify checksums

sha256sum --check ../downloads.sha256sum

# Extract dependencies

tar -xvf binutils-2.35.2.tar.xz

tar -xvf gcc-10.2.0.tar.xz

#tar -xvf musl-1.2.2.tar.gz

tar -xvf glibc-2.34.tar.xz

SYSROOT="$PWD/sysroot" # XXX: symlink to build musl libc before running this script
PATH="${SYSROOT}/bin:$PATH"
export PATH

CONF_HOST="x86_64-linux-gnu"
CONF_BUILD="${CONF_HOST}"
CONF_TARGET="riscv64-linux-gnu"

# Build binutils

mkdir binutils-build
pushd binutils-build

../binutils-2.35.2/configure \
    --host="${CONF_HOST}" \
    --build="${CONF_BUILD}" \
    --target="${CONF_TARGET}" \
    --prefix="${SYSROOT}" \
    --disable-gdb \
    --disable-readline \
    --with-system-zlib
popd

make -C binutils-build ${JOBS} --output-sync all
make -C binutils-build ${JOBS} --output-sync install

# Build gcc step 1

# time-x doesn't have the gmp / mpfr / mpc / isl dependencies installed
pushd gcc-10.2.0
./contrib/download_prerequisites
popd

# Building in the source directory is buggy / does not work
mkdir gcc-build
pushd gcc-build

# You can use this to also build mpcr, etc.. if you don't have them installed
#./contrib/download_prerequisites

mkdir -p "${SYSROOT}"

# checked by make all-gcc
mkdir -p "${SYSROOT}/include"

# Mostly taken from myunix3 mk/toolchain.mk or gcc --verbose on debian
# --enable-multilib: build different libgcc to support -march (i think)
# --with-sysroot: gcc needs headers from the target:
#
# parts taken from https://github.com/riscv/riscv-gnu-toolchain/blob/master/Makefile.in
# other parts from myunix3
#
# We also specifc --with-abi / --with-arch to select the abi libgcc should use
# We build with support for C++ as it seems to be required for building glibc
../gcc-10.2.0/configure \
    --host="${CONF_HOST}" \
    --build="${CONF_BUILD}" \
    --target="${CONF_TARGET}" \
    --enable-languages=c,c++,fortran \
    --disable-shared \
    --disable-libitm \
    --disable-libquadmath \
    --disable-libquadmath-support \
    --enable-plugin \
    --enable-default-pie \
    --enable-multiarch \
    --disable-werror \
    --disable-multilib \
    --with-arch=rv64imafdc \
    --with-abi=lp64 \
    --prefix="${SYSROOT}" \
    --disable-nls \
    --with-system-zlib \
    --disable-threads \
    --with-sysroot="${SYSROOT}" \
    --with-native-system-header-dir="/include"

popd

# build riscv64-linux-gnu-gcc cross compiler
# Seperate make invocations because gccs Makefile is fragile
make -C gcc-build ${JOBS} --output-sync all-gcc
make -C gcc-build ${JOBS} --output-sync install-gcc

# NOTE: we cheat and use the kernel headers provided by linux-libc-dev-riscv64-cross, it works.
pushd sysroot
LINUX_HEADERS="${LINUX_HEADERS:-/usr/riscv64-linux-gnu/include}"
cp -vr "${LINUX_HEADERS}/linux" include/linux
cp -vr "${LINUX_HEADERS}/asm" include/asm
cp -vr "${LINUX_HEADERS}/asm-generic" include/asm-generic
popd

# Build glibc

mkdir glibc-build
pushd glibc-build
# -O3: glibc can't be build without optimizations
CFLAGS="-march=rv64iac -O3" \
../glibc-2.34/configure \
    --host=riscv64-linux-gnu \
    --prefix="${SYSROOT}"
popd

# install-others: installs the header <gnu/stubs.h> that is required by libgcc
# apparently an issue since 2003: https://gcc.gnu.org/legacy-ml/gcc-patches/2003-11/msg00612.html
make -C glibc-build ${JOBS} --output-sync install-headers
#"${SYSROOT}/include/gnu/stubs.h"
touch "${SYSROOT}/include/gnu/stubs.h"

# build libgcc using installed headers
make -C gcc-build ${JOBS} --output-sync all-target-libgcc
make -C gcc-build ${JOBS} --output-sync install-target-libgcc

# glibc depends on libgcc
make -C glibc-build ${JOBS}  --output-sync all
make -C glibc-build ${JOBS}  --output-sync install

popd
