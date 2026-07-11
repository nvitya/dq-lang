# Getting Started

## Get The DQ Compiler

DQ programs are compiled with `dq-comp`. For single-file experiments, the
companion `dq-run` tool compiles a `.dq` file and immediately runs the result.

The easiest way to get the compiler is to download a release package from the
project's GitHub releases page:

```text
https://github.com/nvitya/dq-lang/releases
```

After extracting or installing a release package, make sure the compiler tools
are on your `PATH`:

```bash
dq-comp --version
dq-run --help
```

If there is no release package for your platform yet, build the compiler from
source by following [Getting the DQ Compiler](compiler/getting-dq-comp.md).

## First Program

DQ programs are written in `.dq` files.

```dq
use print

function *Main() -> int:
    PrintLn("hello from DQ")
    return 0
endfunc
```

Run with:

```bash
dq-run hello.dq
```

`dq-run` first invokes `dq-comp`, which compiles `hello.dq` into `./hello`, and
then runs the created executable:

```bash
dq-comp -g -O0 hello.dq
./hello
```

## Program Entry

A program entry point is a special function named `*Main`.

```dq
function *Main() -> int:
    return 0
endfunc
```

The return value becomes the process exit code.

## Arguments

Program arguments are available through the implicit runtime module.

```dq
use print

function *Main() -> int:
    for i : int = 0 count ArgCount():
        PrintLn("arg {} = {}", [i, ArgStr(i)])
    endfor
    return 0
endfunc
```

Run with arguments:

```bash
dq-run args.dq -- one two
```

The `--` separator is useful when program arguments may look like compiler
options.

## Standard Packages

Standard modules are imported with `use`.

```dq
use print
use strutils
use file
use json
```

The compiler searches its built-in standard package roots first, then any roots
added with `--pkg-path`.

## Optimization and Debugging

Code optimization means the compiler spends extra work improving the generated
machine code. Optimized code can make debugging harder because the generated
machine code may no longer follow the source code line by line.

The `-O0` option turns off optimization. The `-O3 --lto` combination enables
the highest optimization level and link-time optimization.

To debug code or see error backtraces with source line information, pass `-g`
when compiling.

When no compiler options are supplied, `dq-run` uses `-g -O0`, which is a good
debugging default and enables useful source locations in runtime backtraces.
`dq-comp` compiles with `-O1` optimization and no debug info by default.

For fastest running code, invoke `dq-run` or `dq-comp` with `-O3 --lto`:

```bash
dq-run -O3 --lto examples/benchmarks/binary_trees_dq.dq 20
```
