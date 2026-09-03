# Attributes, Directives, and Interoperability

This page defines declaration metadata, preprocessing, compiler directives, and
C ABI access. See the [Attributes and Directives](../language/attributes-and-directives.md)
and [C Interoperability](../language/interop.md) guides for common examples.

## Attributes

Attributes use `[[...]]` and attach to the declaration they precede or follow.
Multiple attributes may share one list.

```dq
[[weak]] func Hook()
func Run() [[inline]]:
endfunc
```

Unknown, unsupported, or inapplicable attributes are diagnosed as warnings or
errors depending on whether compilation can continue safely. They never acquire
an undocumented effect. Important implemented attributes include:

| Attribute | Purpose |
| --- | --- |
| `external` | bind a declaration to an external symbol |
| `overload` | place functions of the same name in an overload set |
| `inline`, `always_inline`, `noinline` | control code-generation inlining |
| `weak` | emit a replaceable weak native definition |
| `used` | retain a definition whose references are invisible to the optimizer |
| `virtual`, `override`, `abstract`, `final` | define object dispatch behavior |
| `asm` | make a function body target assembly |
| `packed` or layout attributes | request supported ABI/layout behavior |
| `volatile`, `noread`, `nowrite` | control direct low-level storage access |
| `regrw`, `regro`, `regwo` | shorthand for volatile MMIO register access modes |

An attribute cannot override an incompatible language contract. For example, an
external function has no DQ body, and mutually exclusive inlining choices cannot
be combined.

`used` applies to functions and global storage. It preserves the emitted
definition through compilation and linking, including LTO. Use it when a symbol
is reached only through assembly, a linker script, a hardware table, or another
mechanism that is not represented by ordinary DQ references.

`regrw` means `volatile`, `regro` means `volatile, nowrite`, and `regwo`
means `volatile, noread`. Reads from `noread` declarations and writes to
`nowrite` declarations are errors; modify-assignment requires both access modes.
Combining `noread` and `nowrite` is invalid. These restrictions apply to direct
global, field, and fixed-array-element access and are retained through module
interfaces.

Volatile access does not provide atomicity or a memory barrier. Address-taking
and reference binding currently discard `noread` and `nowrite`, and aggregate
copies are not restricted by member attributes.

## Preprocessor Defines

The compiler maintains compile-time definitions in the `@def` namespace.
Command-line `-Dname` and `-Dname=value`, project definitions, and target/runtime
definitions contribute values.

Target definitions include `LINUX`, `WINDOWS`, `WASI`, or `BARE` for the
platform; `ARM`, `WASM`, or `RISCV` for explicit cross-target architectures;
and `TARGET_32BIT`/`TARGET_64BIT`, `PTRSIZE`, `EXCEPTIONS`, and `DYNSTRINGS` for
the effective representation and feature settings.

Conditional directives include or discard source before parsing inactive code:

```dq
#ifdef FEATURE
    RunFeature()
#else
    RunFallback()
#endif
```

Nested conditionals must be balanced. A definition has the Boolean or integer
value supplied by its source; code should not assume that every defined name has
only the value `true`.

Expressions in `#if` and `#elif` use ordinary DQ lexical name lookup at the
directive location and must evaluate to a constant Boolean value. Access a
preprocessor definition in such an expression through `@def.NAME`. The
existence-testing directives (`#ifdef`, `#ifndef`, `#elifdef`, and `#elifndef`)
take either an unqualified preprocessor-definition name or a namespace-qualified
DQ value symbol such as `@.FEATURE` or `@module.FEATURE`.

`Defined(NAME)` provides the same existence test as a constant Boolean
expression, allowing tests to be combined with `and`, `or`, and `not` in `#if`
or ordinary code. A bare name searches `@def`; namespace-qualified forms such
as `Defined(@.FEATURE)` search the specified DQ scope.

## Include and Source Dependencies

`#include` inserts a DQ header/source fragment at the directive location under
the compiler's include rules. `#srcdep` records an additional source dependency
used for interface freshness. Include cycles and missing inputs are errors.

Modules should normally use `use`; textual inclusion is intended for declarations
or platform integration that cannot be represented as a module import.

## Options and Link Directives

Compiler directives can set supported source options and request native link
libraries. Options affect the documented lexical scope/module according to the
specific directive and must be known before code whose parsing or generation
depends on them.

The command-line and project-file references remain authoritative for build
precedence. A source directive cannot silently override an incompatible target
or ABI selected for the compilation.

## External Functions and Variables

`[[external]]` declares a symbol implemented outside DQ:

```dq
func puts(text : ^char) -> int [[external]]
```

External globals are declared with the corresponding external attribute form.
The declaration's DQ type determines the generated ABI access. DQ does not check
that the linked symbol actually has that native type.

Without an explicit argument, `[[external]]` uses the DQ declaration name as
the native symbol name for both functions and global variables.

`[[external('name', 'module')]]` gives an imported WebAssembly function both
its import name and import module. The module argument is optional and affects
WebAssembly targets only.

```dq
func console_str(address : ^char, length : uint32, field_width : int32) [[external('console_str', 'wasmpascal_env')]]
```

The two-argument form applies to functions; WebAssembly import-module metadata
is not supported for external global variables.

## C Strings and Pointers

Use `^char` for a raw zero-terminated C pointer, `cstring` for a borrowed bounded
descriptor, and `cstring(N)` for owned fixed storage. `.pchar` obtains a borrowed
raw pointer from compatible text storage.

`pointer` corresponds to an untyped native address. Explicit casts are required
before typed access. The programmer owns the validity, alignment, lifetime, and
const-correctness obligations that cannot be expressed at the external ABI.

## Structures and Calling Convention

External structures must use field types, layout attributes, packing, and target
ABI rules matching the C declaration. DQ source field names do not affect the
native layout.

External variadic calls use the target C ABI and its default promotions. The
compiler cannot validate the types expected by the format string or external
callee.

## Linking

Libraries and objects can be supplied through source directives, compiler
options, or `.dqproj` properties. Resolution and ordering are build-system
behavior described under [Compiler and Tools](../compiler/using-dq-comp.md).
