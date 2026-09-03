# Attributes and Directives

For the normative rules, including preprocessing and source dependencies, see
[Attributes, Directives, and Interoperability](../reference/attributes-directives-and-interop.md).

DQ has declaration attributes written with `[[...]]` and source directives
written with `#`.

## Attributes

Attributes can appear before a declaration or after the declaration item,
depending on the declaration kind.

```dq
[[external]] func printf(fmt : ^char, ...) -> int

func printf(fmt : ^char, ...) -> int [[external('printf')]]
```

Multiple attributes can be written in one list.

```dq
func Run() [[virtual, abstract]]
```

## Common Attributes

| Attribute | Use |
| --- | --- |
| `[[external]]` | Link a function or global variable to an external symbol |
| `[[external('name')]]` | Link to a specific external symbol name |
| `[[external('name', 'module')]]` | Import a function from a specific WebAssembly module |
| `[[overload]]` | Mark a function, method, or constructor as overloaded |
| `[[virtual]]` | Mark an object method as virtual |
| `[[override]]` | Mark an object method as overriding a base virtual method |
| `[[abstract]]` | Mark a virtual method as abstract |
| `[[final]]` | Mark a method or virtual slot as final |
| `[[packed]]` | Request packed aggregate layout |
| `[[align(n)]]` | Request alignment |
| `[[volatile]]` | Mark low-level volatile storage/access where supported |
| `[[noread]]` | Forbid direct reads from low-level storage |
| `[[nowrite]]` | Forbid direct writes to low-level storage |
| `[[regrw]]`, `[[regro]]`, `[[regwo]]` | Declare volatile read/write, read-only, or write-only registers |
| `[[cexport]]` | Export a symbol using C-compatible linkage where supported |
| `[[used]]` | Retain a function or global storage even without visible references |
| `[[weak]]` | Emit a module-level function definition with weak linker binding |
| `[[nowarn]]` | Suppress warnings for the declaration where supported |

Unsupported or inapplicable attributes may be ignored with a compiler warning.

Use `[[used]]` for definitions referenced only by assembly, linker scripts,
hardware tables, or another source that LLVM cannot inspect. It prevents the
compiler and linker from discarding the definition during link-time optimization.

## Volatile and Register Access

The register attributes provide concise declarations for memory-mapped I/O:

```dq
struct SDeviceRegisters:
    STATUS  : [[regro]] uint32
    CONTROL : [[regrw]] uint32
    COMMAND : [[regwo]] uint32
endstruct
```

They expand to the fundamental access attributes:

| Register attribute | Equivalent attributes | Allowed operations |
| --- | --- | --- |
| `[[regrw]]` | `[[volatile]]` | read and write |
| `[[regro]]` | `[[volatile, nowrite]]` | read only |
| `[[regwo]]` | `[[volatile, noread]]` | write only |

Reading `[[noread]]` storage or writing `[[nowrite]]` storage is a compile error.
A modify-assignment such as `|=` reads and writes its target and therefore needs
both operations to be allowed. `noread` and `nowrite` cannot be combined on the
same declaration.

Volatile accesses are observable compiler operations, but they are not atomic
and do not imply a CPU or compiler memory barrier. Taking an address or binding
a reference currently discards `noread` and `nowrite`; this is an explicit
low-level escape. Whole-structure initialization and copying also do not yet
apply the access restrictions of individual fields.

## External Attribute

`[[external]]` without an argument uses the DQ declaration name as the external
symbol name.

```dq
[[external]] func puts(s : ^char) -> int
```

Use an argument when the external symbol name differs.

```dq
[[external('fprintf')]] func c_fprintf(stream : pointer, fmt : ^char, ...) -> int
```

On WebAssembly targets, an optional second argument selects the import module.
The first argument remains the imported function name.

```dq
func console_str(address : ^char, length : uint32, field_width : int32) [[external('console_str', 'wasmpascal_env')]]
```

This produces a WebAssembly import named
`wasmpascal_env.console_str`. Without the second argument, unresolved functions
retain the linker's default import module, normally `env`.

External global variables are supported.

```dq
var errno : int [[external]]
var libc_stdout : pointer [[external('stdout')]]
```

## Weak Attribute

`[[weak]]` gives a module-level function definition weak linker binding, allowing
a strong definition with the same linker name to replace it. It does not change
the function's linker name. Combine it with `[[cexport]]` when C or assembly code
must override the function by its unmangled DQ name.

```dq
func DefaultHandler() [[weak, cexport]]:
    // fallback implementation
endfunc
```

The attribute does not take arguments and is not valid on external declarations
or object and struct methods.

## Preprocessor Symbols

`#define` defines a preprocessor symbol.

```dq
#define DEBUG
#define BUFFER_SIZE = 4096
```

Preprocessor symbols can be read through `@def`.

```dq
var size : int = @def.BUFFER_SIZE
```

DQ does not provide C-style textual macro expansion.

## Conditional Compilation

Conditional directives include:

```dq
#if CONDITION
#ifdef SYMBOL
#ifndef SYMBOL
#elif CONDITION
#elifdef SYMBOL
#elifndef SYMBOL
#else
#endif
```

Example:

```dq
#ifdef DEBUG
    const LOGGING : bool = true
#else
    const LOGGING : bool = false
#endif
```

Directive blocks can also be written inline with `#{...}`.

```dq
var value : int = #{ifdef FAST} 1 #{else} 2 #{endif}
```

`#if` and `#elif` expressions resolve unqualified names in the surrounding DQ
lexical scope. The result must be a compile-time constant Boolean value, and
only declarations already visible at the directive can be used. Preprocessor
definitions are a separate namespace and are referenced explicitly:

```dq
const API_LEVEL : int = 4
#define TARGET_VERSION = 7

#if API_LEVEL >= 3
    const HAS_NEW_API : bool = true
#endif

#if @def.TARGET_VERSION >= 7
    const HAS_NEW_TARGET : bool = true
#endif
```

`#ifdef`, `#ifndef`, `#elifdef`, and `#elifndef` test preprocessor definitions
directly when given a bare name. They can also test a value symbol in any named
scope, for example `#ifdef @.FEATURE` or `#ifdef @module.FEATURE`. Bare names in
`#define` value expressions continue to refer to earlier preprocessor definitions.

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

## Link Libraries

`#linklib` requests linking with an external library.

```dq
#linklib('z')

func zlibVersion() -> ^char  [[external]]
```

The exact linker behavior depends on the target platform and compiler driver.

## Directive Style

Directives start with `#` and are part of the DQ source feeder layer. They are
not general-purpose textual macros. Prefer DQ constants, functions, and modules
for normal program structure.
