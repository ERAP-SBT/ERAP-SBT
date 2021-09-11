#!/bin/bash

# You need the following dependencies:
# sudo apt-get install -y libmpfr-dev libmpc-dev zlib1g zlib1g-dev

set -e
set -x
set -u
set -o pipefail

mkdir toolchain
pushd toolchain

# Download dependencies

wget "https://ftp.gnu.org/gnu/binutils/binutils-2.35.2.tar.xz"
wget "https://ftp.gnu.org/gnu/binutils/binutils-2.35.2.tar.xz.sig"

wget "https://ftp.gnu.org/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.xz"
wget "https://ftp.gnu.org/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.xz.sig"

wget "https://musl.libc.org/releases/musl-1.2.2.tar.gz"
wget "https://musl.libc.org/releases/musl-1.2.2.tar.gz.asc"

# Verify checksums

sha256sum --check ../downloads.sha256sum

# Extract dependencies

tar -xvf binutils-2.35.2.tar.xz

tar -xvf gcc-10.2.0.tar.xz

tar -xvf musl-1.2.2.tar.gz

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

make -C binutils-build -j3 --output-sync all
make -C binutils-build -j3 --output-sync install

# Build gcc step 1

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
../gcc-10.2.0/configure \
    --host="${CONF_HOST}" \
    --build="${CONF_BUILD}" \
    --target="${CONF_TARGET}" \
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
    --prefix="${SYSROOT}" \
    --disable-nls \
    --with-system-zlib \
    --disable-threads \
    --with-sysroot="${SYSROOT}" \
    --with-native-system-header-dir="/include"

popd

# build riscv64-linux-gnu-gcc cross compiler
# Seperate make invocations because gccs Makefile is fragile
make -C gcc-build -j3 --output-sync all-gcc
make -C gcc-build --output-sync install-gcc

# Build musl-libc

pushd musl-1.2.2
CFLAGS="-march=rv64iac" \
./configure \
    --target=riscv64-linux-gnu \
    --disable-shared \
    --prefix="${SYSROOT}"
popd

make -C musl-1.2.2 -j3 --output-sync all
make -C musl-1.2.2 -j3 --output-sync install

# Build gcc step 2

# build libgcc using installed headers
make -C gcc-build -j3 --output-sync all-target-libgcc
make -C gcc-build -j3 --output-sync install-target-libgcc

## Cleanup

# Remove zips
rm binutils-2.35.2.tar.xz
rm binutils-2.35.2.tar.xz.sig
rm gcc-10.2.0.tar.xz
rm gcc-10.2.0.tar.xz.sig
rm musl-1.2.2.tar.gz
rm musl-1.2.2.tar.gz.asc

# Remove source dirs
rm -rf binutils-2.35.2
rm -rf gcc-10.2.0
rm -rf musl-1.2.2

# Remove build dirs
rm -rf binutils-build
rm -rf gcc-build

popd
