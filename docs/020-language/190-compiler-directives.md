# Compiler Directives

A **compiler directive** controls how source code is compiled. Use directives
to enable optional logging, choose code for a target platform, include shared
source, or link an external library. Directives are processed during
compilation; they do not run each time the program runs.

For example, include a diagnostic message only when `LOGGING` is defined:

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

This prints `buffer size = 256`. Remove `#define LOGGING` and compile again to
leave out the print statement entirely. By comparison, an ordinary `if` chooses
what to execute while the program is running.

## Definitions and Namespaces

`#define NAME` defines a name with the value `true`. Add `= expression` to
supply a constant value, or add `: Type = expression` to specify its type:

```dq
#define LOGGING
#define BUFFER_SIZE = 256
#define DOUBLE_BUFFER_SIZE = BUFFER_SIZE * 2
#define BOARD_NAME : rostr = "PowerLogger-Lite"
#define MCU_RK3506 : bool = false
#define EXTERNAL_XTAL_HZ : uint = 24000000
```

Typed definitions follow the same type, conversion, and constant-initializer
rules as `const`; the resulting name belongs to `@def`. Types and initializer
names resolve in the surrounding scope. Without a type annotation, the type
is taken from the constant expression. The bare flag form is equivalent to
`#define NAME : bool = true`.

You can use earlier definitions in later ones. Definitions can also come from
compiler options such as `-DLOGGING` or `-DBUFFER_SIZE=512`, project files, and
target settings.

Definitions belong to the `@def` namespace and are normally available by their
short names. Use `@def.BUFFER_SIZE` if a local declaration has the same name.
DQ definitions provide constant values; they do not perform C-style textual
macro substitution. For ordinary program constants, use `const`. Definitions
are useful when the build configuration and conditional compilation need to
refer to the same setting.

## Conditional Compilation

`#if` tests a constant boolean expression. `#ifdef` tests whether a name exists.
Use `#else` for an alternative and `#endif` to close the group:

```dq
use print

func *Main():
    #ifdef WINDOWS
        PrintLn("Running on Windows")
    #elifdef LINUX
        PrintLn("Running on Linux")
    #else
        PrintLn("Running on another platform")
    #endif
endfunc
```

Only the selected branch becomes part of the program. Other branches are
skipped before normal DQ parsing, although their nested conditional directives
must still balance. This lets a source file contain declarations or assembly
instructions that are valid only for the selected platform.

| Directive | Effect |
| --- | --- |
| `#if expression` | Start a branch selected by a constant Boolean expression. |
| `#ifdef name` / `#ifndef name` | Start a branch if a visible name exists / does not exist. |
| `#elif expression` | Test another condition if no earlier branch was selected. |
| `#elifdef name` / `#elifndef name` | Test another name-existence condition. |
| `#else` | Select the remaining alternative. |
| `#endif` | Close the conditional group. |

For a value comparison, use `#if` rather than `#ifdef`:

```dq
#define API_LEVEL = 4

#if API_LEVEL >= 3
    const HAS_NEW_API : bool = true
#else
    const HAS_NEW_API : bool = false
#endif
```

The condition must be a compile-time `bool` and can use only names already
visible at that point. A variable whose value is known only at runtime cannot
supply the condition.

## Target and Feature Definitions

The compiler supplies names for the selected target and enabled features:

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

Check the feature you need rather than assuming every target has it. Disabled
features are normally absent, so use `#ifdef EXCEPTIONS`, for example, to test
for exception support.

## Include

`#include` inserts another file's source at that point:

```dq
#include "./shared.dqi"
```

Single quotes are also accepted. Use an include for a source fragment; use
[`use`](170-modules-and-packages.md) when you want a module with its own public
interface and namespace.

Local include paths must start with `./` or `../`; bare filenames are rejected.
A path such as `platform/config.dqi` selects the highest-priority `platform`
package directory first, then looks for `config.dqi` inside it. A missing file
is an error; lookup does not fall back to another version of the package or
search the current directory. Include paths retain their filename extensions.
`./` and `../` are relative to the source file; `^/`
is relative to the configured package containing the current source file, just
like `use ^/`. Inside an included fragment from another package, this is the
fragment's package root; returning to the including file restores its context.
A `^/` path requires the current source file to belong to a configured package.

Package search paths also configure include discovery: `--pkg_path` and the
project-file `pkg_path` setting, as well as direct `--pkg` / `pkg` mappings,
apply to both `use` and package-qualified
`#include` paths. No separate include-directory list is needed. A platform
package can ship configuration fragments, register modules, and drivers together;
the project only needs to make that package discoverable. For example, with
`pkg_path = './packages'`, `#include "platform/config.dqi"` selects the file
`packages/platform/config.dqi` relative to the project directory. Within that
fragment, `./` and `../` resolve from the fragment's directory, while `^/`
resolves from the selected `platform` package root.

### Include-Once Fragments

Put `#include_once` at the start of an included fragment when it should be
processed only once. Later inclusions skip the rest of that file. This is
separate from `use`, which already imports a module as one shared instance.
For example, save this fragment as `shared.dqi` beside your main file:

```dq
#include_once
const MAX_ITEMS : int = 16
```

The main file can include it more than once, directly or through other
fragments, without declaring `MAX_ITEMS` twice. Included declarations appear
in the scope where the include occurs; the fragment has no separate module
namespace.

### Separate Header Files

A module can keep its public declarations in a `.dqh` file with the same base
name. For `file.dq`, this directive includes `file.dqh`:

```dq
#include header
```

Use it before `implementation`. It cannot be used from inside a `.dqh` file.

## Link Libraries

`#linklib` asks the linker to use an external library. For example, the
following declaration gets zlib's version string:

```dq
#linklib('z')

func zlibVersion() -> ^char [[external]] ..
```

The library must be installed and available to the target's linker. See
[C Interoperability](200-c-interoperability.md#linking-and-native-ownership)
for the full example and setup links.

## Existence Tests and Defined

Existence tests find visible names, including local variables, parameters,
object members, types, module declarations, merged imports, and definitions.
`Defined(NAME)` provides the same test as a constant boolean expression:

```dq
#define LOGGING = false

#if Defined(LOGGING) and @def.LOGGING
    // included only if LOGGING exists and its value is true
#endif
```

A name can exist even when its value is `false` or zero. `#ifdef LOGGING`
therefore succeeds for the definition above; it does not read that value.

Qualified names restrict lookup: `@def.FEATURE` checks definitions,
`@.MODULE_VALUE` checks the current module, and `@module.PUBLIC_VALUE` checks
that namespace. Names from inner or completed sibling scopes are not visible.
Preprocessor definitions remain available to existence tests in methods even
without a method-local `use`.

You may test whether an uninitialized local exists without reading it. This
does not permit you to read the variable before assigning a value.

## Optional Constants

`FirstInt`, `FirstFloat`, and `FirstBool` select the first available constant
from optional names, with a required fallback at the end. This is useful for
settings that a board or build configuration may supply:

```dq
const BUFFER_SIZE : int = FirstInt(BOARD_BUFFER_SIZE, DEFAULT_BUFFER_SIZE, 256)
const SCALE : float = FirstFloat(BOARD_SCALE, 1)
const LOGGING : bool = FirstBool(BOARD_LOGGING, false)
```

Missing names are skipped from left to right. The first name that exists must
be a constant convertible to the required type. A variable or an incompatible
constant is an error, rather than a reason to try the next name. The fallback
must be valid even when it is not selected.

The results are `int`, `float`, and `bool`, respectively. These expressions
always run during compilation and can be used in ordinary expressions or
conditional directives. Optional candidates must be bare names, such as
`BOARD_SCALE`, rather than qualified names or arbitrary expressions.

## Directive Forms

An ordinary directive starts with `#` and ends at the physical line ending.
For a directive embedded inside a source line, use `#{...}`:

```dq
#define FAST
var value : int = #{ifdef FAST} 1 #{else} 2 #{endif}
```

Directives accept `//` and `/* ... */` comments. A line comment does not extend
a directive onto the next line. Only supported directive names have meaning;
there is no general `#pragma` or implied syntax from other languages.

## Include Dependencies

If an implementation include affects the compiled module, declare it before
`implementation` with `#srcdep`:

```dq
#srcdep "./somefunc_impl.dqi"

func Calculate(value : int) -> int ..

implementation

#include "./somefunc_impl.dqi"
```

Here `somefunc_impl.dqi` contains the body of `Calculate`. `#include` inserts
that body where it belongs, while the earlier `#srcdep` tells the build system
that a change to the fragment can require recompiling the module.

`#srcdep` uses the same explicit local and package path rules as `#include`.
It records the dependency without inserting its contents there. It
uses the same path lookup as `#include` and records only the named file; it does
not parse that file or discover its dependencies.

Automatic include tracking covers directives directly in the main `.dq` file
or its same-basename `.dqh`. Nested includes in `.dqi` files are still parsed,
but their dependencies are not tracked automatically. Add explicit dependencies
when needed for reliable rebuilding.

For build-option precedence and library search paths, see
[Compiler Usage and Options](../030-compiler/040-compiler-usage-and-options.md)
and [Project Files](../030-compiler/050-project-files.md).
