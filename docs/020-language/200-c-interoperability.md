# C Interoperability

DQ can call functions and access globals implemented outside DQ by declaring
their native types and symbol names. The compiler checks the DQ declaration;
the programmer must match the actual external ABI, ownership, and lifetime.

```dq
func c_puts(text : ^char) -> int32 [[external('puts')]] ..

func *Main():
    c_puts("hello from C")
endfunc
```

This calls the C library's `puts` function. Ordinary hosted programs already
link their C runtime; other libraries may need an explicit link request.

## External Functions and Symbols

`[[external]]` uses the DQ declaration name as the native symbol name.
`[[external('name')]]` supplies a different name. External declarations have no
DQ body. The declaration controls how arguments and the result are generated,
so a mistaken signature can corrupt state even when compilation succeeds.

On WebAssembly, `external('name', 'module')` also selects the function's import
module. The optional module argument applies only to functions on that target;
it is not external-global metadata.

## Match C Types Explicitly

| C-side concept | Typical DQ declaration |
| --- | --- |
| `int`, `unsigned int` on usual hosted ABIs | `int32`, `uint32` |
| Fixed-width `int8_t` ... `uint64_t` | Matching DQ fixed-width integer. |
| `size_t` | `uint` on supported pointer-sized hosted ABIs. |
| `float`, `double` | `float32`, `float64` |
| `void *`, opaque handle | `pointer` |
| Pointer to a known type | `^T` |
| Zero-terminated byte string | `^char` or a compatible `rostr` argument. |
| C record/union | An explicitly matching `struct`/`union` layout. |

DQ `int` is pointer-sized; it is not a portable spelling for C `int` or C `long`.
In particular, Windows x64 and Linux x64 differ in the width of C `long`.
Check the target's declaration instead of copying a type name by resemblance.
A DQ object reference, dynamic array descriptor, `str`, or unsized `embstr`
descriptor is not automatically the C library's corresponding representation.

## Variadic Functions and Callbacks

C-compatible variadic external functions declare `...`:

```dq
func printf(format : ^char, ...) -> int32 [[external]] ..

func *Main():
    var value : int32 = 42
    printf("value = %d\n", value)
endfunc
```

The unnamed arguments follow target C ABI promotions. DQ cannot verify the types
expected by a C format string; use matching types and format specifiers. This is
different from DQ formatting, whose arguments are boxed as `[]anyvalue`.

Use a plain compatible function reference for a C callback. A bound
`of object` method reference contains an extra receiver and is not a plain C
function pointer. Retained callbacks and their context must remain alive for as
long as the external library may call them.

## External Global Variables

A global may also bind to a native name:

```dq
var native_counter : int32 [[external('native_counter')]]
```

This fragment requires the native program/library to supply that symbol. C
headers sometimes expose apparent globals through macros or functions; declare
the real exported interface. For example, `errno` is platform-specific and must
not be assumed to be a directly exported integer variable.

## Strings, Buffers, and Borrowing

A C string pointer must refer to suitably terminated live storage. Text literals,
`rostr`, `str.pchar`, and bounded `embstr(N).pchar` provide appropriate terminated
storage according to their lifetime rules. A `strslice` may be unterminated;
copy it into a `str` or bounded buffer when a C string is required.

`embstr(N)` has exactly `N` storage bytes, including its terminator, and a maximum
content length of `N - 1`. `.pchar` is a borrowed pointer. A C function that
writes into it must respect its capacity and any runtime descriptor invariants.
An owning dynamic string must not be mutated through a raw C pointer in a way
that bypasses its copy-on-write or length management.

C functions using termination see only the prefix before an embedded zero byte.
Use a pointer-plus-length API for binary data. Raw pointers and borrowed text do
not extend their owners' lifetimes when retained by C code.

## Structures, Unions, and Layout

Fields, padding, packing, alignment, and the calling convention must match the
native declaration. Use fixed-width fields and `SizeOf`/`OffsetOf` to check the
layout. A packed DQ record is appropriate only when the C record is packed the
same way; packing a normally aligned record changes its ABI.

Unions provide untagged overlapping storage for native unions. The same
representation-validity obligations apply as in DQ code. See
[Structures and Unions](100-structures-and-unions.md) and [Attributes](180-attributes.md).

## Linking and Native Ownership

`#linklib('name')` requests an external library. Compiler options and project files
can also supply native libraries and objects:

```dq
#linklib('z')
func zlibVersion() -> ^char [[external]] ..
```

The example requires a usable zlib library in the linker's search path. Existing
C declarations are also available through modules such as `use libc/stdio`.

Release a native resource with its API's matching operation: for example, close
a file with that library's close function, not `delete`. A borrowed pointer is
not evidence that DQ owns its allocation. Avoid propagating a DQ exception
through an external boundary unless that ABI explicitly supports it.

[Linux setup](../030-compiler/020-linux-development-environment.md#add-external-libraries)
and [Windows setup](../030-compiler/030-windows-development-environment.md#add-external-libraries-with-msys2)
explain native library installation and search paths.
