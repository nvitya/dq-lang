# Cross-Compiling the DQ Compiler

This guide explains how to cross-compile the DQ compiler (`dq-comp`) from an x86_64 Ubuntu host (e.g. 26.04) to different architectures.

## Prerequisites: LLVM Dependency

The DQ compiler depends on LLVM (`find_package(LLVM REQUIRED CONFIG)`). To build `dq-comp` so that it runs on a target architecture (e.g., `aarch64`), you must provide the target architecture's version of the LLVM libraries. 

This is a known issue on Debian/Ubuntu: installing target-architecture `-dev` packages (like `llvm-dev:arm64`) can conflict with critical host packages and try to remove your desktop environment! 

**Do NOT use `apt` to install target LLVM development libraries.**

Instead, use the official pre-built LLVM binaries for the target architecture, or build LLVM from source for the target.

### Example: Setting up for aarch64-linux (Using Pre-built LLVM)

1. **Install the cross-compiler and multiarch dependencies on the host:**
   Because the official LLVM binaries for Linux are compiled with `libc++` and Link Time Optimization (LTO), we must use `clang++` and `lld` as the cross-compiler instead of GNU `g++`. We also need the target architecture's `libc++`, `zlib`, `zstd`, and `libxml2` headers.
   ```bash
   sudo apt update
   sudo apt install clang lld \
        libc++-dev:arm64 libc++abi-dev:arm64 \
        zlib1g-dev:arm64 libzstd-dev:arm64 libxml2-dev:arm64
   ```

2. **Download Pre-built LLVM for aarch64:**
   Go to the [LLVM GitHub Releases page](https://github.com/llvm/llvm-project/releases) and download the `aarch64-linux-gnu` tarball for the LLVM version you want (e.g., LLVM 20).
   
   ```bash
   mkdir -p sysroots/llvm-aarch64
   cd sysroots
   wget https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.0/LLVM-21.1.0-Linux-ARM64.tar.xz
   tar -xf LLVM-21.1.0-Linux-ARM64.tar.xz -C llvm-aarch64 --strip-components=1
   cd ..
   ```

3. **Configure CMake to use the downloaded LLVM:**
   When running the make target, you can pass the `LLVM_DIR` directly via the environment or add it to the make command. For our setup, we can explicitly tell CMake where to find it:
   
   ```bash
   make cross-aarch64-linux CMAKE_EXTRA_ARGS="-DLLVM_DIR=$(pwd)/sysroots/llvm-aarch64/lib/cmake/llvm"
   ```

*(For other architectures like `armhf` or `riscv64`, substitute `:arm64` with `:armhf` or `:riscv64` in the `apt install` command above.)*

## Building

We have added specific Make targets that simplify the CMake configuration for cross-compiling. The toolchain files are located in `toolchains/`.

### Supported Targets

- `make cross-aarch64-linux`
- `make cross-armhf-linux`
- `make cross-rv64g-linux`
- `make cross-x86_64-win`

## Windows x86_64

The Windows cross-build is different from the Linux cross-builds in two ways:

1. You need a Windows cross C/C++ toolchain that runs on the Linux host.
2. `dq-comp` itself links against LLVM C++ libraries, so the build still needs
   Windows-target LLVM development libraries visible through `LLVM_DIR`.

The `llvm-mingw` project is a good source for the first part. For an x86_64
Linux host building x86_64 Windows binaries, use the Ubuntu x86_64 tarball:

```text
llvm-mingw-20251216-ucrt-ubuntu-22.04-x86_64.tar.xz
```

For a future self-contained Windows DQ package, the matching Windows-hosted
toolchain archive is useful too:

```text
llvm-mingw-20251216-ucrt-x86_64.zip
```

The `.tar.xz` archive is used on the Linux build machine. The `.zip` archive is
for the final Windows package, where it can provide `clang.exe`, `lld`, startup
objects, CRT libraries, and Windows import libraries for DQ users.

### Recommended Local Layout

Keep checked-in CMake toolchain files in `toolchains/`. Put downloaded and
extracted binary toolchains under `sysroots/`, which is ignored by git:

```bash
mkdir -p sysroots/llvm-mingw-x86_64
tar -xf toolchains/llvm-mingw-20251216-ucrt-ubuntu-22.04-x86_64.tar.xz \
  -C sysroots/llvm-mingw-x86_64 --strip-components=1
```

If the archive was downloaded somewhere else, use that path instead of the
`toolchains/...tar.xz` path above. The important result is:

```text
sysroots/llvm-mingw-x86_64/bin/x86_64-w64-mingw32-clang++
sysroots/llvm-mingw-x86_64/x86_64-w64-mingw32/
```

### Configure With llvm-mingw

The Windows toolchain file accepts `DQ_MINGW_ROOT`:

```bash
make cross-x86_64-win CMAKE_EXTRA_ARGS="\
  -DDQ_MINGW_ROOT=$(pwd)/sysroots/llvm-mingw-x86_64 \
  -DLLVM_DIR=/path/to/windows-target-llvm/lib/cmake/llvm"
```

`DQ_MINGW_ROOT` points CMake at the llvm-mingw cross compiler and target
headers/libraries. `LLVM_DIR` must point at the LLVM CMake package for LLVM
libraries that are linkable into the Windows `dq-comp.exe`.

### Important Limitation

The llvm-mingw release archive provides the compiler, linker, CRT/startup
objects, and Windows import libraries. It does not provide the LLVM development
CMake package used by this project:

```text
lib/cmake/llvm/LLVMConfig.cmake
```

Therefore llvm-mingw alone is probably not sufficient to build `dq-comp.exe`.
You still need a compatible Windows-target LLVM development build. The most
reliable approach is to build LLVM for the same MinGW/UCRT ABI using the
llvm-mingw toolchain, then pass that build's `lib/cmake/llvm` directory as
`LLVM_DIR`.

### Building the Windows LLVM Development Package

Use the helper script after extracting the llvm-mingw Linux-hosted toolchain:

```bash
tools/build-llvm-mingw-ucrt.sh
```

The script builds and installs a minimal LLVM development package here:

```text
sysroots/llvm-x86_64-win-ucrt
```

It downloads the matching LLVM source archive if needed, configures LLVM for
`x86_64-w64-windows-gnu`, builds only the X86 target backend, disables optional
compression/XML/terminal dependencies, and installs the CMake package needed by
`dq-comp`:

```text
sysroots/llvm-x86_64-win-ucrt/lib/cmake/llvm/LLVMConfig.cmake
```

After that, configure the DQ compiler cross-build with:

```bash
make cross-x86_64-win CMAKE_EXTRA_ARGS="\
  -DDQ_MINGW_ROOT=$(pwd)/sysroots/llvm-mingw-x86_64 \
  -DLLVM_DIR=$(pwd)/sysroots/llvm-x86_64-win-ucrt/lib/cmake/llvm"
```

### Building a Windows Release Package

After the cross-build succeeds, create a distributable zip with:

```bash
make package-x86_64-win CMAKE_EXTRA_ARGS="\
  -DDQ_MINGW_ROOT=$(pwd)/sysroots/llvm-mingw-x86_64 \
  -DLLVM_DIR=$(pwd)/sysroots/llvm-x86_64-win-ucrt/lib/cmake/llvm"
```

The package is written to:

```text
dist/dq-<version>-x86_64-windows-ucrt.zip
```

It contains:

- `bin/dq-comp.exe`
- `bin/dq-run.exe`
- `bin/dqatrun.exe`
- the required llvm-mingw runtime DLLs
- the Windows-hosted llvm-mingw linker SDK under `toolchain/llvm-mingw/`
- `stdpkg/`
- project license and Windows quick-start notes

The package script expects the Windows-hosted llvm-mingw archive here by
default:

```text
sysroots/llvm-mingw-20251216-ucrt-x86_64.zip
```

Override it with `DQ_WINDOWS_TOOLCHAIN_ZIP=/path/to/llvm-mingw-ucrt-x86_64.zip`,
or use an already extracted Windows-hosted toolchain with:

```bash
DQ_WINDOWS_TOOLCHAIN_ROOT=/path/to/llvm-mingw-20251216-ucrt-x86_64 \
  tools/package-windows-release.sh
```

At runtime, `dq-comp.exe` links application executables with the packaged
toolchain:

```text
toolchain\llvm-mingw\bin\clang.exe --target=x86_64-w64-windows-gnu -fuse-ld=lld
```

Set `DQ_LINKER_DRIVER` to override that driver path.

If the helper needs a different native `llvm-tblgen`, set it explicitly:

```bash
LLVM_TBLGEN=/path/to/llvm-tblgen tools/build-llvm-mingw-ucrt.sh
```

Prebuilt package managers such as MSYS2 may also provide `LLVMConfig.cmake`, but
mixing their LLVM libraries with the llvm-mingw toolchain can create C++ ABI and
runtime mismatches. For the first Windows DQ package, prefer building LLVM with
the same llvm-mingw toolchain used to build `dq-comp.exe`.

### Current Portability Status

The repository has a Windows cross-build target and the hosted compiler tools
now build as Windows executables with the llvm-mingw toolchain:

- `dq-comp.exe`
- `dq-run.exe`
- `dqatrun.exe`

The compiler sources now guard Linux-only backtrace support, use portable
executable discovery, and provide a Windows implementation of the process
runner and artifact locking. Application linking uses the bundled llvm-mingw
toolchain, and hosted Windows programs use a minimal `rtl/rtl_windows.dq`
runtime entry point. That runtime currently omits Linux-only SIGSEGV recovery.

### Example: Building for aarch64

From the root of the `dq-lang` project, run:

```bash
make cross-aarch64-linux
```

This will:
1. Create a build directory at `build-cross/aarch64-linux`.
2. Run CMake configured with `-DCMAKE_TOOLCHAIN_FILE=../../toolchains/aarch64-linux.cmake`.
3. Compile the project for the `aarch64` target.

The resulting `dq-comp` executable will be placed in `build-cross/aarch64-linux/build/compiler/` or standard output directories defined by CMake.

## Toolchain Files
If you need to customize the cross-compilation environment (for example, setting a custom sysroot or pointing to a locally compiled LLVM), you can edit the corresponding toolchain file in the `toolchains/` directory.

- `CMAKE_FIND_ROOT_PATH` is currently pointed to the default Ubuntu cross-compiler sysroot (e.g., `/usr/aarch64-linux-gnu`).
