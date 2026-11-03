# Compiler and Tool Overview

The DQ tools turn `.dq` source modules into native programs. The compiler uses
LLVM to generate machine code and a Clang linker driver to combine that code
with the DQ runtime and native libraries.

| Tool | Purpose |
| --- | --- |
| `dq-comp` | Compile a source module or `.dqproj` project and link an executable. |
| `dq-run` | Compile and run a single source file. |
| `dq-atrun` | Compile and execute the repository's autotests. |

## Install and Check the Tools

Use a [DQ release package](https://github.com/nvitya/dq-lang/releases) for your
host platform. Keep its `bin`, `stdpkg`, and bundled toolchain directories
together, since the tools discover supporting files relative to the compiler.

Follow [Linux Development Environment](020-linux-development-environment.md)
or [Windows Development Environment](030-windows-development-environment.md)
for installation. To work on the compiler itself, see
[Building the Compiler from Source](../090-appendices/020-building-the-compiler-from-source.md).

```bash
dq-comp --version
dq-comp --help
dq-run --help
```

## Compile a Program

Save this as `hello.dq`:

```dq
use print

func *Main():
    PrintLn("Hello, DQ!")
endfunc
```

Run it directly:

```bash
dq-run hello.dq
```

Or keep the compiled executable:

```bash
dq-comp -g -O0 hello.dq
./hello
```

On Windows, the output is `hello.exe`; run it with `.\hello.exe` in PowerShell.
`*Main` is the DQ program entry point. The selected runtime supplies the native
startup code on hosted targets.

## Choose a Workflow

Use `dq-run` for small examples. Compiler options go before the source filename;
program arguments go after it, optionally separated by `--`:

```bash
dq-run -O2 hello.dq -- first second
```

When no compiler options are supplied, `dq-run` selects `-g -O0`. These flags
provide source information for debugging and runtime backtraces.

Use `dq-comp` for explicit output names, reusable builds, and cross targets.
A [project file](050-project-files.md) keeps a longer set of options together.
Imported DQ modules and runtime modules are compiled as needed; you do not need
to list every imported source file on the command line.

The next chapters describe [program execution](015-runtime-and-program-execution.md),
[compiler options](040-compiler-usage-and-options.md), and the
[generated build artifacts](060-build-process-and-module-artifacts.md).
