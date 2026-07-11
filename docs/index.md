# What is DQ?

DQ is a **universal** and **human-friendly** programming language.

**Universal**, because it is intended to cover many use cases, including scripts and tools, server applications, embedded applications, GUI applications, and games.

**Human-friendly**, because DQ syntax and library interfaces are designed to make code easy to read and easy to write by humans.

DQ is a mix of Pascal, Python and C/C++. The name comes from here: the D is the next letter after C and Q is the next after P.

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

# Quick Look

For a quick look at DQ syntax and features, start with the
[`nanonet/nano_sockets.dq`](https://github.com/nvitya/dq-lang/blob/main/stdpkg/nanonet/nano_sockets.dq)
module. It shows real DQ code in a compact, practical module.

# Motivation

What was my motivation to create the DQ language? See the [Motivation](motivation.md) page.

# DQ Compiler and Libraries

I implemented a compiler for the DQ language in C++ using LLVM. As I am not an expert in compiler development,
I used AI (GPT-5, Gemini pro, Opus) for the design and implementation, in a controlled way.

The DQ compiler C++ code is restricted only to the source code parsing and machine code generation using the LLVM C++ interfaces.

The compiled applications are linked against the libc to use the low-level system functions.

The DQ specific runtime library is implemented in DQ (which can be found under the
[stdpkg/rtl](https://github.com/nvitya/dq-lang/blob/main/stdpkg/rtl)).

# Getting Started

To get the DQ compiler and start experimenting with the language follow the instructions in the
[Getting Started](getting-started.md) page.

# Project Page

The DQ language specification and the DQ compiler are available on GitHub:

[github.com/nvitya/dq-lang](https://github.com/nvitya/dq-lang)

# Language Documentation

For more complete language documentation read the following pages:

* [Basics](language/basics.md)
* [Types](language/types.md)
* [Expressions](language/expressions.md)
* [Statements](language/statements.md)
* [Functions](language/functions.md)
* [Objects](language/objects.md)
* [Modules](language/modules.md)
* [Memory and Pointers](language/memory-and-pointers.md)
* [Attributes and Directives](language/attributes-and-directives.md)
* [Inter-Operation](language/interop.md)
