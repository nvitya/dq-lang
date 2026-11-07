# Features and Design Goals

DQ aims to combine the convenience of a language used for everyday tools with
static type checking, native compilation, fast execution and direct access to the machine.
The goal is to make programs easy to read and maintain as they grow from small
utilities into larger applications.

Most of DQ's individual features are established ideas from other programming languages.
What distinguishes the
language is the combination: readable syntax, deliberate expression rules,
objects with inheritance, automatic cleanup, exceptions, and low-level control
within one language.

## Motivation

DQ grew out of experience with Pascal application development, C++ embedded
software, and a large Python project for device production testing. The
motivation was practical: keep the convenience of writing small programs,
catch more mistakes before running them, and retain the ability to work with
hardware and existing native libraries.

Those needs connect application programming and systems programming. A tool
might start by reading a configuration file, then grow to communicate with a
device, store measurements in a database, and serve a web interface. DQ's goal
is to support that growth without requiring a different language for the parts
that need native performance or direct memory access.

## Familiar Ideas, Deliberate Choices

DQ draws on four main language traditions, combining features that support
human-readable code, predictable calculations, and direct control of the machine.

These ideas are adapted to work together under DQ's own rules. Human readability
guides the syntax, while explicit types and storage choices keep the program's
behavior understandable.

### Features Adopted from Python

- Human-readable source code.
- Readable blocks introduced by `:`. DQ adds explicit closers such as `endif`
  and `endfunc`, preserving the visual structure while making indentation optional.
- Statements on separate lines without required semicolons, keeping the source
  easy to read with little punctuation.
- Flexible module system, with non-merging, merging imports and re-exports
- Excellent, user-friendly, modern APIs
- String, array slicing

### Features Adopted from Pascal / Delphi

- Human-readable source code.
- Strict type-safety.
- Implicit function `result`
- Floating-point division with `/`, with a separate operator for integer division.
- Explicit rounding when converting floating-point values to integers.
- Boolean values distinct from numbers, requiring explicit comparisons in conditions.
- Object properties, single inheritance, and exception handling.
- Module interface/implementation split
- Elegant variable arguments and anyvalue (variants)
- Function references, object method references
- Simple and fast build system

### Features Adopted from C / C++

- Compact expression syntaxes.
- Pointers, structures, and unions for direct control over memory and data layout.
- Embeddable objects.
- Fixed-width numeric types and bitwise operations for programming close to the machine.
- Direct access to C-compatible libraries.
- C-style comments and conditional-compilation directives.
- Text formatting
- Efficient data processing.

### Features Adopted from JavaScript

- String and array properties accessed directly on the value, such as `text.length`.
- String and array methods called on the value, such as `text.IndexOf("word")` and
  `text.StartsWith("prefix")`, keeping text operations easy to discover and read.

See [String Functions](../020-language/075-string-functions.md) for DQ's string
properties and methods.

## Some Distinctive DQ Design Choices

- **Carefully selected operator precedence:** The precedence rules reduce the
  need for parentheses in common expressions, like this:
  `flags & mask <> 0 and count > 0`
- **Separate logical and bitwise operators:** `and`, `or`, and `not` express
  Boolean logic; `&`, `|`, `~`, and `xor` express bit operations. The distinct
  notation makes the intended operation easy to recognize.
- **Explicit special functions:** `*` marks functions with language-defined
  roles, such as `*Main()`, `*Create()`, and `*Destroy()`. The marker makes those
  roles visible and lets the compiler reject an unrecognized special function
  name, including a misspelling.
- **Explicit object embedding:** `<-` constructs an embedded object whose
  lifetime follows its containing scope or object. For example,
  `var file <- OFile()` makes storage and automatic cleanup visible in the
  declaration.
- **Embedded string:** `embstr(n)` for simple, embedded-friendly, heap-less string handling
- **Conditional compilation aware of program symbols:** Directives such as
  `#if` and `#ifdef` can use visible program symbols as well as compile-time
  defines. For example, `#ifdef somelocalvar` checks whether a local variable
  exists in the current scope.
- **Customizable runtime:** DQ packages and runtime libraries are distributed
  as source code and compiled as part of the application build. Compile-time
  defines can control their conditional compilation, allowing supported
  features and settings to be tailored to the application and target.


## Readable Code with Explicit Structure

DQ favors syntax that makes the purpose of a statement visible. Types follow
names, function return types follow `->`, and blocks end with words such as
`endif` and `endfunc`. Indentation is recommended but does not define the blocks.
Semicolons are unnecessary when statements are on separate lines.

```dq
func Clamp(value : float, lower : float, upper : float) -> float:
    if value < lower:
        return lower
    elif value > upper:
        return upper
    endif
    return value
endfunc
```

Naming conventions distinguish functions such as `ReadSensor` from values such
as `temperature`. Object methods can access their members without repeating a
`self.` prefix. The intention is to keep frequently used code easy to scan,
while retaining clear declarations and boundaries.

See [Basic Syntax](../020-language/010-basic-syntax.md) and
[Functions](../020-language/060-functions.md).

## Expressions That Say What They Mean

Small differences in arithmetic and conversion rules can prevent common
mistakes. DQ makes several explicit choices:

- `/` performs floating-point division: `3 / 2` is `1.5`. Use `div` when integer
  division is intended.
- Conditions require Boolean values. A number or pointer does not implicitly
  become `true` or `false`; write a comparison such as `count <> 0`.
- Floating-point values do not implicitly become integers. Choose the rounding
  behavior with `Round`, `Floor`, `Ceil`, or `Trunc`.
- Assignment is a statement, so an accidental `=` cannot act as a comparison
  inside a condition.
- Logical operations use `and`, `or`, and `not`, separately from bitwise `&`, `|` and `~`
  operations.

For a C or C++ programmer, these rules change some familiar defaults. For a
Python programmer, floating-point division is familiar, while explicit Boolean
conditions are a stricter requirement. The goal is to make the intended
operation apparent in the source.

See [Expressions and Operators](../020-language/040-expressions-and-operators.md).

## Convenient Data with Control over Storage

Strings, dynamic arrays, slices, and formatted text are part of everyday DQ
programming. They do not require writing allocation and resizing code for each
use. Fixed arrays, structures, embedded objects, and fixed-storage strings are
also available when storage size and placement matter.

DQ combines automatic storage management with explicit ownership. Strings and
dynamic arrays manage their storage; embedded objects are destroyed with their
containing scope or object. Autofree owners release heap allocations when they
are replaced or leave scope, including during exception handling.

This provides deterministic cleanup without a tracing garbage collector.
Compared with a garbage-collected object model, the programmer has more control
over when resources are released, but also more responsibility: borrowed
references must not outlive their storage, and manually allocated objects need
an owner or an explicit release. DQ's raw pointers and manual allocation do not
provide a general memory-safety guarantee.

See [Memory Management and Autofree](../020-language/130-memory-management-and-ownership.md).

## Objects and Error Handling for Application Code

DQ supports single inheritance, virtual methods, constructors, destructors, and
properties. These let a program express related kinds of objects and expose
simple interfaces while keeping implementation details inside the object.
They coexist with plain functions and structures, so object-oriented design
can be used where it helps.

Exceptions keep the normal sequence of operations readable. A function can
open a file and write several values without repeating an error-result check
after every operation. Failures reach a matching handler, and an unhandled
exception normally produces a message and backtrace before the program exits.
Automatic cleanup and `finally` blocks handle resources along the way.

On supported hosted targets, unwind tables provide zero-cost exception handling:
normal calls do not need an extra exception check. Throwing and handling an
exception still involves runtime work.

See [Objects and Properties](../020-language/120-objects-and-properties.md) and
[Exceptions and Error Handling](../020-language/140-exceptions-and-error-handling.md).

## Modules and a Short Build Workflow

A DQ module is a source file with a public interface and an optional private
`implementation` section. Imports can expose selected names directly or require
namespace qualification. This gives growing programs explicit boundaries
without maintaining separate C-style header files.

For a small program, `dq-run app.dq` compiles and runs it. The compiler follows
module dependencies and reuses compiled artifacts, so a basic DQ program does
not need a separate build script. Project files provide configuration as the
build grows.

The aim is a short edit–run cycle with the checks of a compiled language.
Compilation time and execution speed depend on the program and build settings;
using LLVM is a foundation for optimization, not a guarantee about benchmarks.

See [Modules and Packages](../020-language/170-modules-and-packages.md) and
[Compiler and Tools](../030-compiler/index.md).

## Native Libraries and Embedded Targets

DQ can call C-compatible functions, including variadic functions, and link to
existing native libraries. Pointers, explicit-width numeric types, structures,
unions, and assembly functions support work close to the hardware.

The compiler uses LLVM for code generation and supports hosted and bare-metal
targets. The same language can serve application and embedded development, but
the available runtime facilities differ: a bare-metal program needs its own
platform support, and not every target supports exceptions or every package.

See [C Interoperability](../020-language/200-c-interoperability.md) and
[Targets and Cross-Compilation](../030-compiler/070-targets-and-cross-compilation.md).

## Goals and Current Scope

DQ's direction is to make ordinary programming tasks straightforward while
keeping performance, storage, and resource lifetimes understandable. Readable
source, useful diagnostics, consistent library interfaces, and a manageable
build process are all part of that goal.

Language features are only part of choosing a tool. Existing libraries,
developer experience, platform support, and long-term maintenance matter too.
DQ is a young project, and its ecosystem and field experience are still growing.
For its current development state, see the
[Project Introduction](010-introduction.md#project-state); for available
libraries, see [Standard Packages](../040-packages/index.md).
