# Release and Cross-Build Notes

> Maintainer procedure: these commands build distribution artifacts and are not
> part of the DQ language or compiler command-line contract.

## Release Packages

Build a compact Linux package without bundled LLVM tools:

```bash
make -j"$(nproc)" package-linux-release
```

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
