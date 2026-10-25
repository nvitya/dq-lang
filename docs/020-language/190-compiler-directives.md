# Compiler Directives

Directives control source processing during compilation. They can define
compile-time values, select source branches, include text, and request link
libraries. They are not runtime statements and do not provide C-style textual
macro expansion.

```dq
use print

#define LOGGING
#define BUFFER_SIZE = 256

func *Main():
    #ifdef LOGGING
        PrintLn("buffer size = {}", [BUFFER_SIZE])
    #endif
endfunc
```

## Directive Forms

An ordinary directive starts with `#` and ends at the physical line ending.
The `#{...}` spelling closes at `}` and can be embedded within a source line:

```dq
#define FAST
var value : int = #{ifdef FAST} 1 #{else} 2 #{endif}
```

Directives accept `//` and `/* ... */` comments. A line comment does not continue
a directive onto the next line. Inactive conditional branches are skipped before
normal parsing, but their nested conditionals must remain balanced.

## Definitions and Namespaces

`#define NAME` defines Boolean `true`. `#define NAME = expression` defines a
supported constant Boolean or integer value. Command-line `-DNAME` and
`-DNAME=value`, project definitions, and target settings also supply definitions.
Bare names in definition values can use earlier preprocessor definitions.

Definitions live in `@def`, which also participates in ordinary unqualified
lookup. Use `@def.NAME` when a lexical declaration shadows the same name.
An existence test distinguishes whether the name is present, not whether its
value happens to be true or nonzero.

## Conditional Compilation

| Directive | Effect |
| --- | --- |
| `#if expression` | Start a branch selected by a constant Boolean expression. |
| `#ifdef name` / `#ifndef name` | Start a branch if a visible name exists / does not exist. |
| `#elif expression` | Test another condition if no earlier branch was selected. |
| `#elifdef name` / `#elifndef name` | Test another name-existence condition. |
| `#else` | Select the remaining alternative. |
| `#endif` | Close the conditional group. |

```dq
const API_LEVEL : int = 4

#if API_LEVEL >= 3
    const HAS_NEW_API : bool = true
#else
    const HAS_NEW_API : bool = false
#endif
```

`#if` and `#elif` use the lexical scope at the directive location; their values
must be compile-time `bool`. Only declarations already visible there participate.
Do not read a nonconstant variable in a condition that must be evaluated by the
compiler.

### Existence Tests and Defined

`#ifdef`, `#ifndef`, `#elifdef`, `#elifndef`, and `Defined(NAME)` search the visible
lexical scopes, including local variables, parameters, object members, types,
module declarations, merged imports, and preprocessor definitions. Qualified
names restrict lookup, for example `@def.FEATURE`, `@.MODULE_VALUE`, or
`@module.PUBLIC_VALUE`.

Existence tests do not read the symbol's value. An uninitialized local can be
tested for existence, although reading its value is still invalid. Inner or
completed sibling scopes do not contribute names. Preprocessor definitions
remain available to existence tests in methods even without a module `use`.

```dq
use print

func *Main():
    var local : int
    #ifdef local
        PrintLn("exists={}", [Defined(local)])
    #endif
endfunc
```

`Defined` is also a constant Boolean expression in ordinary code and can be
combined with `and`, `or`, and `not` in a `#if` expression.

## Optional Constants

`FirstInt`, `FirstFloat`, and `FirstBool` select the first available constant
from optional bare identifiers; the final argument is a required fallback:

```dq
const BUFFER_SIZE : int = FirstInt(BOARD_BUFFER_SIZE, DEFAULT_BUFFER_SIZE, 256)
const SCALE : float = FirstFloat(BOARD_SCALE, 1)
const LOGGING : bool = FirstBool(BOARD_LOGGING, false)
```

Lookup is left-to-right in the lexical scope. Missing identifiers are skipped.
The first existing identifier must name a constant convertible to the intrinsic's
result type; a variable or incompatible constant is an error rather than a
reason to try the next name. The fallback must be a valid convertible constant
even when it is not selected.

Result types are `int`, `float`, and `bool` respectively. These forms always
execute during compilation and work both in ordinary expressions and in
conditional directives. Optional candidate syntax accepts bare identifiers,
not qualified names such as `@def.NAME` or arbitrary candidate expressions.

## Target and Feature Definitions

| Definition | Meaning |
| --- | --- |
| `LINUX`, `WINDOWS`, `WASI`, `BARE` | Effective platform/runtime selection. |
| `X86_FAMILY`, `X86`, `X64` | x86 family and selected 32/64-bit architecture. |
| `ARM`, `ARM_M`, `ARM_A`, `ARMHF` | Applicable Arm profile/ABI markers. |
| `ARM64`, `AARCH64` | AArch64 target markers. |
| `RISCV`, `RV32`, `RV64` | RISC-V family and register width. |
| `WASM` | WebAssembly target. |
| `TARGET_32BIT`, `TARGET_64BIT` | Effective pointer width. |
| `PTRSIZE` | Pointer size in bytes. |
| `EXCEPTIONS` | DQ exception support is enabled. |
| `DYNSTRINGS` | Heap-backed dynamic strings are enabled. |

Test the relevant feature rather than assuming every hosted or bare target has
the same facilities. Definitions that describe disabled features are normally
absent, so existence testing is useful.

## Include

Source include directives are supported.

```dq
#include "file.dqi"
#include 'file.dqi'
```

Include files are processed by the source feeder before parsing the resulting
DQ source.

A module can keep its public declarations in a same-basename `.dqh` file:

```dq
#include header
```

For `file.dq`, this includes `file.dqh`. The directive is valid only before
`implementation` and cannot be used from a `.dqh` file.

Bare include paths first resolve relative to the current source file and then
through package search roots. `./` and `../` are source-file relative, while
`^/` is module-root relative.

Implementation includes that must invalidate the module object can be declared
before `implementation` without including their contents there:

```dq
#srcdep "somefunc_impl.dqi"
```

`#srcdep` uses the same path resolution as `#include`. It records only the named
file and does not parse it or discover further dependencies. Automatic include
dependencies are limited to directives written directly in the main `.dq` or its
same-basename `.dqh`; includes nested inside other `.dqi` files are still parsed
but are not tracked automatically.


### Include-Once Fragments

Put `#include_once` in an included fragment when its contents should be processed
only on the first inclusion. A later inclusion skips the remainder of that file.
This is a source-inclusion rule, separate from `use`, which already identifies
one canonical module.

## Source Root Option

```dq
#opt module_root_depth = 1
```

This sets how many parent levels above the main source directory establish its
module root. A value of zero uses the main source directory itself. The value
must be a nonnegative integer literal. Place the option before root-sensitive
imports or includes; `^/` paths resolve from that selected root.

## Link Libraries

`#linklib` requests linking with an external library.

```dq
#linklib('z')

func zlibVersion() -> ^char  [[external]]
```

The exact linker behavior depends on the target platform and compiler driver.


Only supported directive names have meaning; there is no general `#pragma`,
C macro substitution, or implied directive syntax from another language.
Build-option precedence and library search paths belong to
[Compiler Usage and Options](../030-compiler/040-compiler-usage-and-options.md)
and [Project Files](../030-compiler/050-project-files.md).
