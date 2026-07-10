# C Interoperability

DQ is designed to call C libraries directly. Interop code normally combines
external declarations, C-compatible types, pointers, and `#linklib`.

## External Functions

Declare C functions with `[[external]]`.

```dq
function printf(fmt : ^cchar, ...) -> int  [[external]]
```

When the DQ name should differ from the C symbol, pass the symbol name.

```dq
function c_fprintf(stream : pointer, fmt : ^cchar, ...) -> int  [[external('fprintf')]]
```

## Variable Arguments

C-style variadic functions use `...`.

```dq
[[external]] function printf(fmt : ^cchar, ...) -> int

printf("value = %d\n", 42)
```

Use C-compatible argument types and format strings that match the external ABI.

## External Global Variables

External global variables can be imported with `[[external('name')]]`.

```dq
var libc_stdout : pointer [[external('stdout')]]
```

## C Strings

Use `^cchar`, `cstring`, and `cstring(n)` for C-style strings.

```dq
function WriteStr(s : cstring):
    var p : ^cchar = &s[0]
    while p^ <> 0:
        putchar(p^)
        p += 1
    endwhile
endfunc
```

`cstring(n)` declares fixed storage.

```dq
var buf : cstring(127) = "hello"
```

String literals can be passed to C string parameters when the type is
compatible.

## Generic Pointers

Use `pointer` for opaque C pointers.

```dq
[[external]] function fopen(path : ^cchar, mode : ^cchar) -> pointer
[[external]] function fclose(file : pointer) -> int
```

Cast to typed pointers before dereferencing.

## Struct Layout

Use structs for C-compatible records when layout is known.

```dq
struct SPoint:
    x : int32
    y : int32
endstruct
```

Attributes such as `[[packed]]`, `[[align(n)]]`, and `[[volatile]]` are available
for low-level layout/access cases where supported by the compiler.

## Linking Libraries

Use `#linklib` to request an external library.

```dq
#linklib('z')
[[external]] function zlibVersion() -> ^cchar
```

## Standard Package Declarations

The `stdpkg/libc` modules provide some existing C declarations.

```dq
use libc/stdio
```

## ABI Responsibility

DQ type checking verifies the DQ side of the declaration, but the programmer is
responsible for matching the real C ABI: integer widths, pointer types, struct
layout, calling convention, variadic arguments, ownership, and lifetime.
