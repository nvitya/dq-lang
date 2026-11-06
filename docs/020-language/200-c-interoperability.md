# C Interoperability

DQ can call functions from C libraries and let C code call DQ functions. This
is useful for reusing existing libraries or working with an operating system's
native APIs. There are two parts to calling a C function: **declare** its
interface so DQ can compile the call, and **link** its implementation so the
finished program can run it. Declare the function's name, parameter types,
and result in DQ, then mark it `[[external]]`.

For example, call the C library's `puts` function to print a line:

```dq
func c_puts(text : ^char) -> int32 [[external('puts')]] ..

func *Main():
    c_puts("hello from C")
endfunc
```

This prints `hello from C`. `external('puts')` connects the DQ name `c_puts`
to the native function named `puts`. Normal hosted programs already link the
C runtime; other libraries may need an explicit link request.

## External Functions and Symbols

If the DQ and native names are the same, use plain `[[external]]`:

```dq
func puts(text : ^char) -> int32 [[external]] ..
```

The trailing `..` ends the declaration without a DQ body. The implementation
comes from the external library.

Match the library's actual declaration, usually found in its C header. The
compiler checks your DQ code against the declaration you wrote; it cannot
check that declaration against the library. A wrong signature may compile
successfully and still fail at runtime.

The **ABI**, or application binary interface, includes details such as type
sizes, memory layout, and how function arguments are passed. These must agree
between DQ and the native code.

## Match C Types Explicitly

Use fixed-width DQ types where the C declaration has a known width:

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

DQ `int` is pointer-sized, so use `int32` for the usual C `int`. C `long`
also varies by platform: it has different widths on Windows x64 and Linux x64.
Check the target's header rather than assuming similar type names match.

DQ objects, dynamic arrays, `str`, and unsized `embstr` are not automatically
represented like a C library's objects, arrays, or strings. Pass the raw data
in the form the library expects.

## Strings, Buffers, and Borrowing

C string functions usually expect a pointer to bytes ending in a zero byte.
A text literal or `rostr` can be passed directly to a `^char` parameter:

```dq
use print

func strlen(text : ^char) -> uint [[external]] ..

func *Main():
    var text : rostr = "hello"
    PrintLn("{} bytes", [strlen(text)])
endfunc
```

This prints `5 bytes`. DQ passes the underlying text address without copying
it. Although the C parameter is written `^char`, do not let C write through
it when the source is read-only.

For a dynamic `str` or fixed-storage `embstr(N)`, use `.pchar` to obtain a borrowed
pointer to terminated text. A `strslice` may not end in a zero byte; copy it
into a `str` or fixed-storage buffer before passing it to a C string function:

```dq
var message : str = "hello world"
var first_word : str = message[:5]
c_puts(first_word.pchar) // prints hello, using c_puts from the opening example
```

Keep the text or buffer alive while C uses its address. If the library stores
the pointer for later, the source must remain valid for that entire period.

A C function that writes into a buffer must know its usable size. Do not let it
write beyond the capacity or bypass DQ's string length and sharing rules. See
[Buffer and String Details](#buffer-and-string-details) before passing writable
string storage.

## Variadic Functions

A C function with `...` accepts extra arguments. For example, `printf` uses its
format string to interpret those arguments:

```dq
func printf(format : ^char, ...) -> int32 [[external]] ..

func *Main():
    var value : int32 = 42
    printf("value = %d\n", value)
endfunc
```

This prints `value = 42`. Here `%d` expects a C `int`, so the argument is
`int32`. DQ cannot verify the types expected by a C format string. Extra
arguments follow the target's C argument-promotion rules. DQ's own formatting
instead uses a checked list of [`anyvalue`](150-anyvalue.md) values.

## Callbacks

A **callback** is a function you give to a library for it to call. Use a plain
function reference with the exact signature the C API expects. A bound
`of object` method reference also stores its receiver, so it is not a plain C
function pointer. Keep any callback context alive for as long as the library
may use it.

For example, the C library's `qsort` function sorts a buffer by repeatedly
calling a comparison function supplied by the caller:

```dq
use print

type FCompare = func(left : pointer, right : pointer) -> int32

func qsort(base : pointer, count : uint, size : uint,
    compare : FCompare) [[external]] ..

func CompareInts(left : pointer, right : pointer) -> int32:
    var a : int32 = ^int32(left)^
    var b : int32 = ^int32(right)^
    if a < b:
        return -1
    elif a > b:
        return 1
    endif
    return 0
endfunc

func *Main():
    var values : [4]int32 = [30, 10, 40, 20]
    qsort(%values[0], values.length, SizeOf(int32), CompareInts)
    for value : int32 in values:
        PrintLn("{}", [value])
    endfor
endfunc
```

This prints `10`, `20`, `30`, and `40`, each on its own line. `qsort` receives
the address of the first element, the number of elements, and the size of each
one in bytes. It passes two element addresses to `CompareInts`, which reads
them as `int32` values and returns a negative, zero, or positive C `int`.
Explicit comparisons avoid the overflow that subtracting the values could cause.

`qsort` rearranges the caller's array during the call and does not retain its
address afterward. The local array therefore lives long enough. This example
uses plain fixed-width numbers; do not use raw C byte-copying operations to
rearrange DQ values that require managed copying or cleanup.

## Linking and Native Ownership

Use `#linklib('name')` when an external library needs to be linked explicitly.
For example, this program prints the installed zlib version:

```dq
use print
#linklib('z')

func zlibVersion() -> ^char [[external]] ..

func *Main():
    var version : rostr = zlibVersion()
    PrintLn("zlib {}", [version])
endfunc
```

This requires zlib to be installed in the target linker's search path. Compiler
options and project files can also provide native libraries and object files.
Existing C declarations are available through modules such as `use libc/stdio`.

Release resources with the operation their API specifies. For example, close a
native file using the library's close function. A returned pointer may be
borrowed, or may need a library-specific release function; do not assume that
`delete` or an `autofree` pointer is appropriate.

[Linux setup](../030-compiler/020-linux-development-environment.md#add-external-libraries)
and [Windows setup](../030-compiler/030-windows-development-environment.md#add-external-libraries-with-msys2)
explain native library installation and search paths.

## Exporting DQ Functions

Use `[[cexport]]` to expose a function under its DQ name, or `[[export('name')]]`
to choose its native name:

```dq
func Add(left : int32, right : int32) -> int32 [[cexport]]:
    return left + right
endfunc
```

The corresponding C declaration includes the header for fixed-width integers:

```c
#include <stdint.h>

int32_t Add(int32_t left, int32_t right);
```

The C and DQ files must be compiled for the same target and linked together.
The export attribute supplies the symbol name; it does not itself build a
shared library or arrange runtime initialization for a C-owned entry point. See
[Attributes](180-attributes.md#linkage-and-placement) for exported globals and
symbol retention.

Avoid letting a DQ exception cross a native call boundary unless that interface
explicitly supports it. Handle failures within the DQ entry point when needed.

## External Global Variables

A global variable may also come from native code:

```dq
var native_counter : int32 [[external('native_counter')]]
```

The linked native code must supply that symbol. Some C headers expose apparent
globals through macros or functions instead. For example, do not assume `errno`
is an exported integer variable; use the platform's real interface.

## Structures, Unions, and Layout

When passing a C record or union, match its field types, order, padding,
alignment, and calling convention. Check `SizeOf` and `OffsetOf` on the target.
Use `[[packed]]` only when the corresponding C record is packed the same way;
packing a normally aligned record changes its layout.

A union gives several fields the same storage without remembering which one
was written. Ensure the selected field contains a valid value before reading
it. See [Structures and Unions](100-structures-and-unions.md) and
[Attributes](180-attributes.md).

## Buffer and String Details

`embstr(N)` has `N` storage bytes including its zero terminator, leaving room
for at most `N - 1` content bytes. A C writer must respect both that capacity
and any runtime descriptor requirements. Raw writes into a dynamic `str` must
not bypass its copy-on-write and length handling.

C termination-based APIs see only the text before the first zero byte. To pass
binary data or text containing embedded zeros, use an API that takes a pointer
and a length. DQ text operations preserve embedded zeros when the length is
known.

`rostr` also converts to `^char` in C variadic arguments, such as a `%s`
argument to `printf`. Neither this conversion nor `.pchar` extends the source
lifetime. See [Conversions and Borrowed Lifetimes](070-strings-and-characters.md#conversions-and-borrowed-lifetimes).

## WebAssembly Imports

For WebAssembly functions, `[[external('name', 'module')]]` can select both the
import name and its module. The second argument applies only to function
imports on that target, not to external global variables.
