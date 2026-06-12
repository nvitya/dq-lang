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
