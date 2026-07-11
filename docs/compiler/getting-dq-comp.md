# Getting the DQ Compiler

This page describes how to get `dq-comp` and `dq-run`.

## Release Packages

For normal use, prefer a release package from the DQ GitHub releases page:

```text
https://github.com/nvitya/dq-lang/releases
```

The release archive names include the DQ version. For version `0.32.6`, the
main packages are:

| Package | File name | Notes |
| --- | --- | --- |
| Linux full package | `dq-0.32.6-x86_64-linux-full.tar.gz` | Recommended Linux package. Includes DQ tools, `stdpkg`, examples, docs, autotests, bundled LLVM/Clang, runtime libraries, and a Linux link sysroot. |
| Linux light package | `dq-0.32.6-x86_64-linux.tar.gz` | Smaller package for systems that already have the native compiler/linker dependencies installed. |
| Windows UCRT package | `dq-0.32.6-x86_64-windows-ucrt.zip` | Windows package with the DQ tools and bundled Windows LLVM/MinGW runtime pieces. |

### Linux Full Package

For a normal Linux install, use the full package:

```bash
tar -xzf dq-0.32.6-x86_64-linux-full.tar.gz
cd dq-0.32.6-x86_64-linux-full
bin/dq-comp --version
bin/dq-run examples/basic/test1.dq
```

The full package is intended to work without installing compiler packages from
the operating system. It has been tested in clean Ubuntu 24.04, Ubuntu 26.04,
and Debian 13 containers.

You can also add the package tools to your shell environment:

```bash
. ./dq-env.sh
dq-comp --version
dq-run examples/basic/test1.dq
```

### Linux Light Package

The light Linux package is useful when the host system already has the native
toolchain dependencies. On a plain Ubuntu 24.04 system, install:

```bash
sudo apt-get update
sudo apt-get install -y clang lld g++ libc6-dev libstdc++-14-dev libgcc-14-dev
```

Then extract and run:

```bash
tar -xzf dq-0.32.6-x86_64-linux.tar.gz
cd dq-0.32.6-x86_64-linux
bin/dq-comp --version
bin/dq-run examples/basic/test1.dq
```

The light package depends on the host system for `clang++`, `lld`, glibc
startup files, the C/C++ linker libraries, GCC support libraries, and
`libbacktrace`.

After extracting a package or sourcing `dq-env.sh`, make sure the compiler
tools are available:

```bash
dq-comp --version
dq-run --help
```

## Building From Source

Build from source when there is no release package for your platform, or when
you want to work on the compiler itself.

### Requirements

You need:

| Tool | Notes |
| --- | --- |
| CMake 3.10 or newer | used to generate the native build files |
| GNU Make | used by the repository build wrapper |
| A C++23 compiler and standard library | **GCC 14 or newer**; needed for `std::print` and backtrace support |
| LLVM 21 development files | CMake must be able to find LLVM 21's `LLVMConfig.cmake` |
| LLVM support library headers | zlib, zstd, libedit, curl, libxml2, and libffi development packages |

The compiler is currently developed and tested with LLVM 21. Older LLVM
versions, including LLVM 18 and below, are not expected to work.

### Install GCC 14 or Newer

DQ uses C++23 standard-library features such as `std::print` and stacktrace
support for diagnostics and crash backtraces. Ubuntu 24.04 uses GCC 13 as the
default compiler, but GCC 13's standard library is too old for these features.
Check the selected compiler before configuring the build:

```bash
g++ --version
```

The first line should show version 14 or newer.

On Debian or Ubuntu, install GCC 14 with:

```bash
sudo apt update
sudo apt install gcc-14 g++-14
```

To make GCC 14 the default compiler for normal shell commands, register it with
`update-alternatives`:

```bash
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 140
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 140
sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-14 140
```

If more than one compiler version is registered, select GCC 14:

```bash
sudo update-alternatives --config gcc
sudo update-alternatives --config g++
sudo update-alternatives --config c++
```

Verify the selected versions again:

```bash
g++ --version
```

### Install LLVM 21

On Debian or Ubuntu systems, use the official LLVM APT packages from
`https://apt.llvm.org/`.

Install the LLVM APT helper script and request LLVM 21:

```bash
sudo apt install wget ca-certificates gnupg
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 21
```

Install the packages needed to build DQ:

```bash
sudo apt install \
  build-essential cmake llvm-21-dev clang-21 lld-21 \
  zlib1g-dev libzstd-dev libedit-dev libcurl4-openssl-dev \
  libxml2-dev libffi-dev
```

Verify that LLVM 21 is available:

```bash
/usr/lib/llvm-21/bin/llvm-config --version
```

The command should print a `21.x` version.

### Get The Source

Clone the DQ repository and enter the source tree:

```bash
git clone https://github.com/nvitya/dq-lang.git
cd dq-lang
```

### Configure

From the repository root, generate the build files:

```bash
cmake .
```

This creates the root `Makefile` used by the repository's `GNUmakefile`.
(See [Troubleshooting](#troubleshooting) if you experience problems here.)

### Build

Build the compiler and helper tools:

```bash
make -j"$(nproc)"
```

The host build places the user-facing tools in the repository's `build`
directory:

```text
build/dq-comp
build/dq-run
build/dqatrun
```

### Install

Install the built tools and standard packages:

```bash
sudo make install
```

The install target installs `dq-comp` and `dq-run` into `bin`, and installs the
standard DQ packages into `lib/dq/stdpkg` under the chosen CMake install prefix.
The default prefix is usually `/usr/local`.

To install under a different prefix, configure with `CMAKE_INSTALL_PREFIX`:

```bash
cmake . -DCMAKE_INSTALL_PREFIX="$HOME/.local"
make -j"$(nproc)"
make install
```

Then make sure the selected `bin` directory is on your `PATH`.

### Try The Compiler

Compile and run a single DQ file with the installed tools:

```bash
dq-run examples/console/hello_libc.dq
```

If you are working on your own file:

```bash
dq-run path/to/file.dq
```

`dq-run` uses `-g -O0` by default when no compiler options are supplied, which
is useful while developing or debugging.

### Troubleshooting

#### cmake .

If CMake finds another LLVM version, or cannot find LLVM at all, pass LLVM 21's
CMake package directory explicitly:

```bash
cmake . -DLLVM_DIR=/usr/lib/llvm-21/lib/cmake/llvm
```

To build with a specific compiler without changing the system default, pass it
through `CC` and `CXX` before the first configure command. For example:

```bash
CC=gcc-14 CXX=g++-14 cmake .
```

If configuration fails inside LLVM's CMake files with a missing imported target
such as `ZLIB::ZLIB`, one of LLVM's support-library development packages is
missing. Install the dependency packages listed above, then run `cmake .` again.

If configuration or compilation fails with a missing C++23 standard-library
header such as `print` or `stacktrace`, CMake is using a compiler or standard
library that is too old. On Ubuntu 24.04 this usually means it found GCC 13.
Install GCC 14 or newer, remove the old CMake cache if necessary, and configure
again with `CC=gcc-14 CXX=g++-14 cmake .`.

#### Link Driver

`dq-comp` uses Clang as the native linker driver when it turns generated object
files into an executable. A compiler built from these instructions uses LLVM's
own `clang++` from the configured LLVM installation by default.

If you are using an older build, or if you want to select another linker driver,
set `DQ_LINKER_DRIVER`:

```bash
DQ_LINKER_DRIVER=clang++-21 dq-run examples/basic/test1.dq
```

You can also export it for the current shell:

```bash
export DQ_LINKER_DRIVER=clang++-21
```
