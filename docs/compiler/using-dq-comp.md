# Using the DQ Compiler

The main tools are:

| Tool | Purpose |
| --- | --- |
| `dq-comp` | compile DQ modules and link executables |
| `dq-run` | compile a single DQ file and run it |
| `dq-atrun` | run compiler autotests |

For instructions on building these tools from source, see
[Getting the DQ Compiler](getting-dq-comp.md).

## `dq-comp`

Basic use:

```bash
dq-comp app.dq
./app
```

Useful options:

| Option | Meaning |
| --- | --- |
| `-o <file>` | set output filename |
| `-c` | compile only; do not link |
| `--ifgen` | generate a standalone `.dqm_if` interface file |
| `--ifdump` | dump a standalone `.dqm_if` interface |
| `--no-use-sys` | disable implicit merged `sys` import |
| `--target=<name>` | select a compiler target |
| `--sysroot=<path>` | select the target system root used for linking |
| `--cpu-features=<features>` | append LLVM CPU features to the target defaults |
| `--targets` | list canonical targets and their LLVM/default settings |
| `--exceptions`, `--no-exceptions` | enable or disable DQ exception handling |
| `--dynstrings`, `--no-dynstrings` | enable or disable heap-backed dynamic strings |
| `--pkg-path <path>` | add a package search root; repeatable |
| `--build <tag>` | select `.dqbuild/<tag>` |
| `--build-suffix <suffix>` | append to the selected build tag |
| `--build-root <path>` | select the local artifact build root |
| `--package-build-root <path>` | select a separate package artifact build root |
| `--lto[=full|off]` | control full LTO bitcode sidecars |
| `-Dname` | define a compile-time symbol as true |
| `-Dname=value` | define an integer or boolean symbol |
| `-O0`..`-O3`, `-Os`, `-Oz` | optimization level |
| `-g` | generate debug info |
| `-v`, `-vv`, `-vvv` | increasing compiler verbosity |
| `-ir` | print generated LLVM IR |

The default optimization level is currently `-O1`.

The `rtl/sys` prelude is implicitly merged for hosted and bare targets. Use
`--no-use-sys` only for low-level bootstrap or RTL work that must opt out.
Exception handling defaults to enabled on hosted targets and disabled on bare
targets; when enabled, `#ifdef EXCEPTIONS` selects exception-aware source code.
Runtime checks remain active when exception handling is disabled.
Dynamic strings follow the same target defaults; when enabled,
`#ifdef DYNSTRINGS` selects `str`-dependent source. With dynamic strings
disabled, `strslice`, `rostr`, string literals, and bounded `embstr` remain available,
but owning `str` operations are rejected at compile time.

## Compiler Targets

Run `dq-comp --targets` to print the canonical target names together with their
architecture, platform, LLVM triple, CPU/features, feature defaults, and
default link mode. In addition to the current host and Cortex-M targets, the
compiler supports:

| Target | Behavior |
| --- | --- |
| `x64-windows` | Windows x64 GNU/UCRT executable; also accepts `x86_64-win`, `x64-win`, and `x86_64-windows` |
| `armhf-linux` | ARMv7-A hard-float Linux executable |
| `arm64-linux` | AArch64 Linux executable |
| `rv64g-linux` | RV64G/LP64D Linux executable |
| `wasm32-wasi` | Hosted WASI command module; links automatically and defaults to a `.wasm` output |
| `wasm32-bare` | Bare WebAssembly object generation; compile-only by default |
| `rv32imac-bare` | Bare RV32IMAC ELF object generation; compile-only by default |

`wasm32-wasi` requires Clang/LLD and a WASI sysroot containing libc at link
time. DQ exceptions are not supported on this target: `--exceptions` and
`exceptions = true` are rejected, while dynamic strings remain enabled by
default. The resulting command module uses WASI libc startup and can be run by
a WASI runtime such as Wasmtime.

The foreign Linux targets require a target sysroot at link time. Select it with
`--sysroot=/path/to/sysroot` or the `sysroot` project property. DQ passes this
to its Clang linker driver as `--sysroot`; the sysroot must come from the
target distribution or SDK and match the deployed C library and development
packages.

For example, an ARM64 Debian sysroot is selected per build:

```bash
dq-comp --target=arm64-linux --sysroot=/path/to/debian-arm64-sysroot app.dq
```

The two bare targets do not bundle startup code, linker scripts, libc, or
compiler-runtime libraries. They emit objects without linking unless `--link`
is requested. A forced bare link must supply the appropriate objects and linker
arguments and normally sets `compiler_runtime = 'none'` and
`c_runtime = 'none'`.

Target selection defines architecture and ABI markers as applicable: 32-bit
x86 uses `X86_FAMILY` and `X86`, while 64-bit x86 uses `X86_FAMILY` and `X64`.
Arm targets use `ARM` plus `ARM_M` or `ARM_A`; the hosted hard-float target also
defines `ARMHF`. AArch64 defines `ARM_A`, `ARM64`, and `AARCH64`. RISC-V targets
define `RISCV` plus `RV32` or `RV64`. Bare targets also define `BARE`, and
`WASM`/`WASI`, `TARGET_32BIT`/`TARGET_64BIT`, `EXCEPTIONS`, and `DYNSTRINGS`
continue to reflect the selected target and feature settings.

### Windows cross-compilation from Linux

When built with LLVM's X86 backend, the Linux compiler can produce Windows x64
executables using a Linux-hosted llvm-mingw UCRT toolchain. With the toolchain
extracted under `sysroots/llvm-mingw-x86_64`, source the setup script in Bash
before compiling and running:

```bash
source tools/dqcrosswinsetup.sh
build/dq-comp --target=x64-windows app.dq
wine app.exe
```

The script detects the repository root from its own location, sets
`DQ_MINGW_ROOT`, and uses `winepath` to set `WINEPATH` to the toolchain's target
runtime DLL directory. Wine loads the DLLs directly from that directory, so
you do not need to copy them beside each executable. The script also sets
`MESA_VK_IGNORE_CONFORMANCE_WARNING=1` to suppress Mesa's RADV Vulkan
conformance warning. Wine and `winepath` must be installed.

You can source the script by its absolute path from any working directory.
For example, add this line to `~/.bashrc` to configure new Bash terminals
automatically (adjust the path to your checkout):

```bash
source /lindata/workvc/dq-lang/tools/dqcrosswinsetup.sh
```

Source the script in the current terminal as well to apply the settings
immediately. Running it as a separate process does not configure the current
shell.

Windows outputs default to `.exe`. An explicit `-o` filename is used exactly as
given, including filenames without an extension. Object generation with `-c`
does not require the Windows libraries or Wine.

Set `DQ_MINGW_ROOT` to the extracted llvm-mingw root when it is installed
elsewhere:

```bash
DQ_MINGW_ROOT=/path/to/llvm-mingw build/dq-comp --target=x64-windows app.dq
```

The linker search order is `DQ_LINKER_DRIVER`, the cross driver under
`DQ_MINGW_ROOT/bin`, compiler-relative `toolchain/llvm-mingw`, `llvm-mingw`,
and `sysroots/llvm-mingw-x86_64` installations, then
`x86_64-w64-mingw32-clang++` on `PATH`. An invalid explicit `DQ_MINGW_ROOT`
fails when linking. Native Windows compiler installations retain their
Windows-hosted driver discovery. `--sysroot` is still passed through when set.

The toolchain must provide Clang/LLD, startup objects, UCRT and Windows import
libraries, libc++, and libunwind. Full LTO also requires a toolchain with the
same LLVM major version as DQ. Windows full-LTO links use LLD's default
optimization pipeline rather than the ELF-specific O1 inlining pipeline.

As an alternative to setting `WINEPATH`, copy the toolchain's target runtime
DLLs beside the executable: `libc++.dll`, `libunwind.dll`, and
`libwinpthread-1.dll` when provided. These are under
`x86_64-w64-mingw32/bin` in llvm-mingw.

The opt-in compatibility suite copies the runtime DLLs automatically:

```bash
make test-windows
# Custom installation:
DQ_MINGW_ROOT=/path/to/llvm-mingw make test-windows
# Direct invocation with explicit tool paths:
python3 tools/test-windows.py --compiler build/dq-comp \
    --mingw-root /path/to/llvm-mingw --wine /path/to/wine
```

It requires Linux, Python 3, llvm-mingw, and usable 64-bit Wine. Missing
prerequisites fail the requested run; they are not silently skipped. Fixtures
live outside `autotest/tests`, and normal `make test`, `make autotest`, and
`make check` runs do not invoke the suite or require these dependencies.
Executables, caches, runtime DLLs, command logs, and a dedicated Wine prefix
are stored under `build/test-windows`. The suite leaves the user's Wine
prefix untouched.

## `dq-run`

`dq-run` is for experiments and single-file programs:

```bash
dq-run app.dq
dq-run -O2 app.dq -- arg1 arg2
```

Compiler options must come before the `.dq` file. Program arguments come after
the file, or after `--`.

When no compiler options are supplied, `dq-run` passes `-g -O0` to `dq-comp`.

## Build Artifacts

DQ packages and the RTL are source-first. The compiler writes generated module
artifacts under `.dqbuild`.

```text
.dqbuild/<build-tag>/
  local/
    app.o
    app.dqm_if
  pkg/
    print/
      print.o
      print.dqm_if
    rtl/
      rtl_linux.o
      rtl_linux.dqm_if
```

Each module has a normal native `.o` object and a standalone `.dqm_if` interface.
Both are generated artifacts, not source package contents.

By default the build root is the directory containing the main `.dq` file. The
default build tag is `<target-arch>-<target-rtl>`, for example
`x86_64-linux`.

The build tag is only a namespace. A tag named `debug` does not imply `-g`, and
a tag named `release` does not imply `-O3`.

By default, local and package artifacts use the same build root. With
`--package-build-root <path>`, package and RTL artifacts are instead written to
`<path>/.dqbuild/<build-tag>/pkg`, while local artifacts remain under the normal
build root. This allows several projects with the same build context to share
compiled package modules.

## Package Search Path

Bare imports such as `use print` or `use json` are resolved through package
search roots. The compiler adds standard roots automatically, including:

```text
/usr/lib/dq/stdpkg
<compiler-dir>/../lib/dq/stdpkg
<compiler-dir>/../stdpkg
/usr/lib/dq/packages
<compiler-dir>/../lib/dq/packages
~/.dq/packages
```

Additional roots can be appended with `--pkg-path`. Search uses the last
matching package root first, so later `--pkg-path` entries can override earlier
roots.

## Cleaning

Clean `.dqbuild/<tag>` when the compiler version, target, ABI-relevant options,
package sources, or important build defines change.

```bash
rm -rf .dqbuild/x86_64-linux
```
