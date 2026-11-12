# Compiler Usage and Options

Compile a source file or a [project file](050-project-files.md) with `dq-comp`.
Use `dq-comp --help` to see the options accepted by your installed version.

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
| `--pkg_path <path>` | add a package search root; repeatable; alias: `--pkg-path` |
| `--pkg <name:directory>` | map a package directly; repeatable; takes priority over search roots |
| `--build <tag>` | select `.dqbuild/<tag>` |
| `--build-suffix <suffix>` | append to the selected build tag |
| `--build-root <path>` | select the local artifact build root |
| `--package-build-root <path>` | select a separate package artifact build root |
| `--lto[=full|off]` | control full LTO bitcode sidecars |
| `-Dname` | define a compile-time symbol as true |
| `-Dname=value` | define an integer or boolean symbol |
| `-O0`..`-O3`, `-Os`, `-Oz` | optimization level |
| `-g` | generate debug info |
| `--static` | link statically; equivalent to `--linker-arg=-static` |
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

Additional roots can be appended with `--pkg_path`. Search uses the last
matching package root first, so later `--pkg_path` entries can override earlier
roots. Both `--pkg_path=path` and `--pkg_path path` are accepted.

Use `--pkg=board:../boards/adcif` (or `--pkg board:../boards/adcif`) to select a
specific source directory for `board`. Explicit mappings override all search
roots, command-line mappings override project mappings, and the last mapping
for a name wins. A mapped directory must exist; missing files within it never
fall back to another package. CLI paths are relative to the working directory.
The equivalent project properties are `pkg_path` and `pkg`; see
[package lookup](050-project-files.md#package-lookup).

## Linking Native Code

`--link` requests linking explicitly; `-c` produces an object without linking.
`--linker-arg=<arg>` passes an argument to the native linker. For repeatable
linker inputs, library directories, or a linker script, use the corresponding
[project properties](050-project-files.md#properties).

`DQ_LINKER_DRIVER` selects the Clang driver used for linking. Match its LLVM
major version to the compiler's LLVM version, particularly for full LTO builds.
The full release packages supply a matching toolchain.

See [Targets and Cross-Compilation](070-targets-and-cross-compilation.md) for
application targets, sysroots, and Windows cross-linking. The
[tool overview](010-compiler-and-tool-overview.md) covers `dq-run`.
