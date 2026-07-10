# Getting Started

DQ programs are written in `.dq` files and compiled with `dq-comp`.

```dq
use print

function *Main() -> int:
    PrintLn("hello from DQ")
    return 0
endfunc
```

Compile and run:

```bash
dq-comp hello.dq
./hello
```

For quick experiments use `dq-run`. It compiles the file and then runs the
resulting executable.

```bash
dq-run hello.dq
```

When no compiler options are supplied, `dq-run` uses `-g -O0`, which is a good
debugging default and enables useful source locations in runtime backtraces.

## Program Entry

A program entry point is a special object-style function named `*Main`.

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

