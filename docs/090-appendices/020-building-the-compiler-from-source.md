# Building the Compiler from Source

Build from source when there is no release package for your platform, or when
you want to work on the compiler itself.

## Requirements

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

## Select a C++ Compiler

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

Select GCC 14 for this checkout when configuring CMake, without changing the
system compiler:

```bash
CC=gcc-14 CXX=g++-14 cmake .
```

Use this instead of the plain `cmake .` command in the configure step below.
CMake remembers the selected compiler in its cache.

## Install LLVM 21

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

## Get The Source

Clone the DQ repository and enter the source tree:

```bash
git clone https://github.com/nvitya/dq-lang.git
cd dq-lang
```

## Configure

From the repository root, generate the build files:

```bash
cmake .
```

This creates the root `Makefile` used by the repository's `GNUmakefile`.
(See [Troubleshooting](#troubleshooting) if you experience problems here.)

## Build

Build the compiler and helper tools:

```bash
make -j"$(nproc)"
```

The host build places the user-facing tools in the repository's `build`
directory:

```text
build/dq-comp
build/dq-run
build/dq-atrun
```

## Install

Install the built tools and standard packages:

```bash
sudo make install
```

On Linux, the install target installs `dq-comp`, `dq-run`, and `dq-atrun` into
`lib/dq/bin` and creates relative symlinks to them in `bin` under the chosen
CMake install prefix. Standard DQ packages are installed into `lib/dq/stdpkg`.
The default prefix is usually `/usr/local`.

To install under a different prefix, configure with `CMAKE_INSTALL_PREFIX`:

```bash
cmake . -DCMAKE_INSTALL_PREFIX="$HOME/.local"
make -j"$(nproc)"
make install
```

Then make sure the selected `bin` directory is on your `PATH`.

## Try The Compiler

Compile and run a single DQ file with the installed tools:

```bash
dq-run examples/langdemo.dq
```

If you are working on your own file:

```bash
dq-run path/to/file.dq
```

`dq-run` uses `-g -O0` by default when no compiler options are supplied, which
is useful while developing or debugging.

## Troubleshooting

### cmake .

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

### Link Driver

`dq-comp` uses Clang as the native linker driver when it turns generated object
files into an executable. A compiler built from these instructions uses LLVM's
own `clang++` from the configured LLVM installation by default.

If you are using an older build, or if you want to select another linker driver,
set `DQ_LINKER_DRIVER`. Its LLVM major version must match the LLVM version used
to build `dq-comp`:

```bash
DQ_LINKER_DRIVER=clang++-21 dq-run examples/basic/test1.dq
```

You can also export it for the current shell:

```bash
export DQ_LINKER_DRIVER=clang++-21
```


## Verify the Build

Run the compiler autotests from the repository root:

```bash
make test
```

For a single test with detailed output:

```bash
build/dq-atrun --clean -c build/dq-comp autotest/tests/basic/printf.dq
```

To exercise reuse of existing module artifacts, omit `--clean`:

```bash
build/dq-atrun -c build/dq-comp -r autotest/tests
```

[Release and Cross-Build Notes](033-release-and-cross-builds.md) covers building
tools for other hosts and producing distribution archives.
