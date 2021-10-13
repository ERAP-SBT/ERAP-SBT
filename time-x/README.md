# Benchmarks

This directory contains all information necessary to run benchmarks on time-x.

The path `/u/home/hettwer` may need to be replaced with your own home directory path on time-x.

## Building the time-x-base container

```bash
podman build -t time-x-base --network=host base-container
```

## Building the toolchains

```bash
# Setup

git clone "https://github.com/riscv/riscv-gnu-toolchain.git"
rm -rf build-rv64g build-rv64iamc && mkdir build-rv64g build-rv64iamc

# Configure

podman run --init --rm -it --network=host --userns=keep-id -v "$PWD:/u/home/hettwer:rw" -w "/u/home/hettwer/build-rv64g" time-x-base \
    ../riscv-gnu-toolchain/configure --with-arch=rv64imafdc --with-abi=lp64d --prefix="/u/home/hettwer/toolchains/rv64g"
podman run --init --rm -it --network=host --userns=keep-id -v "$PWD:/u/home/hettwer:rw" -w "/u/home/hettwer/build-rv64iamc" time-x-base \
    ../riscv-gnu-toolchain/configure --with-arch=rv64imac --with-abi=lp64 --prefix="/u/home/hettwer/toolchains/rv64iamc"

# Building

podman run --init --rm -it --network=host --userns=keep-id -v "$PWD:/u/home/hettwer:rw" -w "/u/home/hettwer/build-rv64g" time-x-base \
    make --output-sync=target -j"$(nproc)" linux
podman run --init --rm -it --network=host --userns=keep-id -v "$PWD:/u/home/hettwer:rw" -w "/u/home/hettwer/build-rv64iamc" time-x-base \
    make --output-sync=target -j"$(nproc)" linux

# Uploading to time-x

rsync -e 'ssh -J hettwer@login.caps.in.tum.de' -va "$PWD/toolchains" hettwer@time-x.caps.in.tum.de:
```

## Installing qemu-user (on time-x)

```bash
apt-get download qemu-user-static
dpkg-deb --extract qemu-user-static_1%3a4.2-3ubuntu6.17_amd64.deb /u/home/hettwer/qemu-user-static
```

## Installing the benchmark suite (on time-x)

Note: Before running things on time-x, make sure nobody else is using it (`w` command shows logged in sessions)

```bash
# -d: destination
# -u: toolset to use
# -f: preform install without questions
/opt/cpu2017-1.1.0/install.sh -d "$HOME/cpu2017" -u "linux-x86_64" -f
cd "$HOME/cpu2017"
source shrc
```

## Configure the benchmark

The `config` directory contains a few configurations for running benchmarks:

* `Example-gcc-linux-x86.cfg`: The Example provided by SPEC CPU 2017, the base for all other configurations
* `native.cfg`: Derived from `Example-gcc-linux-x86.cfg`, with additional fixes for gcc compatibility provided by Alexi Engelke
* `qemu-user.cfg`: Derived from `native.cfg`: cross compiles to rv64g and uses qemu-user to perform the benchmarks

```bash
rsync -e 'ssh -J hettwer@login.caps.in.tum.de' -va "$PWD/config/" hettwer@time-x.caps.in.tum.de:/u/home/hettwer/cpu2017/config/

# runcpu.cross is required for cross compiling benchmarks
rsync -e 'ssh -J hettwer@login.caps.in.tum.de' -va "$PWD/runcpu.cross" hettwer@time-x.caps.in.tum.de:/u/home/hettwer/cpu2017/
```

## Running regression tests

```bash
# Cleanup old left overs

# Build benchmarks

# Run benchmarks with size=test
```

## Running the benchmarks


### Native

```bash
runcpu --iterations=2 --size=refspeed --reportable --config native --action=run intspeed
```

### QEMU-user

The tests don't work well with `runcpu.cross`, so we can't use `--reportable`

```bash
runcpu --iterations=2 --size=refspeed --noreportable --config qemu-user --action=run intspeed
```

## Results

The `results` directory contains the results for all benchmarks referenced in the Ausarbeitung.
