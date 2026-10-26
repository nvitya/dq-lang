# Basic Syntax

A DQ program is text stored in one or more `.dq` files. Each file is a module.
A function groups statements that perform a task; `*Main` is the entry function
of an ordinary application. For installation and the compile/run commands, see
[Getting Started](../010-project/030-getting-started.md).

```dq
use print

func *Main():
    // A variable stores a value under a name.
    var count : int = 3
    PrintLn("Count = {}", [count])
endfunc
```

`use print` imports printing functions. The colon starts a function body and
`endfunc` closes it. A newline normally finishes a statement.

## Source Files and Names

Source text can contain Unicode text in strings and comments. Names are
case-sensitive: `count`, `Count`, and `COUNT` are different identifiers.
Identifiers start with an ASCII letter or `_` and continue with ASCII letters,
digits, or `_`. Unicode identifiers are not supported.

Keywords are lowercase, for example `func`, `var`, `if`, and `endfunc`. Built-in
names such as `SizeOf`, `Ord`, `Round`, and `Object` keep their documented case.
Use descriptive names; the language does not require a particular naming prefix.

A `.dq` file is a source module. `.dqh` files conventionally hold public header
fragments and `.dqi` files hold other included fragments. Textual includes are
explained in [Compiler Directives](190-compiler-directives.md); module imports
are explained in [Modules and Packages](170-modules-and-packages.md).

## Statements, Whitespace, and Blocks

Spaces and tabs separate tokens. Indentation makes nesting clear, but does not
create or close blocks. A semicolon explicitly ends a statement and is needed
between multiple statements on the same physical line:

```dq
var x : int = 1
var y : int = 2; x += y
```

Expressions can continue across lines inside balanced parentheses or brackets.
Use the matching closer for each colon block: `endif`, `endwhile`, `endfor`,
`endfunc`, `endobject`, `endstruct`, `endunion`, or `endtry` as appropriate.
Supported constructs also allow braces:

```dq
func Twice(value : int) -> int {
    return value * 2
}
```

A block must consistently use its chosen closing style. Assembly function bodies
require the colon/`endfunc` style. This book normally uses named closers.

## Comments

`//` starts a comment through the end of the line. `/*` and `*/` enclose a block
comment. Comments act as whitespace; comment markers inside a string are text.

```dq
// A line comment.
var count : int = 3 /* a comment between tokens */
```

## Literals

A literal writes a value directly in source code:

| Kind | Examples |
| --- | --- |
| Whole number | `42`, `0xFF` |
| Floating point | `3.14`, `2e3` |
| Boolean | `true`, `false` |
| Empty reference/pointer | `nil` |
| Text | `"hello"`, `'hello'`, `"/"` |
| Character | `'/'`, `'€'` |
| Array | `[1, 2, 3]` |
| Default aggregate | `{}` |

A leading minus is an operator: `-42` negates the literal `42`. Numeric literals
receive the type required by their context; there is no need to add `.0` merely
to initialize a floating-point variable with a whole number.

Double quotes always denote text. Single quotes containing exactly one Unicode
scalar denote a `wchar`; single quotes containing zero or multiple scalars denote
text. Consequently, use `"/"` when comparing a string with one-character text.

Common escapes are `\n`, `\r`, `\t`, `\\`, `\"`, and `\'`. `\xNN` inserts a
byte using exactly two hexadecimal digits. Triple-quoted text spans lines:

```dq
var message : str = """
    first line
      indented line
    """
```

Triple double quotes or triple single quotes delimit multiline text. Delimiter-only
boundary lines are removed; the closing delimiter's indentation determines the
common indentation removed from the content. Escape sequences are still processed.
For character and string representations, see
[Character and String Types](070-strings-and-characters.md). Byte and Unicode
operations are described in [String Functions](075-string-functions.md).

## Declarations and Expressions

The usual declaration forms are `var name : Type = value`,
`const NAME : Type = value`, and `type Name = Type`. A colon introduces a type
annotation in a declaration. `=` assigns a value; `==` compares values.
Assignment is a statement without resulting a value. Assignment is not chainable so `a = b = 1` is invalid (compiler error).

An expression computes a value, such as `2 + 3` or `count > 0`. Conditions require
`bool`: write `count <> 0`, using a number (or anything else than `bool`) as a condition is not allowed.
The inequality operator is `<>`; `!=` is not accepted.

Read more about operators and expressions in the chapter [Expressions and Operators](040-expressions-and-operators.md)

## Attributes and Directives

`[[...]]` attaches attributes to declarations. `#` starts compiler directives,
including conditional compilation and source inclusion. They have different
roles from executable statements; see [Attributes](180-attributes.md) and
[Compiler Directives](190-compiler-directives.md).
