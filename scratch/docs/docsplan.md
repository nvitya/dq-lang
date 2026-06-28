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

### Generic Properties

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
* No semicolons (`;`) required to close the statements
* `^` symbol is used for pointer type definition and dereferencing
* `null` is used for null pointer value
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
* Function, variable, statement attributes are denoted with the `[[attribute]]` form, like `[[virtual]]` or `[[external('printf')]]`
* Object variables are references, objects can be allocated on the heap, stack, main data segment or embedded to another object
* Star functions (function names marked with `*`) are special-purpose functions, like `*Main()` or `*Create()`
* Fixed array type notation: `[n]T`, dynamic array notation: `[*]T`
* Array literal notation: `[1, 2, 3]`
* Built-in `cchar` and `cstring(n)` type to ease the zero-terminated C-style string handling.
* Dynamic (heap-allocated) string handling with the `str` type
* `anyvalue` and `[]anyvalue` types for handling variable arguments (e.g. `Print('{}: {}, {}', [1, 'abc', 3.14])`)
* Objects support single inheritance only
* Property support for objects (e.g. `myobj.myprop = 1` calls `myobj.SetMyProp(1)`), like in Delphi
* Enumeration type with context awareness (`c = red` instead of `c = NColor.red`)
* The `implementation` keyword sepearates the module public interface from the private interface (like in Pascal)
* DQ supports calling C functions with variable number of arguments like `printf()`

### Language Features

### DQ Compiler Properties

* Written in C++ using the LLVM library
* Uses libc for basic system functions
* Fast compilation using a single-pass forward parser (no separate pre-processing or tokenizer)
* DQ modules are precompiled into `.dqm` files, which are standard object files containing a special `.dqm_if` section. When using a pre-compiled module just the `.dqm_if` (binary) block required to be loaded
* The DQ runtime library is written in DQ and distributed in source-code form
* Backtrace printing with function names and line number on un-caught exceptions and signals
* `dq-run` utility to compile and run
* Highly parallelized DQ compiler autotest utility (`dqatrun`), written in C++


## DQ Operators and Operator Precedence


