# Getting Started

This chapter takes you from installing DQ to running your first program. You
need the DQ compiler and a text editor. Visual Studio Code is recommended for
syntax highlighting, compiler diagnostics, and running programs from the editor.

## Install DQ

Download a compiler package for your operating system from the
[DQ releases page](https://github.com/nvitya/dq-lang/releases). Choose a compiler
archive from the release assets; the GitHub **Source code** archives contain
the project sources rather than ready-to-run compiler tools.

Follow the setup guide for your system, then return here:

- [Linux Development Environment](../030-compiler/020-linux-development-environment.md):
  installing a release, setting up your shell and editor, and adding external libraries.
- [Windows Development Environment](../030-compiler/030-windows-development-environment.md):
  extracting the Windows package, configuring `PATH`, setting up the editor, and
  adding external libraries.

The two main tools are:

| Tool | Purpose |
| --- | --- |
| `dq-comp` | Compiles DQ source files and links them into an executable. |
| `dq-run` | Compiles a DQ program and immediately runs it. |

In a terminal with your DQ environment configured, check that the compiler is
available:

```text
dq-comp --version
```

The command should print the installed compiler version.

## Write Your First Program

Create a directory for your experiments. In that directory, use your editor to
create a plain text file named `hello.dq`. The `.dq` extension identifies DQ
source code. On Windows, make sure the file is not accidentally saved as
`hello.dq.txt`.

Put this code in the file and save it:

```dq
use print

func *Main():
    PrintLn("Hello from DQ!")
endfunc
```

The program has three main parts:

- `use print` makes the standard package's printing functions available.
- `func *Main():` begins the program's entry function. Execution starts here
  after the runtime has initialized the program.
- `PrintLn(...)` writes text to the terminal and ends the output line.
  `endfunc` closes the function.

Text inside double quotes is a string: a sequence of characters representing
text. The indentation makes the function body easier to read.

## Compile and Run

Open a terminal in the directory containing `hello.dq` and run:

```text
dq-run hello.dq
```

The program prints:

```text
Hello from DQ!
```

`dq-run` invokes the compiler, creates an executable, and starts it. If
compilation reports an error, correct the indicated line in your source file,
save it, and run the command again.

You can also compile and run as separate steps. On Linux:

```bash
dq-comp -g -O0 hello.dq
./hello
```

On Windows, in Command Prompt or PowerShell:

```bat
dq-comp -g -O0 hello.dq
.\hello.exe
```

The compiler produces `hello` on Linux and `hello.exe` on Windows. The `-g`
option includes debugging information, and `-O0` disables optimization so the
generated code is easier to debug. When called without compiler options,
`dq-run` uses both options automatically.

## Print Values

Formatting lets you insert values into a message. Each `{}` is replaced by the
next value in the list between `[` and `]`:

```dq
use print

func *Main():
    var name : str = "DQ"
    var count : int = 3
    PrintLn("Hello, {}! Count = {}", [name, count])
endfunc
```

Here, `var` declares a variable: a named place to store a value. `str` is a
string type, and `int` is an integer type for whole numbers. This program prints
`Hello, DQ! Count = 3`.

## Program Arguments and Exit Status

Programs can receive arguments from the command line. Save this example as
`args.dq`:

```dq
use print

func *Main():
    for i : int = 1 count ArgCount() - 1:
        PrintLn("Argument {}: {}", [i, ArgStr(i)])
    endfor
endfunc
```

Run it with two arguments:

```text
dq-run args.dq -- one two
```

The output is:

```text
Argument 1: one
Argument 2: two
```

`ArgCount()` includes the program name at index `0`, so this example starts at
index `1`. `ArgStr(i)` reads the argument at that index. These functions are
available through the implicit `sys` runtime interface.

Compiler options go before the source filename. Program arguments go after it;
the optional `--` separator makes the boundary explicit.

`*Main` has no return value. The program normally exits with status `0`, which
means success. Set the global `exit_code` variable to another value before
leaving `*Main` to report failure, for example `exit_code = 1`.

## Continue Learning

Read the [DQ Programming Language overview](../020-language/index.md) for the
language topics in book order. The
[Standard Packages](../040-packages/index.md) provide printing, file access,
JSON handling, networking, and other services.

For build options and larger programs, continue with
[Compiler Usage and Options](../030-compiler/040-compiler-usage-and-options.md)
and [Project Files](../030-compiler/050-project-files.md).
