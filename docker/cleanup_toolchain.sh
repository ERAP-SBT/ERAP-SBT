#!/bin/bash

pushd toolchain

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
