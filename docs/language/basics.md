# DQ Language Basics

This page describes the basic source syntax of the DQ language as implemented by
the current compiler.

## Source Files

DQ source files use the `.dq` extension. A source file is a module. A program
entry point is normally declared with the special function `*Main`.

```dq
function *Main() -> int:
    return 0
endfunc
```

The compiler is case-sensitive. By convention, functions and methods start with
an uppercase letter when they are part of the public API, while variables and
parameters use lowercase names, often with underscores.

## Type Annotations

Type annotations are written after the declared name with `:`.

```dq
var count : int = 0
const LIMIT : int = 100
function Add(a : int, b : int) -> int
```

Function return types are written after the parameter list with `->`.

```dq
function IsReady() -> bool:
    return true
endfunc
```

## Statements and Blocks

Statements do not require semicolons. A semicolon may be used as an optional
statement separator.

The normal block form starts with `:` and ends with a matching `end...` keyword.

```dq
if value > 0:
    value -= 1
endif

while value > 0:
    value -= 1
endwhile
```

Brace blocks are also accepted by the compiler.

```dq
function Sum3() -> int
{
    result = 0
    for i : int = 1 to 3
    {
        result += i
    }
}
```

Indentation is recommended, but block structure is determined by the block
tokens, not by indentation.

## Comments

DQ uses C-style comments.

```dq
// Single-line comment

/*
   Multi-line comment
*/
```

## Names

All symbols are case-sensitive.

Names may refer to variables, constants, functions, types, modules, object
members, properties, enum values, and namespaces. The exact lookup rules depend
on the scope:

- normal functions see local symbols, module symbols, and imported symbols;
- object methods first see object members and methods;
- names outside object scope can be reached with namespace qualification such as
  `@.Name` or `@module.Name`.

## Literals

Integer literals are written in decimal or C-style hexadecimal form.

```dq
var a : int = 123
var b : uint = 0xFF00AA55
```

Floating point literals use a decimal point.

```dq
var f : float64 = 3.14
```

String literals normally use double quotes. Single quotes can also delimit text,
but a single-quoted literal containing exactly one character is a `char`.

```dq
var a : str = "hello"
var b : str = 'world'
var ch : char = 'x'
```

The `null` literal is used for null pointer, object-reference, and function
reference values.

```dq
type FIntFunc = function(a : int) -> int

var p : ^int = null
var cb : FIntFunc = null
```

## Declarations

Local and global variables are declared with `var`.

```dq
var count : int = 0
var name : str = "dq"
```

Constants are declared with `const`.

```dq
const MAX_COUNT : int = 100
```

Types are introduced with `type`, `struct`, `object`, and `enum`.

```dq
type TIndex = int

struct SPoint:
    x : int
    y : int
endstruct

enum NColor = (red, green, blue)
```

## Assignment

Assignment is a statement. The assignment operator `=` is not an expression and
cannot be used where a value is expected.

```dq
x = 1
x += 2
arr[i] = x
```

The compiler also supports compound assignments such as `+=`, `-=`, `*=`, `/=`,
and similar operators where the target type supports the operation.

## Conditions Require Bool

DQ does not convert numbers, pointers, or other values to `bool` implicitly.
This is intentionally different from C.

```c
if (number) {
    /* C accepts this */
}
```

In DQ, conditions must have type `bool`. Write the comparison explicitly.

```dq
if number <> 0:
    // ok
endif

if ptr != null:
    // ok
endif
```

The same rule applies to `while`, `for ... while`, logical operators, and
inline `iif`.

## Built-In Result Variable

Functions with a return type have a built-in `result` variable. Assigning to
`result` sets the function result. A `return expr` statement is also supported.

```dq
function Add(a : int, b : int) -> int:
    result = a + b
endfunc

function Add2(a : int, b : int) -> int:
    return a + b
endfunc
```

## Main Function

The program entry point is the special function `*Main`.

```dq
function *Main() -> int:
    return 0
endfunc
```

Star functions are reserved for special runtime and object functions. Common
star functions are `*Main`, `*Create`, and `*Destroy`.
