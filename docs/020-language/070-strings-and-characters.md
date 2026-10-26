# Character and String Types

A string stores text as a sequence of bytes. A character represents one byte,
one UTF-16 code unit, or one Unicode scalar, depending on its type. DQ provides
owned strings, borrowed views, and fixed-capacity buffers so code can choose
how storage is allocated and who keeps it alive. For methods, standalone helpers,
and text operations, see [String Functions](075-string-functions.md).

```dq
use print

func AppendField(buffer : embstr, key : rostr, value : strslice):
    buffer.AddFmt('"{}":"{}"', [key, value])
endfunc

func *Main():
    var name : rostr = "Ada Lovelace"
    var first_name : strslice = name[0:3]
    var field : embstr(32) = ""
    AppendField(field, "name", first_name)
    field.AddFmt(', "year":{}', [1979])

    var json : str = ""
    json.Reserve(field.length + 2)
    json.Add('{')
    json.Add(field)
    var closing : ^char = "}"
    json.Add(closing)
    PrintLn(json)
endfunc
```

The output is `{"name":"Ada", "year":1979}`. `name` borrows zero-terminated text as `rostr`,
and `first_name` borrows a byte range as `strslice`. `field` owns a fixed buffer
as `embstr(32)`; the unsized `embstr` parameter lets `AppendField` write into
that buffer with a single `AddFmt()` call. The `Add()` accepts all string types, including `^char`.

`Reserve()` is optional: a dynamic string grows automatically as text is added.
Here it reserves enough capacity for the field and both braces before assembly,
improving performance by avoiding heap reallocations and copying as the string
grows. It does not change the string's length or contents.

## Character Types

| Type | Meaning |
| --- | --- |
| `char` | unsigned 8-bit byte: one ASCII character, one character from a single-byte code page, or one UTF-8 code unit (one Unicode character uses 1...4 UTF-8 code units) |
| `char16` | unsigned 16-bit UTF-16 code unit (one Unicode character uses 1...2 UTF-16 code units)<br>Recommended to use only for Windows API compatibility |
| `wchar` | unsigned 32-bit Unicode scalar value |

`char`, `char16`, `wchar` are distinct types, not integer aliases. `Ord` converts a character to an
integer; explicit or checked conversion creates a character from an integer.

A **code unit** is one storage element of a text encoding. For example, `😀`
uses four `char` bytes in UTF-8 or two `char16` values in UTF-16, but its
character value fits in one `wchar`.

A character literal can initialize any character type whose range includes its
value. Character variables do not implicitly convert to integers or to other
character types. To convert between character types, pass through the integer
code returned by `Ord()`:

```dq
var c8  : char   = 'A'
var c16 : char16 = '€'
var c32 : wchar  = '😀'

var code : int   = Ord(c8)             // 65
var same : char  = char(code)          // 'A'
var wide : wchar = wchar(Ord(c16))     // '€'
var next : char  = char(Ord(c8) + 1)   // 'B'

var checked  : char  = IntToChar(code)     // 'A'; checks the range at runtime
var fallback : wchar = IntToWchar(0xD800, '?') // '?' for an invalid scalar
```

These conversions preserve the numeric code; they do not encode or decode UTF-8
or UTF-16. Explicit casts reject invalid constant values at compile time, but
runtime casts can truncate to the destination width and do not validate Unicode
scalars. Use `IntToChar()` or `IntToWchar()` when runtime validation is needed.
Without a fallback argument, an invalid value raises a runtime error.

The following examples intentionally produce compiler errors:

```dq
var c8     : char = 'A'
var c32    : wchar = 'A'
var code   : int = 65

c8 = code           // error: no implicit integer-to-character conversion
code = c8           // error: use Ord(c8)
code = int(c8)      // error: an integer cast cannot replace Ord()
c32  = c8           // error: no implicit conversion between character types
c8 = char(c32)      // error: use char(Ord(c32)) or IntToChar(Ord(c32))
c8 = c8 + 1         // error: arithmetic requires Ord(c8)

var too_large : char = '€'             // error: the literal does not fit in one byte
var overflow  : char = char(256)       // error: constant is outside 0 .. 255
var non_bmp   : char16 = '😀'          // error: the literal does not fit in one code unit
var surrogate : wchar = wchar(0xD800)  // error: a surrogate is not a Unicode scalar
```

## Text Types

| Type | Ownership and representation |
| --- | --- |
| [`str`](#str-dynamic-strings) | Dynamic, heap-allocated, reference-counted copy-on-write byte string.<br>May contain internal zero bytes and is not necessarily valid UTF-8.<br>A managed type that cannot be a `struct` field.<br>The current length is always stored. |
| [`embstr(N)`](#embstrn-and-embstr-embedded-strings) | Embedded string: fixed-capacity, mutable, zero-terminated byte storage.<br>A zero-byte scan may be required to calculate the length. |
| [`embstr`](#embstrn-and-embstr-embedded-strings) | Writable, appendable borrowed reference to a shared embedded string descriptor, which carries the storage capacity. Typically used for function parameters.<br>A zero-byte scan may be required to calculate the length. |
| [`rostr`](#rostr-read-only-zero-terminated-strings) | Read-only borrowed, zero-terminated byte string.<br>A zero-byte scan may be required to calculate the length. |
| [`strslice`](#strslice-string-slices) | Read-only borrowed view of text bytes. Zero-termination is not guaranteed.<br>Length is always known; conversion from raw or embedded storage calculates it before creating the view. |
| [`^char`](#char-raw-c-string-pointers) | Raw pointer to text bytes; C string operations expect a terminating zero byte. |

On targets built with dynamic strings disabled, `str` and operations that
produce it are unavailable. Text literals, `rostr`, `strslice`, and `embstr(N)` remain
available for text processing with borrowed or fixed storage.

### `str`: Dynamic Strings

`str` allocates heap memory for its contents. As the contents grow, the backing
memory block may be reallocated. The current length is always known. A trailing
zero byte maintains C string compatibility. The contents can also hold arbitrary
binary data, including internal zero bytes. It is a **managed type**: the runtime
tracks references and frees the memory when the string data is no longer used.
A `str` cannot be a `struct` field. Its maximum text length is `2147483647` bytes.
Its `.length` reads the stored byte count without scanning the text.

### `embstr(N)` and `embstr`: Embedded Strings

`embstr(N)` provides fixed storage of `N` bytes. It holds up to `N - 1` text
bytes plus a terminating zero byte, making it compatible with C strings. The
position of the terminating zero determines the text length, so calculating
the length may require a scan at runtime. DQ can cache the length to avoid
repeated scans. Embedded strings can be stored in the program's data segment,
on the stack, or inside structures and objects. Their maximum text length is
`16777215` bytes.

For `embstr(N)`, `.storage_size` is `N` and `.maxlength` is `N - 1`.
Assignment and append operations truncate to fit the fixed capacity:

```dq
var fixbuf : embstr(8) = "abc"
fixbuf.Append("defghi") // stores "abcdefg"
```

Use an unsized `embstr` parameter to modify an existing fixed buffer through its
shared [`SDqTextInfo` descriptor](#text-descriptors). Assigning or passing it
preserves the descriptor reference, so length updates made through one alias
are visible to the others. It remains writable and appendable: character writes,
assignment, `Add()`, and `AddFmt()` modify the original buffer within its fixed
capacity. The descriptor carries that capacity across function calls even though
the parameter's type is written without `N`.

The inline storage has no length field. The descriptor carries the capacity and
caches the current length when available; otherwise the runtime scans within
the buffer's capacity to calculate it.

### `rostr`: Read-Only Zero-Terminated Strings

Use `rostr` for read-only string parameters when zero-termination is required.
It borrows text literals, `str`, `embstr(N)`, unsized `embstr`, or `^char` without
copying the text bytes or retaining their storage. Its descriptor can be
reassigned, but character writes and writable character references through it
are rejected.

A `rostr` carries a pointer and either a known length or an indication that the
length is unknown. Borrowing a literal or a `str` preserves its known length.
Borrowing a raw `^char` does not scan immediately; the first operation needing
the length, such as `.length`, scans and caches it in that `rostr` descriptor.

### `strslice`: String Slices

Use `strslice` for read-only byte ranges with explicit bounds. It borrows text
without copying its bytes or retaining its storage. A slice can describe an
unterminated range or a range containing zero bytes.

Its length is always known, so `.length` reads the stored byte count directly.
Conversion from raw or embedded storage resolves an unknown length before
creating the slice; embedded-string scans stop at the buffer's capacity.
Converting a `rostr` with an unknown length also resolves that length first.

Use `StrSlice(data, length)` to supply the length explicitly and avoid scanning:

```dq
var data : [3]char = ['A', char(0), 'B']
var view : strslice = StrSlice(%data[0], data.length)
```

### `^char`: Raw C String Pointers

Use `^char` for raw C interfaces. It carries only a pointer, with no length or
ownership information. Converting it to a borrowed string cannot recover a
known length from the pointer alone. If the length is already known, use
`StrSlice(data, length)` to pass it explicitly.

`.pchar` exposes a borrowed `^char` for compatible zero-terminated text storage.
Operations that move or replace the source storage invalidate that pointer.
A nil `^char` is not valid where a C string is required unless the called API
explicitly defines nil handling. See
[C Interoperability](200-c-interoperability.md#strings-buffers-and-borrowing)
for passing text to C functions.

### Length Calculation and Zero-Byte Scanning

A zero-byte scan reads successive bytes until the terminating zero is found.
Its cost grows with the text length. Keeping or supplying a known byte length
avoids this work.

The following example shows when scanning happens for `rostr` and `strslice`
parameters:

```dq
use print

func GetLength(astr : rostr) -> int:
    result = astr.length
endfunc

func GetSliceLength(astr : strslice) -> int:
    result = astr.length
endfunc

func *Main():
    var sr : rostr = "asdfg";  // length calculated at compile time
    PrintLn("rostr length = {}", [GetLength(sr)])   // uses the cached length at astr.length
    var pc : ^char = "asdfg" // no length info
    PrintLn("pointer length = {}", [GetLength(pc)])      // scans at astr.length
    PrintLn("pointer length (second call) = {}", [GetLength(pc)]) // scans again
    PrintLn("slice parameter length = {}", [GetSliceLength(pc)]) // scans before entry

    var view : strslice = pc  // scans now; view stores the resulting length
    PrintLn("slice length = {}", [view.length])          // no scan
    PrintLn("saved slice length = {}", [GetSliceLength(view)]) // no scan
endfunc
```

### Text Descriptors

The compiler carries text pointers together with metadata in small structures
called **descriptors**. For calls with `embstr`, `rostr`, or `strslice` arguments,
it creates temporary descriptors as needed during argument conversion.
The compiler may pass either a pointer to the descriptor or its members directly as function arguments.

For `embstr`, the runtime uses the following descriptor structure:

```dq
struct SDqTextInfo:
    dataptr : ^char   // points to the first text byte
    charlen : int32   // known byte length, or a negative value when unknown
    info    : uint32  // bits 0..23: maximum length; bits 24..31: flags
endstruct
```

For `rostr` and `strslice`, the runtime uses a more compact descriptor:

```dq
struct SDqRoStrInfo:
    dataptr : ^char
    charlen : int32   // rostr: negative means unknown; strslice: always non-negative
endstruct
```

`charlen` counts text bytes, excluding any terminating zero byte. A non-negative
value indicates a known length; `0` represents a known empty string. For `embstr`
and `rostr`, a negative value means that the length must be calculated when
needed. For `strslice`, the length is always known and non-negative.

### Conversions and Borrowed Lifetimes

| Destination | Relevant rule |
| --- | --- |
| `str` | Owns/retains its text value; accepts compatible text views. |
| `rostr` | Requires terminated storage, borrows it, and does not retain its owner. |
| `strslice` | Borrows a byte range with an always-known length; conversion resolves an unknown source length first. No terminator is promised. |
| `embstr(N)` | Copies into fixed storage and truncates to fit. |
| Unsized `embstr` | Shares a writable embedded string descriptor; requires suitable existing storage. |
| `^char` | A raw C pointer with no automatic lifetime extension. |

Slicing a `rostr` by byte offsets returns `strslice`, including full-range and
suffix slices. A `rostr` converts to `strslice`, resolving any unknown length
first. There is no direct conversion from `strslice` to `rostr`, even by a cast.
When terminated storage is needed for a slice, first copy it into an owning
`str` or a fixed-capacity `embstr(N)`.

Both `rostr` and `strslice` convert to owned `str` or fixed-capacity `embstr(N)`.
These conversions copy the byte range, truncating only when the fixed buffer
lacks capacity. A `rostr` cannot become an unsized writable `embstr` alias.

```dq
var owned : str = "hello"
var borrowed : rostr = owned
var part : strslice = borrowed[1:4]  // borrows "ell"
var copied : str = part             // owns a copy of "ell"
```

Borrowed text must not outlive its source. The owner must stay alive, and its
storage and contents must remain stable while a read-only view is used. An
unsized `embstr` requires live, writable source storage. Neither a cast nor
storing a view in `anyvalue` extends the owner's lifetime. Returning a view into
a local buffer is invalid.

See [Memory Management and Ownership](130-memory-management-and-ownership.md)
for managed values and borrowed lifetimes.

## Literals

Double quotes always represent text. A single-quoted literal containing exactly
one Unicode scalar is `wchar`; empty or longer single-quoted literals are text.

```dq
var path       : str = "/"
var slash      : wchar = '/'
var byte_slash : char = '/'
var greeting   : rostr = "Hello, world!"
var field      : rostr = '"name":"Ada"'
```

The `byte_slash` initialization is accepted because the literal is a compile-time
scalar below 256. In general, conversion from `wchar` to `char` is explicit.
Single quotes around longer text let it contain double quotes without escaping
them, as in `field`.

For longer text that spans several lines, use triple double quotes (`"""`) or
triple single quotes (`'''`):

```dq
var document : rostr = """
    {
      "name": "Ada",
      "year": 1979
    }
    """
```

The opening and closing lines containing only the delimiters are removed.
The closing delimiter's indentation determines how much common indentation is
removed from each content line. In this example, the two spaces before each JSON
field remain, and there is no leading or trailing newline.

Line breaks within the text are preserved. Escape sequences such as `\n`, `\t`,
and `\\` are still processed. Double quotes inside a triple-double-quoted literal
do not need escaping unless they form the closing delimiter. Triple-single-quoted
literals follow the same rules with single quotes.

## Dynamic String Value Semantics

Assigning a `str` shares storage. Mutation detaches when necessary so other
string values retain their contents.

```dq
var a : str = "abc"
var b : str = a
b[0] = 'X'       // a remains "abc"
```

`Clone()` forces independent storage. Operations that can grow or detach a
string invalidate borrowed pointers and views into its previous storage.
