# Getting the DQ Compiler

This page describes how to get `dq-comp` and `dq-run`.

## Release Packages

For normal use, prefer a release package from the DQ GitHub releases page:

```text
https://github.com/nvitya/dq-lang/releases
```

After extracting or installing a release package, make sure the compiler tools
are on your `PATH`:

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
| A C++23 compiler | GCC or Clang |
| LLVM 21 development files | CMake must be able to find LLVM 21's `LLVMConfig.cmake` |

The compiler is currently developed and tested with LLVM 21. Older LLVM
versions, including LLVM 18 and below, are not expected to work.

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
sudo apt install build-essential cmake llvm-21-dev clang-21 lld-21
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

If CMake finds another LLVM version, or cannot find LLVM at all, pass LLVM 21's
CMake package directory explicitly:

```bash
cmake . -DLLVM_DIR=/usr/lib/llvm-21/lib/cmake/llvm
```

To build with Clang 21 explicitly:

```bash
CC=clang-21 CXX=clang++-21 cmake .
```

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

### Verify

Run the compiler autotests:

```bash
make test
```

The direct autotest command is:

```bash
build/dqatrun -c build/dq-comp -r autotest/tests
```

To run one test with more focused output:

```bash
build/dqatrun -c build/dq-comp autotest/tests/basic/printf.dq
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
