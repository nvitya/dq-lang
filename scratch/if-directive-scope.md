# DQ Scope Lookup in `#if` Directives

## Motivation

Conditional compilation should be able to depend directly on DQ constants in
the lexical scope containing the directive. Preprocessor definitions and DQ
symbols are separate namespaces and should be referenced explicitly when that
distinction matters.

For example:

```dq
const BOARD_REVISION : int = 3
#define TARGETVER = 5

#if BOARD_REVISION >= 2
    // selected using a DQ constant
#endif

#if @def.TARGETVER > 4
    // selected using a preprocessor definition
#endif
```

## Name lookup in conditional expressions

In `#if` and `#elif` expressions, an unqualified name is resolved as an
ordinary DQ name at the directive location. Lookup starts in the current
lexical scope and follows the normal parent-scope and visibility rules.

Consequently, this is valid:

```dq
const BOARD_REVISION : int = 3

#if BOARD_REVISION >= 2
    const HAS_NEW_BOARD : bool = true
#endif
```

Only declarations already visible when the directive is encountered can be
used. A declaration appearing later in the source is not available to an
earlier directive. Normal shadowing rules apply when multiple visible scopes
contain the same name.

Although ordinary name lookup is used, a conditional expression must remain a
compile-time constant Boolean expression. Resolving a variable, parameter,
function, or another nonconstant symbol therefore does not make it valid in a
condition; constant evaluation must reject it. An unknown unqualified name is
also an error unless it is used through `IntVal` as described below.

The same rules apply to the inline `#{if ...}` and `#{elif ...}` forms.

## Preprocessor definitions

Preprocessor definitions are accessed explicitly through the existing `@def`
namespace in `#if` and `#elif` expressions:

```dq
#define TARGETVER = 5

#if @def.TARGETVER > 4
    // ...
#endif
```

A bare `TARGETVER` in this example is looked up as a DQ symbol, not as a
preprocessor definition. If no visible DQ symbol has that name, the bare use is
an error.

The existence-testing directives continue to operate on preprocessor
definitions and do not change:

```dq
#ifdef TARGETVER
#ifndef TARGETVER
#elifdef TARGETVER
#elifndef TARGETVER
```

Likewise, expressions defining the value of another preprocessor definition
continue to resolve bare names in the preprocessor-definition scope. This
preserves constructs such as:

```dq
#define TARGETVER = 5
#define NEXT_TARGETVER = TARGETVER + 1
```

Thus, the scope change applies only to `#if` and `#elif` condition expressions,
not to `#define` value expressions or the existence-testing directives.

## Optional integer constants with `IntVal`

`IntVal` is a special expression available only in `#if` and `#elif`
conditions. It permits a condition to use an optional DQ integer constant and
provide a value when that constant is absent:

```dq
#if IntVal(BOARD_REVISION, 0) >= 2
    // ...
#endif
```

Its syntax is:

```dq
IntVal(identifier, default_integer_constant_expression)
```

The first argument is syntactically an identifier rather than a general
expression. `IntVal` looks it up using the same DQ lexical-scope rules as an
unqualified name in the surrounding conditional expression.

- If the identifier resolves to a DQ integer constant, `IntVal` returns its
  value.
- If the identifier does not resolve to any symbol, `IntVal` returns the
  default value.
- If the identifier resolves to a symbol that is not a constant, compilation
  fails. The default does not hide an accidental reference to a variable or
  another nonconstant declaration.
- If the identifier resolves to a constant whose value is not an integer,
  compilation fails.
- The default must be an integer constant expression.

The result has the integer type of the default expression. When the named
constant exists, its value is converted to that type using the normal
compile-time integer conversion rules. Giving the result a type determined by
the default keeps the expression's static type independent of whether the
optional declaration exists.

`IntVal` does not search the preprocessor-definition scope, and its first
argument cannot be an `@def` expression. Preprocessor definitions can instead
be tested with `#ifdef` or accessed as `@def.NAME` in an ordinary condition.

Outside a directive condition, `IntVal` has no special meaning. The special
form therefore does not reserve the name for ordinary DQ functions or calls.

## Examples

### Module constant

```dq
const API_LEVEL : int = 4

#if API_LEVEL >= 3
    function UseNewApi()
    endfunc
#endif
```

### Lexical shadowing

```dq
const LEVEL : int = 1

function Configure()
    const LEVEL : int = 3

    #if LEVEL >= 2
        // uses the local LEVEL
    #endif
endfunc
```

### Optional constant

```dq
#if IntVal(OPTIONAL_BUFFER_SIZE, 256) > 128
    // uses 256 when OPTIONAL_BUFFER_SIZE is not declared
#endif
```

### DQ constant and definition with the same name

```dq
const VERSION : int = 2
#define VERSION = 7

#if VERSION == 2
    // DQ constant
#endif

#if @def.VERSION == 7
    // preprocessor definition
#endif
```

## Diagnostics

The compiler should diagnose at least the following cases:

- an unknown bare name in a conditional expression;
- a resolved nonconstant symbol used in a constant condition;
- a final `#if` or `#elif` expression that is not Boolean;
- malformed `IntVal` syntax or an incorrect argument count;
- a nonconstant or noninteger symbol selected by `IntVal`;
- a noninteger or nonconstant `IntVal` default expression;
- an integer value that cannot be converted according to the existing constant
  conversion rules.

## Compatibility

This is a source-incompatible change for conditions that currently refer to
preprocessor definitions without qualification:

```dq
#define TARGETVER = 5
#if TARGETVER > 4       // old form
#endif
```

Such conditions must be migrated to:

```dq
#if @def.TARGETVER > 4
#endif
```

`#ifdef` and related existence tests require no migration. `#define`
expressions that refer to other definitions also require no migration.

## Implementation outline

The conditional-expression parser currently saves the active DQ scope and then
temporarily replaces it with the preprocessor-definition scope. For `#if` and
`#elif`, it should instead parse using the saved active DQ scope. Explicit
`@def.NAME` references already use namespace lookup and can continue through
that path.

`#define` value parsing should retain its current preprocessor-definition
scope.

`IntVal` should be recognized by the expression parser only while it is parsing
a directive condition. It should read its first argument as a raw identifier,
look up that identifier in the active DQ scope, validate the selected symbol and
value, and produce an ordinary typed integer constant expression. No run-time
code generation is required.

Tests should cover module and local constants, parent-scope lookup, shadowing,
forward references, variables and noninteger constants, missing names with and
without `IntVal`, explicit `@def` access, unchanged `#ifdef` behavior, inline
directives, and the separation between `#if` and `#define` lookup rules.
