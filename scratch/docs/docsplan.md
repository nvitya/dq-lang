# DQ Documentation Plan

## Index Page

### Short introduction, max 5 lines:
* Compiled to machine code, that can run with the same speed as C/C++
* Statically, strictly typed
* Compact, human-readable
* Universal (database and embedded support is also planned)
* Mostly uses well-tested concepts from other popular languages

### Highlighted features, max 7
* `3 / 2 * 10 == 10 * 3 / 2` is true
* Strict `bool` type
* Pointers and pointer arithmetic
* Manual memory management (no garbage collector)
* Objects with inheritance, virtual functions and properties
* Object functions don't need to use `self.` for member accesses
* Exceptions for runtime error management
* Dynamic arrays and dynamic strings
* Easy library development
* Simple, fast compile + run

### Motivation (Why I made the DQ compiler)

### Main Links
* Getting started
* Language presentation by code examples
* Full language documentation
* Github project page


## Language Introduction

### Main Language Features

* All symbols in the DQ language are case sensitive
* The functions and methods start with a capital letter (e.g. `SizeOf()`)
* Variables, function arguments are lower-case (separated with underscore)
* Blocks are started with `:` and closed with and `endxxx` word, like `if <condition> : ... endif`. Proper block indentation is recommended, but not required
* Types specified with `:` after the symbol name (like in Python or Pascal)
* The function return value specified with `->` after the function arguments (like in Python)
* C-style comments: single-line comments starting with `//`, multi-line comments surrounded with `/*` and `*/`
* Local and global variables are declared starting with the `var` keyword
* Strings are delimited either with single-quotes (`'`) or double quotes (`"`), like in JavaScript or Python
* Hexadecimal numbers are written in C-format: e.g. `0xFF0123AD`
* C-style compiler directives with the `#` symbol, but multiple directives in single line also allowed with the `#{...}` form
* Supporting conditional compilation with `#if`/`#ifdef`/`#elif`/`#else`/`#endif`, like in C
* No preprocessor macro support. Preprocessor symbols are accessible with `@def.`. e.g.: `@def.PTRSIZE`
* No semicolons (`;`) required to close the statements
* `^` symbol is used for pointer type definition and dereferencing
* Generic, untyped pointers declared with `pointer`. Excplicit casting required before using these.
* The `[]` index operator on the typed pointers do not dereference, unlike in C
* `null` is used for null pointer value
* `&` operator is used for getting the address of a variable
* Type casting uses the Delphi / C++ form: `T(x)`
* Variable assignments are done with the `=` symbol and it cannot be used in another expressions
* `==` is used for equality checking, `<>` or `!=` are for non-equality checking
* The `\` operator always results to floating point, floating point to integer conversions must be written explicitly using the `Round()`, `Floor()` or `Ceil()` built-in functions.
* Integers may be converted automatically to floating point, when necessary
* For integer division/modulo the `IDIV`/`IMOD` operator should be used
* `and`, `or` and `not` are logical operators, for bitwise calculations use the uppercase versions: `AND`, `OR`, `XOR`, `NOT`. The bitwise operators have much higher precedence than the logical operators.
* The DQ operator precedence and type check/conversion rules allows to write the most frequent expressions without any parantheses
* No implicit (automatic) conversions between numeric and `bool` types (`<> 0` must be used)
* `iif(condition, trueexpr, falseexpr)` built-in macro for inline-if expressions (analogue to the `condition ? trueexpr : falseexpr` in C/C++)
* `while` loops
* `for` loops with multiple flavors: `to`, `downto`, `count`, `downcount`, `while`. `in` is planned too.
* Function, variable, statement attributes are denoted with the `[[attribute]]` form, like `[[virtual]]` or `[[external('printf')]]`
* Object variables are references, objects can be allocated on the heap, stack, main data segment or embedded to another object
* Star functions (function names marked with `*`) are special-purpose functions, like `*Main()` or `*Create()`
* Fixed array type notation: `[n]T`, dynamic array notation: `[*]T`.
Inferred array length from array literal: `var a : [?]int = [1, 2, 3]`
* Array literal notation: `[1, 2, 3]`
* Built-in `cchar` and `cstring(n)` type to ease the zero-terminated C-style string handling.
* Dynamic (heap-allocated) string handling with the `str` type
* `anyvalue` and `[]anyvalue` types for handling variable arguments (e.g. `Print('{}: {}, {}', [1, 'abc', 3.14])`)
* Objects support single inheritance only
* At the object functions no `self` argument definition required and no `self.` prefix required to access the object members.
* Virtual functions must be defined with `[[virtual]]` and `[[override]]` attributes
* Support for function overloading, but the functions must be defined with the `[[overload]]` attribute
* Property support for objects (e.g. `myobj.myprop = 1` calls `myobj.SetMyProp(1)`), like in Delphi
* Enumeration type with context awareness (`c = red` instead of `c = NColor.red`)
* The `implementation` keyword sepearates the module public interface from the private interface (like in Pascal)
* DQ supports calling C functions with variable number of arguments like `printf()`
* `#linklib('libname')` directive (in .dq source code) to link to a library
* Flexible `use` statement for module "import" and namespace merging. By default `use mymod` merges all interface symbols of the `@mymod` into the current scope. `use mymod --` does not merge. Furthermore `only`, `exclude` and `reexport` options are also supported.
* Every used module gets its own namespace identified by a single word (selectable with `as`). The `@mymod.` prefix can be used for fully qualified access
* Object member functions use a restricted value-symbol scope. Symbols outside of the object scope must be addressed with the `@` namespace qualifier (`@.` can be used to access the global scope of the current module)
* Dynamic object casting with `TryCast(OSecond, x)` and `var o2 : OSecond tryfrom x`. (The result is null if the x is not compatible with OSecond)
* Type inference is not implemented, but, if it will be implemented, it will be marked with the `?` symbol:
  `var x : ? = 3` or `var x ?= 3`
* Built-in integer types: `int`, `uint`. These are always the with of the `pointer`. Use `int32`, `uint16` etc. for specific widths. The `byte` alias also available and same as `uint8`.
* Built-in floating point types: `float64`, `float32`. The `float` is alias to `float64` on platforms with HW double precision floating point support, `float32` when only 32-bit HW floating support is available.
* Type aliases with `type TMyFloat = float32`
* Constants defined with `const`: `const MAXLEN : int = 3`
* Function references: `type FCallBack = function(a1 : int, a2 : ref SDataStruct) -> int`
* Object function references: `type FObjCallBack = function(a1 : int, a2 : ref SDataStruct) -> int of object`
* Function argument modes: by-value, `ref`, `refin`, `refout`, `refnull`
* Pointers to a structure automatically dereferenced when its members addressed with the `.`

### DQ Compiler Features

* Written in C++ using the LLVM library
* Uses libc for basic system functions
* The DQ runtime library is written in DQ and distributed in source-code form
* No make files required
* DQ source code file extension: `.dq`
* Fast compilation using a single-pass forward parser (no separate pre-processing or tokenizer)
* gcc-like compiler switches (`-g`, `-O1`)
* Debug info generation with `-g`
* LLVM IR printing with `-ir`
* DQ modules are precompiled into `.dqm` files, which are standard object files containing a special `.dqm_if` section. When using a pre-compiled module just the `.dqm_if` (binary stored AST) block required to be loaded
* The precompiled DQ modules are put into the `.dqbuild` subdirectory at the main source code file
* Backtrace printing with function names and line number on un-caught exceptions and signals
* `dq-run` utility to compile and run (by default non-optimized and debugger friendly = `-g -O0`)
* Highly parallelized DQ compiler autotest utility (`dqatrun`), written in C++


## DQ Operators and Operator Precedence


