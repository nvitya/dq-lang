# DQ-lang Project Introduction

DQ is a **general-purpose** and **human-friendly** programming language.

**General-purpose**, because it is intended to cover many use cases, including scripts and tools, server applications, embedded applications, GUI applications, and games.

**Human-friendly**, because DQ syntax and library interfaces are designed to make code easy to read and easy to write by humans.

The DQ-lang Project is developed to offer a serious alternative for Python and C++. The DQ Language syntax is a kind of mixture of Pascal, Python and C/C++. The name comes from here: the D is the next letter after C and Q is the next after P.

The DQ source code files (.dq files) are translated to directly executable machine code, so the DQ applications run very fast, matching the speed of C++, Rust. For the machine code translation the DQ compiler uses the well-tested LLVM framework, wich also allows to target different CPU architectures, like x86, ARM32, ARM64, ARM Cortex-M, Risc-V, WASM. The DQ compiler allows you to create Windows, Linux and bare-metal applications, also with cross-compiling.

## Current State

The compiled DQ programs on Windows and Linux use the standard and well-tested C library for low-level system functions. Combined with the reliable LLVM code generator and excessive compiler tests, the DQ compiler and its compiled applications are pretty stable already.

The DQ standard library offers user-friendly, object based interface for several programming tasks, including database handling, threading, http serving, and JSON handling. As DQ is a new languge (2026) the library support is not complete though. DQ has a very flexible module system and simple way importing functions from external C compatible libraries, which provides unlimited possibilities at application development.

## DQ Language Self-Introduction

```
use print

func *Main():
    PrintLn('DQ Short Introduction')

    if 3 / 2 == 1.5  and 3 div 2 == 1  and uint32(3) - uint32(5) < 0:
        PrintLn('DQ is human friendly.')
    endif

    var sarr : [5]int = [2, 3, 5, 7, 11]
    Print('Static array:')
    for i : int = 0 count sarr.length:
        Print(' {}', [sarr[i]])
    endfor
    PrintLn()

    var darr : [*]float = [2, 3.14, 7 / 3]
    darr.Append(13 / 2)
    darr.PopFirst()
    Print('Dynamic array:')
    for f :? in darr:
        Print(' {}', [f])
    endfor
    PrintLn()

    var ds : str = 'This is a'
    ds.Reserve(1024) // reserve at least 1024 bytes to avoid re-allocations
    ds.Add(' dynamic string.')
    var dpos : int = ds.IndexOf('dynamic')
    ds.Insert(dpos, 'heap allocated ')
    PrintLn(ds)

    PrintLn('String slicing: ds[2:6]="{}", ds[$end-5:]="{}"', [ds[2:6], ds[$end-5:]])

    var x : int = 7
    PrintLn('original x = {}', [x])
    var xptr : ^int = %x   // %x = address of x
    ref xref = x
    xref = 42
    if xptr^ == x  and xref == x:
        PrintLn('Supports pointers and references. x = {}', [x])
    endif

    PrintLn('DQ has much more features. See further examples.');
endfunc
```

DQ supports objects with inheritance and virtual functions. See this example:

(dq_intro_3.dq)


## Highlighted features

* `3 / 2 == 1.5` and no number hintings are required (like 1.0, 1.0f, 2ul etc.)
* Single purpose operator symbols and well-designed operator precedence. The need to use parentheses reduced drastically, e.g. <br> `if b & 0x80 == 0 and not 2 <= b < 10:  ...`
* Statically, strictly typed, with no implicit number to boolean conversion
* Objects with inheritance, virtual functions and properties
* Object functions don't need to use `self.` for member accesses
* Semi-automatic memory management with embeddable objects (`<-`) and `autofree` types
* Exceptions for runtime error management, with no-cost happy path
* Dynamic arrays and dynamic strings
* Excessive built-in string functions, supporting no-heap strings for small embedded systems
* Pointers, pointer arithmetic and inline assembly functions for low-level programming
*


[Feature Highlights](feature-highlights.dq)


---


## Highlighted features
* Compiled to machine code that can run at the same speed as C/C++
* Statically, strictly typed
* Mostly uses well-tested concepts from other popular languages
* `3 / 2 * 10 == 10 * 3 / 2` is true
* No implicit conversions between numbers and booleans
* Pointers and pointer arithmetic
* Manual memory management (no garbage collector)
* Objects with inheritance, virtual functions and properties
* Object functions don't need to use `self.` for member accesses
* Exceptions for runtime error management
* Dynamic arrays and dynamic strings
* Simple, fast compile + run

## Quick Look

For a quick look at DQ syntax and features, start with the
[`nanonet/nano_sockets.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/nanonet/nano_sockets.dq)
module. It shows real DQ code in a compact, practical module.

## Motivation

What was my motivation to create the DQ language? See the [Motivation](motivation.md) page.

## Current Status, Roadmap


The DQ compiler and libraries are currently in Pre-V1 phase. The DQ compiler is already pretty reliable, because it uses LLVM for the code generation and libc for the low-level runtime functions. There are some applications running developed in DQ already. More and more libraries and applications will be developed, but the standard library set is somewhat limited yet.

The DQ compiler standard package already contains user-friendly interfaces for many tasks, including:

files
JSON
async sockets
http serving
database
threading

It is very easy to use C libraries from DQ, so if there is no existing DQ standard function for a specific task, you can create a module with the missing C interface bindings.

Any help welcomed in DQ standard package development too.

---


## DQ Compiler and Libraries

I implemented a compiler for the DQ language in C++ using LLVM. As I am not an expert in compiler development,
I used AI (GPT-5, Gemini pro, Opus) for the design and implementation, in a controlled way.

The DQ compiler C++ code is restricted only to the source code parsing and machine code generation using the LLVM C++ interfaces.

The compiled applications are linked against the libc to use the low-level system functions.

The DQ specific runtime library is implemented in DQ (which can be found under the
[stdpkg/rtl](https://github.com/nvitya/dq-lang/blob/main/stdpkg/rtl)).

## Getting Started

To get the DQ compiler and start experimenting with the language follow the instructions in the
[Getting Started](getting-started.md) page.

## Project Page

The DQ language specification and the DQ compiler are available on GitHub:

[github.com/nvitya/dq-lang](https://github.com/nvitya/dq-lang)

## Discussions

The DQ community subreddit is available at:

[**https://www.reddit.com/r/dqlang/**](https://www.reddit.com/r/dqlang/)

Use it for questions, feedback, language-design discussions, examples, and general conversation about the DQ programming language and compiler.

For confirmed bugs and concrete feature requests, please use the [GitHub issue tracker](https://github.com/nvitya/dq-lang/issues)
