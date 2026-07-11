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
| `--ifdump` | dump a `.dqm_if` or embedded `.dqm` interface |
| `--no-use-sys` | disable implicit merged `sys` import |
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
    app.dqm
  pkg/
    print/
      print.dqm
    rtl/
      rtl_linux.dqm
```

`.dqm` files are object files with an embedded `.dqm_if` interface section. They
are generated artifacts, not source package contents.

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
