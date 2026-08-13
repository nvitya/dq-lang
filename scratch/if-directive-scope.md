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
also an error unless it is used as an optional candidate of one of the
`FirstInt`, `FirstFloat`, or `FirstBool` intrinsics described below.

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

## Typed constant fallbacks with `FirstInt`, `FirstFloat`, and `FirstBool`

`FirstInt`, `FirstFloat`, and `FirstBool` are globally available compile-time
intrinsics. They select the first available DQ constant from a left-to-right
list of optional names, or use a required final fallback when none of those
names is declared. They can be used in `#if` and `#elif` conditions and in any
other expression context:

```dq
#if FirstInt(BOARD_REVISION, DEFAULT_BOARD_REVISION, 0) >= 2
    // ...
#endif

#if FirstFloat(SCALE_FACTOR, DEFAULT_SCALE_FACTOR, 1.0) > 0.5
    // ...
#endif

#if FirstBool(ENABLE_CACHE, DEFAULT_ENABLE_CACHE, false)
    // ...
#endif
```

Their syntax is:

```dq
FirstInt(identifier [, identifier ...], fallback_constant_expression) -> int
FirstFloat(identifier [, identifier ...], fallback_constant_expression) -> float
FirstBool(identifier [, identifier ...], fallback_constant_expression) -> bool
```

Each form requires at least one optional identifier and a final fallback
expression. The optional arguments are syntactically identifiers rather than
general expressions. They are looked up from left to right using the same DQ
lexical-scope rules as an unqualified name at the intrinsic call location.

- If an identifier does not resolve to any symbol, lookup continues with the
  next optional identifier.
- Lookup stops at the first identifier that resolves to a symbol. That symbol
  must be a constant whose value can be converted to the intrinsic's fixed
  result type using the normal compile-time implicit conversion rules.
- If the first resolved symbol is not a constant, compilation fails. A later
  candidate or the fallback does not hide an accidental reference to a
  variable, parameter, function, or another nonconstant declaration.
- If the first resolved symbol is a constant that cannot be converted to the
  fixed result type, compilation fails rather than silently skipping it.
- If none of the optional identifiers resolves, the final fallback expression
  is used.
- The fallback must be a compile-time constant expression convertible to the
  fixed result type. It is validated even when an optional constant is
  selected.

The result types are always the canonical built-in types: `int` for
`FirstInt`, `float` for `FirstFloat`, and `bool` for `FirstBool`. The selected
constant or fallback value is converted to that fixed type at compile time.
The complete intrinsic is replaced with an ordinary typed constant expression;
it does not generate a run-time call.

The optional identifiers do not search the preprocessor-definition scope and
cannot be `@def` expressions. Preprocessor definitions can instead be tested
with `#ifdef` in directive code or accessed as `@def.NAME` in an ordinary
expression. The fallback is an ordinary constant expression and may use
explicitly qualified names.

Because the intrinsics are recognized in every expression context,
`FirstInt`, `FirstFloat`, and `FirstBool` are reserved intrinsic names rather
than ordinary function names. A string counterpart such as `FirstStr` may be
added separately in the future but is not part of this specification.

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

### Optional constants

```dq
#if FirstInt(BOARD_BUFFER_SIZE, OPTIONAL_BUFFER_SIZE, 256) > 128
    // uses the first declared integer constant, or 256 if neither is declared
#endif

#if FirstFloat(OPTIONAL_SCALE, 1.0) >= 0.5
    // uses 1.0 when OPTIONAL_SCALE is not declared
#endif

#if FirstBool(OPTIONAL_LOGGING, false)
    // selected only when OPTIONAL_LOGGING exists and is true
#endif
```

### Use outside conditional directives

The intrinsic is always evaluated during compilation, even when its containing
expression is not itself a directive condition:

```dq
const BUFFER_SIZE : int = FirstInt(BOARD_BUFFER_SIZE, DEFAULT_BUFFER_SIZE, 256)
const SCALE : float = FirstFloat(BOARD_SCALE, 1.0)
const LOGGING : bool = FirstBool(BOARD_LOGGING, false)

var buffer : [FirstInt(BUFFER_SIZE_OVERRIDE, BUFFER_SIZE)]byte
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
- malformed `FirstInt`, `FirstFloat`, or `FirstBool` syntax, including an
  incorrect argument count or a non-identifier optional argument;
- a nonconstant symbol selected by one of the `First...` forms;
- a selected constant or fallback expression that cannot be converted to the
  intrinsic's fixed result type;
- a nonconstant fallback expression;
- a compile-time conversion whose value is not valid under the existing
  constant conversion rules.

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

The globally recognized `FirstInt`, `FirstFloat`, and `FirstBool` names are
also reserved by this change. Existing ordinary functions with those names
must be renamed.

## Implementation outline

The conditional-expression parser currently saves the active DQ scope and then
temporarily replaces it with the preprocessor-definition scope. For `#if` and
`#elif`, it should instead parse using the saved active DQ scope. Explicit
`@def.NAME` references already use namespace lookup and can continue through
that path.

`#define` value parsing should retain its current preprocessor-definition
scope.

`FirstInt`, `FirstFloat`, and `FirstBool` should be recognized by the expression
parser in every expression context. The parser should read all arguments except
the final fallback as raw identifiers, look them up from left to right in the
active DQ scope, validate and convert the first resolved constant, and produce
an ordinary constant expression of the intrinsic's fixed built-in result type.
The final fallback must always be parsed, constant-evaluated, and checked for
conversion to that result type. No run-time code generation is required.

Tests should cover module and local constants, parent-scope lookup, shadowing,
forward references, variables, nonconvertible constants, multiple missing and
available candidates, fixed result types, fallback conversion, missing names
with and without the `First...` forms, explicit `@def` access, unchanged
`#ifdef` behavior, inline directives, use in constant declarations and other
ordinary expressions, reserved-name handling, and the separation between
`#if` and `#define` lookup rules.
