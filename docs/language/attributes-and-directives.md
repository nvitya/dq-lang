# Attributes and Directives

DQ has declaration attributes written with `[[...]]` and source directives
written with `#`.

## Attributes

Attributes can appear before a declaration or after the declaration item,
depending on the declaration kind.

```dq
[[external]] function printf(fmt : ^char, ...) -> int

function printf(fmt : ^char, ...) -> int [[external('printf')]]
```

Multiple attributes can be written in one list.

```dq
function Run() [[virtual, abstract]]
```

## Common Attributes

| Attribute | Use |
| --- | --- |
| `[[external]]` | Link a function or global variable to an external symbol |
| `[[external('name')]]` | Link to a specific external symbol name |
| `[[overload]]` | Mark a function, method, or constructor as overloaded |
| `[[virtual]]` | Mark an object method as virtual |
| `[[override]]` | Mark an object method as overriding a base virtual method |
| `[[abstract]]` | Mark a virtual method as abstract |
| `[[final]]` | Mark a method or virtual slot as final |
| `[[packed]]` | Request packed aggregate layout |
| `[[align(n)]]` | Request alignment |
| `[[volatile]]` | Mark low-level volatile storage/access where supported |
| `[[cexport]]` | Export a symbol using C-compatible linkage where supported |
| `[[nowarn]]` | Suppress warnings for the declaration where supported |

Unsupported or inapplicable attributes may be ignored with a compiler warning.

## External Attribute

`[[external]]` without an argument uses the DQ declaration name as the external
symbol name.

```dq
[[external]] function puts(s : ^char) -> int
```

Use an argument when the external symbol name differs.

```dq
[[external('fprintf')]] function c_fprintf(stream : pointer, fmt : ^char, ...) -> int
```

External global variables are supported.

```dq
var libc_stdout : pointer [[external('stdout')]]
```

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

function zlibVersion() -> ^char  [[external]]
```

The exact linker behavior depends on the target platform and compiler driver.

## Directive Style

Directives start with `#` and are part of the DQ source feeder layer. They are
not general-purpose textual macros. Prefer DQ constants, functions, and modules
for normal program structure.
