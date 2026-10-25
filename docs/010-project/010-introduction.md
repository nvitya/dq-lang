# DQ-lang Project Introduction

DQ is a **general-purpose** and **human-friendly** programming language.

**General-purpose**, because it is intended to cover many use cases, including scripts and tools, server applications, embedded applications, GUI applications, and games.

**Human-friendly**, because DQ syntax and library interfaces are designed to make code easy to read and easy to write by humans.

The DQ-lang Project is developed to offer a serious alternative for Python and C++. The DQ Language mostly applies established concepts from other programming languages, its syntax is a kind of mixture of Pascal, Python and C/C++. The name comes from here: the D is the next letter after C and Q is the next after P.

The DQ source code files (.dq files) are translated to directly executable machine code, so the DQ applications run very fast, matching the speed of C++, Rust. For the machine code translation the DQ compiler uses the well-tested LLVM framework, wich also allows to target different CPU architectures, like x86, ARM32, ARM64, ARM Cortex-M, Risc-V, WASM. The DQ compiler allows you to create Windows, Linux and bare-metal applications, also with cross-compiling.

## DQ Language Self-Introduction

```dq
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
    ds.Add(' dynamic string.')
    var dpos : int = ds.IndexOf('dynamic')
    ds.Insert(dpos, 'heap allocated ')
    PrintLn(ds)

    PrintLn('String slicing: ds[2:6]="{}", ds[$end-5:]="{}"',
        [ds[2:6], ds[$end-5:]] )

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
Source: [https://github.com/nvitya/dq-lang/blob/main/examples/dq_intro_1.dq](https://github.com/nvitya/dq-lang/blob/main/examples/dq_intro_1.dq)

This example covers only a small subset of the capabilities of the DQ language. One of the very important feature is supporting **objects with inheritance and virtual functions**. See the [Object Example Code](../020-language/120-objects-and-properties.md#object-example-code) in the [Objects and Properties](../020-language/120-objects-and-properties.md) for a quick example.

For the more complete feature listing read the [Features and Goals](../010-project/020-features-and-goals.md).

For the complete DQ language description see the [DQ Language Reference](../020-language/index.md)

## Getting Started

To get the DQ compiler and start experimenting with the language follow the instructions in the
[Getting Started](030-getting-started.md) page.

## Project State

The compiled DQ programs on Windows and Linux use the standard and well-tested C library for low-level system functions. Combined with the reliable LLVM code generator and excessive compiler tests, the DQ compiler and its compiled applications are pretty stable already.

During DQ application development some bugs and language defects were detected and fixed. For the compiler development AI assistance (with strict control) is used, which makes possible to fix bugs or apply changes very quickly.

**It is not recommended yet to use DQ in critical applications, due the lack of enough field-testing.** (DQ-lang project started in January 2026, it became application ready only around September 2026.)
**Every new project must start somewhere**, to build up the supporting community takes some time.

You can already use - and I recommend to use - DQ for some less critical applications, or tools.

The DQ [standard packages](../040-packages/index.md) offers user-friendly, object based interface for several programming tasks, including database handling, threading, http serving, and JSON handling. As DQ is a new languge (2026) the library support is not complete though. DQ has a very flexible module system and simple way importing functions from external C compatible libraries, which provides unlimited possibilities at application development.

## Project Page

The DQ-lang project is available on GitHub at:

[github.com/nvitya/dq-lang](https://github.com/nvitya/dq-lang)

## Discussions

For questions, feedback, language-design discussions, examples, and general conversation about the DQ programming language and compiler you can use the discussion page on Github:

[**https://github.com/nvitya/dq-lang/discussions**](https://github.com/nvitya/dq-lang/discussions)

Or the DQ community subreddit:

[**https://www.reddit.com/r/dqlang/**](https://www.reddit.com/r/dqlang/)


For confirmed bugs and concrete feature requests, please use the [GitHub issue tracker](https://github.com/nvitya/dq-lang/issues)


## Supporting the DQ-lang Project

**Please support the DQ-lang project by trying it out, writing and sharing DQ applications and giving feedback!**

## Further Readings

 * [Language Reference](../020-language/index.md)
 * [Compiler and Tools](../030-compiler/index.md)
 * [Standard Packages](../040-packages/index.md)
