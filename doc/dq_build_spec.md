# DQ Build Specification

This document defines the basic build artifact layout for the DQ compiler (`dq-comp`).

## 1. Basic Model

DQ packages and the DQ RTL are distributed as source code.

```text
.dq   = DQ source module
.dqm  = generated compiled module
```

A `.dqm` file is an object file that also contains a special DQ interface section:

```text
.dqm_if
```

There is no separate `.o` output file in the normal DQ build model.

`.dqm` files are generated build artifacts. They are not distributed as part of the RTL or normal packages.

## 2. Source-Only Packages

DQ packages are source-first and should remain clean source directories.

The compiler must not create build directories inside package directories.

Allowed package contents:

```text
dqserial/
  dqserial.dq
  linux.dq
  dqpkg.toml
```

Not allowed as normal distributed package contents:

```text
dqserial/
  dqserial.dqm
  .dqbuild/
```

The same rule applies to the DQ RTL: the installed RTL is source-only.

## 3. Build Directory

All generated build artifacts are placed below a `.dqbuild` directory.

The default build root is the directory containing the main `.dq` file passed to the compiler.

Example:

```bash
dqc /home/user/tools/flash.dq
```

creates build artifacts under:

```text
/home/user/tools/.dqbuild/
```

The compiler must not write generated `.dqm` files into system package directories, user package directories, or RTL source directories.

## 4. Build Tag

The build tag is an opaque user-selected directory name below `.dqbuild`.

```text
.dqbuild/<build-tag>/
```

The default build tag is:

```text
<target-arch>-<target-os>
```

Examples:

```text
.dqbuild/x86_64-linux/
.dqbuild/aarch64-linux/
.dqbuild/arm32-none/
```

The user may freely select another build tag, for example:

```text
.dqbuild/debug/
.dqbuild/release/
.dqbuild/arm64_release/
.dqbuild/cortexm7_debug/
```

The compiler must not infer compiler switches from the build tag. For example, the tag `debug` does not automatically imply `-g`, and the tag `release` does not automatically imply optimization.

The build tag is only a namespace for generated files.

## 5. Target and Build Tag Are Separate

Target architecture and target operating system are compiler settings independent from the build tag.

Conceptually:

```text
target_arch = x86_64, arm32, aarch64, rv32, ...
target_os   = linux, none, windows, ...
build_tag   = user-selected build directory name
```

Example:

```bash
dqc main.dq --arch aarch64 --os linux --build arm64_release
```

creates:

```text
.dqbuild/arm64_release/
```

but the actual target remains:

```text
aarch64-linux
```

## 6. Directory Layout

A typical build directory has this structure:

```text
.dqbuild/<build-tag>/
  build.json

  local/
    <local-module-path>.dqm

  rtl/
    <rtl-module-path>.dqm

  packages/
    <package-name>/
      <package-module-path>.dqm

  out/
    <executable-name>

  link/
    <executable-name>.rsp
```

Directory meaning:

```text
local/     modules from the current application/project source tree
rtl/       compiled DQ RTL modules
packages/  compiled imported package modules
out/       final linked executables or target images
link/      linker response files or linker metadata
```

The exact internal layout may mirror source module paths where needed to avoid name collisions.

## 7. Main Module Handling

The main application source file is compiled like any other module.

Example:

```bash
dqc flash.dq
```

produces:

```text
.dqbuild/<build-tag>/local/flash.dqm
```

The special role of the main module appears only during the final linking step, where it is used as the entry application module.

## 8. Multiple Applications in One Directory

Multiple applications in the same directory share the same `.dqbuild/<build-tag>` tree.

Example source tree:

```text
tools/
  flash.dq
  monitor.dq
  packimage.dq
```

Possible build result:

```text
tools/
  .dqbuild/
    x86_64-linux/
      local/
        flash.dqm
        monitor.dqm
        packimage.dqm

      rtl/
        system.dqm
        strings.dqm

      packages/
        dqserial/
          dqserial.dqm

      out/
        flash
        monitor
        packimage
```

This allows compiled RTL and package modules to be reused by several utilities in the same directory and build tag.

## 9. Cleaning Rule

A `.dqbuild/<build-tag>` directory belongs to one coherent build context.

If the compiler version, target, ABI-relevant compiler options, package sources, RTL sources, or important build definitions change, the user is responsible for cleaning the affected build tag or selecting a different one.

Example:

```bash
rm -rf .dqbuild/release
```

or:

```bash
rm -rf .dqbuild
```

The compiler may create a `build.json` file to record the build context and emit diagnostics if the current invocation obviously conflicts with the existing build directory.

## 10. Manifest

The compiler should create a small manifest file:

```text
.dqbuild/<build-tag>/build.json
```

It may contain information such as:

```json
{
  "build_tag": "arm64_release",
  "target_arch": "aarch64",
  "target_os": "linux",
  "compiler": "dq-comp",
  "compiler_version": "0.1"
}
```

The manifest is for diagnostics and build transparency. It is not a substitute for the user-selected build tag and does not make the build directory a fully managed package cache.

## 11. Summary

The intended DQ build model is:

```text
.dq source files are distributed.
.dqm files are generated locally.
RTL and packages are source-only.
The compiler writes generated files only under .dqbuild/<build-tag>/.
The build tag is user-selected and opaque.
Multiple applications in one directory may share the same build tag.
Cleaning the build directory is the user's responsibility when the build context changes.
```
