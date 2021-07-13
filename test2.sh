#!/bin/bash

# parts taken from https://github.com/riscv/riscv-gnu-toolchain/blob/master/Makefile.in

set -e
set -x

mkdir big_tests
pushd big_tests

wget "https://ftp.gnu.org/gnu/binutils/binutils-2.35.2.tar.xz"
wget "https://ftp.gnu.org/gnu/binutils/binutils-2.35.2.tar.xz.sig"

wget "https://ftp.gnu.org/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.xz"
wget "https://ftp.gnu.org/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.xz.sig"

wget "https://musl.libc.org/releases/musl-1.2.2.tar.gz"
wget "https://musl.libc.org/releases/musl-1.2.2.tar.gz.asc"

sha256sum --check ../downloads.sha256sum

tar -xvf binutils-2.35.2.tar.xz

tar -xvf gcc-10.2.0.tar.xz

tar -xvf musl-1.2.2.tar.gz

SYSROOT="$PWD/sysroot" # XXX: symlink to build musl libc before running this script
PATH="${SYSROOT}/bin:$PATH"
export PATH

mkdir binutils-build
pushd binutils-build

../binutils-2.35.2/configure \
    --target="riscv64-linux-gnu" \
    --host="x86_64-linux-gnu" \
    --build="x86_64-linux-gnu" \
    --prefix="${SYSROOT}" \
    --disable-gdb \
    --disable-readline \
    --with-system-zlib
popd

make -C binutils-build -j3 --output-sync all
make -C binutils-build -j3 --output-sync install

# Building in the source directory is buggy / does not work
mkdir gcc-build
pushd gcc-build

# You can use this to also build mpcr, etc.. if you don't have them installed
#./contrib/download_prerequisites

mkdir -p "${SYSROOT}"

# checked by make all-gcc
mkdir -p "${SYSROOT}/include"

# Mostly taken from myunix2 mk/toolchain.mk:
# --enable-multilib: build different libgcc to support -march (i think)
# --with-sysroot: gcc needs headers from the target ._:
../gcc-10.2.0/configure \
    --enable-languages=c \
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
    --build="x86_64-linux-gnu" \
    --host="x86_64-linux-gnu" \
    --target="riscv64-linux-gnu" \
    --prefix="${SYSROOT}" \
    --disable-nls \
    --with-system-zlib \
    --disable-threads \
    --with-sysroot="${SYSROOT}" \
    --with-native-system-header-dir="/include"

popd

# build riscv64-linux-gnu-gcc cross compiler
make -C gcc-build -j3 --output-sync all-gcc
make -C gcc-build --output-sync install-gcc

# configure musl libc
pushd musl-1.2.2
CROSS_COMPILE="riscv64-linux-gnu-" \
    CFLAGS="-march=rv64iac" \
    ./configure \
        --target=riscv64-linux-gnu \
        --disable-shared \
        --prefix="${SYSROOT}"
popd

# make -C musl-1.2.2 -j3 --output-sync install-headers
make -C musl-1.2.2 -j3 --output-sync all
make -C musl-1.2.2 -j3 --output-sync install

# build libgcc using installed headers
make -C gcc-build -j3 --output-sync all-target-libgcc
make -C gcc-build -j3 --output-sync install-target-libgcc

# finish musl libc
#make -C musl-1.2.2 -j3 --output-sync all
#make -C musl-1.2.2 -j3 --output-sync install

popd
