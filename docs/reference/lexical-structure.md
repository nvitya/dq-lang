# Lexical Structure

This page defines how DQ source text is divided into tokens. For an introductory
view, see [Language Basics](../language/basics.md).

## Source Text and Case

DQ source files use the `.dq` extension. Identifiers and keywords are
case-sensitive. `value`, `Value`, and `VALUE` are different names.

An identifier starts with an ASCII letter or `_` and continues with ASCII
letters, decimal digits, or `_`. Unicode characters are allowed in text but not
in identifiers.

## Whitespace and Statement Boundaries

Spaces and tabs separate tokens. A newline normally terminates a statement. A
semicolon may terminate a statement explicitly and is required between multiple
statements on one physical line.

```dq
var x : int = 1
var y : int = 2; x += y
```

Newlines inside balanced parentheses, brackets, and similar expression contexts
do not necessarily terminate the containing construct. Block structure is
defined by statement syntax, not indentation; indentation is nevertheless the
canonical style.

## Comments

`//` starts a line comment. `/*` and `*/` delimit a block comment.

```dq
// one line

/* more than
   one line */
```

Comments act as whitespace and do not form part of string or character
literals.

## Keywords and Word Operators

Language keywords are lowercase. Contextual loop words such as `to`, `downto`,
`count`, `downcount`, and `step` are recognized in their corresponding `for`
forms.

Logical Boolean operators are the lowercase words `and`, `or`, and `not`.
Integer division, remainder, and XOR use the lowercase words `div`, `mod`, and
`xor`; other bitwise operators use `&`, `|`, `~`, `<<`, and `>>`. The words
`is` and `as` provide object testing and explicit casting.

## Numeric Literals

Decimal and hexadecimal integer literals are supported. Floating-point literals
contain a fractional part and/or exponent.

```dq
42
0xFF
3.14159
1.0e-6
```

A leading `-` is a unary operator rather than part of the literal token.

## Text and Character Literals

Double quotes produce string literals, including a one-character string.
Single quotes produce a `wchar` character literal when they contain one Unicode
scalar and otherwise produce text. Escape processing applies to both forms.

```dq
var text : str = "x"
var slash : wchar = '/'
var message : str = 'hello'
```

Quoted literals end on the same physical line. Use escape sequences such as
`\n`, `\r`, `\t`, `\\`, `\"`, and `\'` for control characters and embedded
delimiters. See [Strings and Characters](strings-and-characters.md) for typing
and conversion rules.

## Attributes and Directives

`[[...]]` encloses declaration attributes. A line whose first active token is
`#` is a compiler directive. These constructs are described in
[Attributes, Directives, and Interoperability](attributes-directives-and-interop.md).
