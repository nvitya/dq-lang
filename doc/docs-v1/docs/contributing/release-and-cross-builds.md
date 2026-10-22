# Release and Cross-Build Notes

> Maintainer procedure: these commands build distribution artifacts and are not
> part of the DQ language or compiler command-line contract.

## Release Packages

Build a compact Linux package without bundled LLVM tools:

```bash
make -j"$(nproc)" package-linux-release
```

Cross-build and package ARM64 Linux without bundled LLVM tools:

```bash
make -j"$(nproc)" package-arm64-linux
# Alias: make -j"$(nproc)" package-aarch64-linux
```

This uses the `cross-aarch64-linux` build and creates
`dist/dq-<version>-arm64-linux.tar.gz`. The archive includes `dq-comp`, `dq-run`,
`dq-atrun`, and the standard packages. The destination needs a 64-bit ARM Linux system,
the tools' runtime dependencies, and a compatible native Clang/linker.
Pass `CMAKE_EXTRA_ARGS` to select the ARM64 LLVM development libraries when
cross-building, as described in the cross-compilation guide below.

Linux archives include `install.sh`. After extraction on the destination, run
`sudo ./install.sh` for the default `/usr/local` installation, or
`./install.sh --prefix "$HOME/.local"` for a user installation. The installer also
accepts `DESTDIR` or `--destdir` for staging and requires no build tools.

To test an extracted Linux release, run from the archive's root:

```bash
bin/dq-atrun --clean -c bin/dq-comp -r autotest/tests
```

The archive includes the test helper package, C++ fixture, and internal test
executables under `autotest/bin`. Native x86 assembly tests are marked
`hostarch('x64')` and are skipped on ARM64. Driver tests also need `c++`, `nm`,
`llvm-readobj`, and `wasm-objdump` on `PATH`. When using versioned LLVM packages,
add their tool directory to `PATH`, for example `/usr/lib/llvm-21/bin`.

On Debian 13 (Trixie), after enabling
[Trixie backports](https://backports.debian.org/Instructions/), install the ARM64
release's runtime and linker dependencies plus the additional test tools with:

```bash
sudo apt install -t trixie-backports libc++1 libc++abi1 clang-21 lld-21 llvm-21
sudo apt install g++ libbacktrace-dev zlib1g-dev wabt wasi-libc
sudo apt install -t trixie-backports libclang-rt-21-dev-wasm32
export PATH="/usr/lib/llvm-21/bin:$PATH"
```

The ARM64 release requires libc++ 21; Trixie's default libc++ 19 is insufficient.
The WASI libc and wasm32 builtins are needed by the cross-target driver tests.

Build the self-contained Linux package:

```bash
make -j"$(nproc)" package-linux-full-release
```

Build the full Windows package from its supported cross-build environment:

```bash
make -j"$(nproc)" package-windows-release
```

Run the normal compiler build and autotests before producing packages. Test each
package in a clean environment that does not accidentally provide unbundled
compiler or runtime dependencies.

## Clean Ubuntu Test Container

One suitable smoke-test environment is Ubuntu 24.04:

```bash
docker pull ubuntu:24.04
docker run --name dq-ubuntu2404 -it ubuntu:24.04 bash
apt update
apt install -y lsb-release wget gnupg software-properties-common ca-certificates sudo
apt install -y build-essential git cmake ninja-build clang lld g++
```

Keep package-production dependencies separate from dependencies expected on the
destination machine. The full package should be tested without relying on a
system LLVM installation.

For compiler cross-compilation prerequisites and target toolchains, see
[Cross-Compiling and Packaging](../compiler/cross-compiling.md).
