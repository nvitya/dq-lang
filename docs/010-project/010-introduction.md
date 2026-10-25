# DQ-lang Project Introduction

DQ is a **general-purpose** and **human-friendly** programming language.

**General-purpose**, because it is intended to cover many use cases, including scripts and tools, server applications, embedded applications, GUI applications, and games.

**Human-friendly**, because DQ syntax and library interfaces are designed to make program source code easy to read and write for people.

The DQ-lang Project is being developed to offer a serious alternative to Python and C++. The DQ language mostly uses established concepts from other programming languages. Its syntax combines elements of Pascal, Python, and C/C++. The name reflects these influences: D is the letter after C, and Q is the letter after P.

DQ source files (.dq files) are translated into directly executable machine code, so DQ applications run very fast, matching the speed of programs written in C++ or Rust. For machine code generation, the DQ compiler uses the well-tested LLVM framework, which also supports different architectures and targets, such as x86, ARM32, ARM64, ARM Cortex-M, RISC-V, and WebAssembly (WASM). The DQ compiler allows you to create applications for Windows, Linux, and bare-metal targets, including through cross-compilation.

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

    PrintLn('DQ has many more features. See further examples.');
endfunc
```
Source: [https://github.com/nvitya/dq-lang/blob/main/examples/dq_intro_1.dq](https://github.com/nvitya/dq-lang/blob/main/examples/dq_intro_1.dq)

This example covers only a small subset of the capabilities of the DQ language. One very important feature is support for **objects with inheritance and virtual functions**. For a quick example, see the [Object Example Code](../020-language/120-objects-and-properties.md#object-example-code) in the [Objects and Properties](../020-language/120-objects-and-properties.md) chapter.

For a more complete list of features, read [Features and Goals](../010-project/020-features-and-goals.md).

For a complete description of the DQ language, see the [DQ Language Reference](../020-language/index.md).

## Getting Started

To get the DQ compiler and start experimenting with the language, follow the instructions on the
[Getting Started](030-getting-started.md) page.

## Project State

Compiled DQ programs for Windows and Linux use the standard, well-tested C library for low-level system functions. With the reliable LLVM code generator and extensive compiler testing, the DQ compiler and its produced applications are already fairly stable.

During the development of DQ applications, bugs and language defects have been detected and fixed. AI assistance is used - under strict supervision - in compiler development, making it possible to fix bugs or implement changes very quickly.

**Using DQ in critical applications is not yet recommended because it has not undergone sufficient field testing.** (The DQ-lang project started in January 2026 and became ready for application development only around September 2026.)
**Every new project must start somewhere**, and building a supporting community takes time.

You can already use DQ for less critical applications and tools, and I recommend doing so.

The DQ [standard packages](../040-packages/index.md) offer user-friendly, object-based interfaces for several programming tasks, including database handling, threading, HTTP serving, and JSON handling. Since DQ is a new language (2026), library support is still incomplete. DQ has a very flexible module system and a simple way to import functions from external C-compatible libraries, which opens unlimited possibilities for application development.

## Project Page

The DQ-lang project is available on GitHub at:

[github.com/nvitya/dq-lang](https://github.com/nvitya/dq-lang)

## Discussions

For questions, feedback, language-design discussions, examples, and general conversation about the DQ programming language and compiler, you can use the discussion page on GitHub:

[**https://github.com/nvitya/dq-lang/discussions**](https://github.com/nvitya/dq-lang/discussions)

You can also use the DQ community subreddit:

[**https://www.reddit.com/r/dqlang/**](https://www.reddit.com/r/dqlang/)


For confirmed bugs and concrete feature requests, please use the [GitHub issue tracker](https://github.com/nvitya/dq-lang/issues).


## Supporting the DQ-lang Project

**Please support the DQ-lang project by trying it out, writing and sharing DQ applications, and giving feedback!**

## Further Reading

 * [Language Reference](../020-language/index.md)
 * [Compiler and Tools](../030-compiler/index.md)
 * [Standard Packages](../040-packages/index.md)
