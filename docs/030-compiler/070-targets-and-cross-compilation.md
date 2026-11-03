# Targets and Cross-Compilation

A target describes the machine and runtime on which your **DQ application** will
run. Selecting a foreign application target does not require rebuilding
`dq-comp`. To build a compiler executable that itself runs on another machine,
see [Compiler Cross-Builds](../090-appendices/033-release-and-cross-builds.md#compiler-cross-builds).

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
source /path/to/dq-lang/tools/dqcrosswinsetup.sh
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
prerequisites fail the requested run; they are not silently skipped. Windows
fixtures live in `autotest/cross-windows`; the suite also reuses the portable
thread/channel autotests and the threads examples. It checks native thread
lifecycle and handle cleanup, atomics, event deadlines, channel capacity and
waits, producer/consumer stress, and work distribution at O0 and O3 with full
LTO. Normal `make test`, `make autotest`, and `make check` runs do not invoke the
Windows suite or require these dependencies.
Executables, caches, runtime DLLs, command logs, and a dedicated Wine prefix
are stored under `build/test-windows`. The suite leaves the user's Wine
prefix untouched.

## Bare-Metal Projects

Bare targets need board-specific startup objects, a linker script, and suitable
runtime libraries when linking a complete image. Put those settings in a
[project file](050-project-files.md#bare-metal-runtimes). The bundled support
libraries cover the supported CPU/ABI combinations; they do not supply a board's
reset handler or hardware initialization.

## WebAssembly Examples

A WASI command module uses a hosted C library:

```bash
dq-comp --target=wasm32-wasi --sysroot=/path/to/wasi-sysroot app.dq
wasmtime app.wasm
```

For a browser or another custom embedding, `wasm32-bare` produces an object by
default. Supply the embedding's link options and imports explicitly. See
[C Interoperability](../020-language/200-c-interoperability.md#webassembly-imports)
for import declarations, and `examples/wasm`, `examples/wasm_webgl`, and
`examples/wasm_gpu` in the repository for integration examples.
