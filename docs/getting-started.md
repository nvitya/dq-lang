# Getting Started

DQ programs are written in `.dq` files and can be run with `dq-run`.

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
The `dq-run` first invokest the `dq-comp` which compiles first into `./hello` and then runs the created executable:
```bash
dq-comp hello.dq
./hello
```

When no compiler options are supplied, `dq-run` uses `-g -O0`, which is a good
debugging default and enables useful source locations in runtime backtraces.
The `dq-comp` by default compiles with `-O1` optimiziation without debug info.

For fastest running code invoke the `dq-run` or `dq-comp` with `-O3 --lto`:
```bash
dq-run -O3 --lto examples/benchmarks/binary_trees_dq.dq 20
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

