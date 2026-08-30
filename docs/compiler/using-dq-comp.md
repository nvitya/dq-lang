# Using the DQ Compiler

The main tools are:

| Tool | Purpose |
| --- | --- |
| `dq-comp` | compile DQ modules and link executables |
| `dq-run` | compile a single DQ file and run it |
| `dqatrun` | run compiler autotests |

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
| `--targets` | list canonical targets and their LLVM/default settings |
| `--exceptions`, `--no-exceptions` | enable or disable DQ exception handling |
| `--dynstrings`, `--no-dynstrings` | enable or disable heap-backed dynamic strings |
| `--pkg-path <path>` | add a package search root; repeatable |
| `--build <tag>` | select `.dqbuild/<tag>` |
| `--build-suffix <suffix>` | append to the selected build tag |
| `--lto[=full|off]` | control full LTO bitcode sidecars |
| `-Dname` | define a compile-time symbol as true |
| `-Dname=value` | define an integer or boolean symbol |
| `-O0`..`-O3` | optimization level |
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
disabled, `strview`, string literals, and bounded `cstring` remain available,
but owning `str` operations are rejected at compile time.

## Compiler Targets

Run `dq-comp --targets` to print the canonical target names together with their
architecture, platform, LLVM triple, CPU/features, feature defaults, and
default link mode. In addition to the current host and Cortex-M targets, the
compiler supports:

| Target | Behavior |
| --- | --- |
| `wasm32_wasi` | Hosted WASI command module; links automatically and defaults to a `.wasm` output |
| `wasm32_bare` | Bare WebAssembly object generation; compile-only by default |
| `rv32imac_bare` | Bare RV32IMAC ELF object generation; compile-only by default |

`wasm32_wasi` requires Clang/LLD and a WASI sysroot containing libc at link
time. DQ exceptions are not supported on this target: `--exceptions` and
`exceptions = true` are rejected, while dynamic strings remain enabled by
default. The resulting command module uses WASI libc startup and can be run by
a WASI runtime such as Wasmtime.

The two bare targets do not bundle startup code, linker scripts, libc, or
compiler-runtime libraries. They emit objects without linking unless `--link`
is requested. A forced bare link must supply the appropriate objects and linker
arguments and normally sets `compiler_runtime = 'none'` and
`c_runtime = 'none'`.

Target selection defines `WASM`, `WASI`, or `RISCV` as applicable. Bare targets
also define `BARE`; all three targets define `TARGET_32BIT`. The existing
`EXCEPTIONS` and `DYNSTRINGS` defines continue to reflect the effective feature
settings.

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
