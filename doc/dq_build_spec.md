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

All compiler-generated build state is placed below a `.dqbuild` directory.
This includes compiled modules, temporary files, interface sidecars, linker
response files, lock files, and other metadata needed during the build.

The default build root is the directory containing the main `.dq` file passed to the compiler.

Example:

```bash
dqc /home/user/tools/flash.dq
```

creates build artifacts under:

```text
/home/user/tools/.dqbuild/
```

The compiler must not write generated build files into system package
directories, user package directories, compiler-shipped package directories, or
RTL source directories.

The final executable or target image is a user-visible output, not package build
state. By default it is written next to the main source file. A compiler option
may request placing the final artifact under `.dqbuild/<build-tag>/` instead.

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

  pkg/
    <package-name>/
      <package-module-path>.dqm

  <temporary-and-metadata-files>
```

Directory meaning:

```text
local/  compiled modules from the current application/project source tree
pkg/    compiled modules resolved through package search paths
```

The `pkg/` directory also contains compiler-shipped standard packages and RTL
packages. There is no separate `rtl/` artifact directory; RTL modules are package
modules for build layout purposes.

Temporary files and metadata may be placed directly below `.dqbuild/<build-tag>/`
or in implementation-selected subdirectories. They must not be created beside
source package files.

The exact internal layout may mirror source module paths where needed to avoid
name collisions.

## 7. Main Module Handling

The main application source file is compiled like any other module.

Example:

```bash
dqc flash.dq
```

produces:

```text
.dqbuild/<build-tag>/local/flash.dqm
flash
```

The `.dqm` is the compiled main module. The `flash` executable is the default
final artifact and is written next to `flash.dq`.

The special role of the main module appears only during the final linking step,
where it is used as the entry application module.

If the user selects build-directory output for the final artifact, the executable
is written below `.dqbuild/<build-tag>/` instead of beside the main source file.

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

      pkg/
        rtl/
          rtl_linux.dqm
        dqserial/
          dqserial.dqm
```

This allows compiled RTL and package modules to be reused by several utilities in the same directory and build tag.

By default, the final executables are written next to their main source files:

```text
tools/
  flash
  monitor
  packimage
```

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

## 10. Summary

The intended DQ build model is:

```text
.dq source files are distributed.
.dqm files are generated locally.
RTL and packages are source-only.
The compiler writes generated build state only under .dqbuild/<build-tag>/.
The final executable is written next to the main source file by default.
The build tag is user-selected and opaque.
Multiple applications in one directory may share the same build tag.
Cleaning the build directory is the user's responsibility when the build context changes.
```
