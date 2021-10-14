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

## Building eragp-sbt-2021

```bash
# Setup
git clone "https://gitlab.lrz.de/lrr-tum/students/eragp-sbt-2021.git"
rm -rf build-sbt

# Configure
podman run --init --rm -it --network=host --userns=keep-id -v "$PWD:/u/home/hettwer:rw" -w "/u/home/hettwer" time-x-base \
    meson setup build-sbt eragp-sbt-2021 -Dprefix=/u/home/hettwer/sbt -Dbuildtype=release

# Building
# --verbose: verify cppflags used
podman run --init --rm -it --network=host --userns=keep-id -v "$PWD:/u/home/hettwer:rw" -w "/u/home/hettwer" time-x-base \
    ninja -C build-sbt --verbose install

# Uploading to time-x
rsync -e 'ssh -J hettwer@login.caps.in.tum.de' -va "$PWD/sbt" hettwer@time-x.caps.in.tum.de:
```

## Building ria-jit

```bash
# Setup
git clone --recurse-submodules https://github.com/aengelke/ria-jit.git
git -C ria-jit switch cm2 --recurse-submodules
rm -rf build-ria-jit && mkdir build-ria-jit

# Configure
podman run --init --rm -it --network=host --userns=keep-id -v "$PWD:/u/home/hettwer:rw" -w "/u/home/hettwer/" time-x-base \
    cmake -S ria-jit -B build-ria-jit -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=true

# Building (use same options as ria-jit paper)
podman run --init --rm -it --network=host --userns=keep-id -v "$PWD:/u/home/hettwer:rw" -w "/u/home/hettwer/build-ria-jit" time-x-base \
    make --output-sync=target -j"$(nproc)"

# Uploading to time-x
rsync -e 'ssh -J hettwer@login.caps.in.tum.de' -va "$PWD/build-ria-jit/translator" hettwer@time-x.caps.in.tum.de:ria-jit/translator
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
# runcpu.cross-ria-jit is required for ria-jit benchmarks
rsync -e 'ssh -J hettwer@login.caps.in.tum.de' -va "$PWD/runcpu.cross" "$PWD/runcpu.cross-ria-jit" hettwer@time-x.caps.in.tum.de:/u/home/hettwer/cpu2017/
```

## Running regression tests

```bash
# Run benchmarks with size=test
runcpu --copies=1 --iterations=1 --size=test --noreportable --ignore_errors --action=run --config=translator-test.cfg --action=run intspeed
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

### RIA-JIT

The tests don't work, because perlbench is missing the clone syscall.
625.x264 fails due to floating point operations, so it was repeated with a softfp toolchain

```bash
runcpu --iterations=2 --size=refspeed --noreportable --config ria-jit --action=run intspeed
runcpu --iterations=2 --size=refspeed --noreportable --config ria-jit-nofloats --action=run 625.x264_s
```

### SBT

`no_trans_bbs` makes things incredibly slow.

```bash
runcpu --ignore_errors --iterations=2 --size=refspeed --noreportable --ignore_errors --config sbt-optimize-all-notransbbs --action=run intspeed

# compare effectivness of optimizations
runcpu --ignore_errors --iterations=2 --size=refspeed --noreportable --ignore_errors --config sbt-optimize-reg_alloc-no_hash_lookup --action=run intspeed
runcpu --ignore_errors --iterations=2 --size=refspeed --noreportable --config sbt-optimize-reg_alloc-no_hash_lookup-plus-lifter --action=run intspeed
runcpu --ignore_errors --iterations=2 --size=refspeed --noreportable --config sbt-optimize-reg_alloc-no_hash_lookup-plus-gen --action=run intspeed
runcpu --ignore_errors --iterations=2 --size=refspeed --noreportable --config sbt-optimize-reg_alloc-no_hash_lookup-plus-ir --action=run intspeed

# for effect of no_trans_bbs, use the fastest benchmark
runcpu --ignore_errors --iterations=2 --size=refspeed --noreportable --config=sbt-optimize-all --action=run 623.xalancbmk_s
# for effect of no_hash_lookup, but also with !no_trans_bbs to allow comparrison
runcpu --ignore_errors --iterations=2 --size=refspeed --noreportable --config=sbt-optimize-all-with-hash-table --action=run 623.xalancbmk_s
```

## Results

The `results` directory contains the results for all benchmarks referenced in the Ausarbeitung.
