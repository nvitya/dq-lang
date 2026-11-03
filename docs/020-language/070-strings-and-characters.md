# Character and String Types

A string stores text as a sequence of bytes. A character represents one byte,
one UTF-16 code unit, or one Unicode character, depending on its type. DQ provides
owned strings, borrowed views, and fixed-capacity buffers so code can choose
how storage is allocated and who keeps it alive. For methods, standalone helpers,
and text operations, see [String Functions](075-string-functions.md).

Example code using strings:
```dq
use print

func AppendField(buffer : embstr, key : rostr, value : strslice):
    buffer.AddFmt('"{}":"{}"', [key, value])
endfunc

func *Main():
    var name : rostr = "Ada Lovelace"
    var first_name : strslice = name[0:3]
    var field : embstr(32)
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
| `wchar` | unsigned 32-bit Unicode character value, excluding UTF-16 surrogate values |

UTF-16 uses pairs of surrogate values to encode some characters. A single
surrogate value does not represent a complete character on its own.

`char`, `char16`, and `wchar` are distinct types, not integer aliases. The
read-only `.ord` property returns a character's integer code as `int`. Prefer
`c.ord`; `Ord(c)` returns the same value. Explicit or checked conversion creates
a character from an integer.

A **code unit** is one storage element of a text encoding. For example, the `😀` symbol
uses four `char` bytes in UTF-8 or two `char16` values in UTF-16, but its
character value fits in one `wchar`.

A character literal can initialize any character type whose range includes its
value. Character variables do not implicitly convert to integers or to other
character types. To convert between character types, pass through the integer
code exposed by `.ord`:

```dq
var c8  : char   = 'A'
var c16 : char16 = '€'
var c32 : wchar  = '😀'

var code : int = c8.ord                 // 65
var same_code : int = Ord(c8)           // 65: equivalent function form
var emoji_code : int = c32.ord          // 128512
var same : char = char(code)           // 'A'
var wide : wchar = wchar(c16.ord)       // '€'
var next : char = char(c8.ord + 1)      // 'B'

var checked  : char  = IntToChar(code)     // 'A'; checks the range at runtime
var fallback : wchar = IntToWchar(0xD800, '?') // '?' for an invalid character value
```

These conversions preserve the numeric code; they do not encode or decode UTF-8
or UTF-16. Explicit casts reject invalid constant values at compile time, but
runtime casts can truncate to the destination width and do not validate Unicode
character values. Use `IntToChar()` or `IntToWchar()` when runtime validation is
needed. Without a fallback argument, an invalid value raises a runtime error.

The following examples intentionally produce compiler errors:

```dq
var c8     : char = 'A'
var c32    : wchar = 'A'
var code   : int = 65

c8 = code           // error: no implicit integer-to-character conversion
code = c8           // error: use c8.ord
code = int(c8)      // error: an integer cast cannot replace .ord
c32  = c8           // error: no implicit conversion between character types
c8 = char(c32)      // error: use char(c32.ord) or IntToChar(c32.ord)
c8 = c8 + 1         // error: use char(c8.ord + 1)
c8.ord = 66         // error: .ord is read-only

var too_large : char = '€'             // error: the literal does not fit in one byte
var overflow  : char = char(256)       // error: constant is outside 0 .. 255
var non_bmp   : char16 = '😀'          // error: the literal does not fit in one code unit
var surrogate : wchar = wchar(0xD800)  // error: wchar excludes UTF-16 surrogate values
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

### The Length Calculation Problem

DQ maintains strong compatibility with C APIs, so its zero-terminated string
buffers can be passed directly to C functions. C APIs that work with text usually
accept a pointer to the start of a text buffer, written as `^char` in DQ. This
pointer carries no information about the text's length.

Zero-terminated strings mark the end of their text with a byte whose value is
zero. This is different from the printable character `'0'`. The terminating byte
is not counted in the text length: `"hello"` needs five text bytes plus one zero
byte.

To calculate the length from that pointer, the program must read from the start
until it finds the zero byte. Longer text takes more work, and repeated length
calculations can scan the same bytes again.

Storing the byte count avoids this work. DQ's string types handle this in
different ways, using stored or cached lengths where possible, as described below.

### `str`: Dynamic Strings

`str` allocates heap memory for its contents. As the contents grow, the backing
memory block may be reallocated. The current length is always known. A trailing
zero byte maintains C string compatibility. The contents can also hold arbitrary
binary data, including internal zero bytes. It is a **managed type**: the runtime
tracks references and frees the memory when the string data is no longer used.
A `str` cannot be a `struct` field. Its maximum text length is `2147483647` bytes.
Its `.length` reads the stored byte count without scanning the text.

An empty `str` may require no heap allocation. Internally, it can be represented
by a `nil` pointer, indicating that no dynamic string handler object has been
allocated for it yet.

Assigning a `str` shares storage. Mutation detaches when necessary so other
string values retain their contents.

```dq
var a : str = "abc"
var b : str = a
b[0] = 'X'       // a remains "abc"

var c : str = a.Clone() // independent storage immediately
c[0] = 'Y'             // a remains "abc"; b remains "Xbc"

c.Reserve(32)          // space for at least 32 text bytes; c is still "Ybc"
c.Add("def")          // reuses the reserved storage; c is now "Ybcdef"
```

### `embstr(N)` and `embstr`: Embedded Strings

`embstr(N)` provides fixed storage of `N` bytes. It holds up to `N - 1` text
bytes plus a terminating zero byte, making it compatible with C strings. The
position of the terminating zero determines the text length, so calculating
the length may require a scan at runtime. DQ can cache the length to avoid
repeated scans. Embedded strings can be stored in the program's data segment,
on the stack, or inside structures and objects. Their maximum text length is
`16777215` bytes.

A declaration without an initializer creates an empty string. `.Add()` and
`.AddFmt()` can be called immediately after the declaration:

```dq
var s : embstr(16)
s.Add("value=")           // = "value=": contents of s
s.AddFmt("{}", [42])      // = "value=42": contents of s
```

For a declaration without an initializer, only the leading zero terminator is
guaranteed; bytes beyond it are unspecified.

For `embstr(N)`, `.storage_size` is `N` and `.maxlength` is `N - 1`.
Assignment and append operations truncate to fit the fixed capacity:

```dq
var fixbuf : embstr(8) = "abc"
fixbuf.Append("defghi") // stores "abcdefg"
```

Use an unsized `embstr` parameter to modify an existing fixed buffer through its
shared [descriptor](#text-descriptors). It remains writable and appendable: character writes,
assignment, `Add()`, and `AddFmt()` modify the original buffer within its fixed
capacity.

### `rostr`: Read-Only Zero-Terminated Strings

Use `rostr` for read-only string parameters when zero-termination is required.
It borrows text literals, `str`, `embstr(N)`, unsized `embstr`, or `^char` without
copying the text bytes or retaining their storage. Its [descriptor](#text-descriptors) can be
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

```dq
func puts(text : ^char) -> int32 [[external]] ..

func *Main():
    var owned : str = "dynamic text"
    var readonly : rostr = "read-only text"
    var buffer : embstr(16) = "fixed text"

    var owned_ptr : ^char = owned.pchar
    var readonly_ptr : ^char = readonly.pchar
    var buffer_ptr : ^char = buffer.pchar
    puts(owned_ptr)
    puts(readonly_ptr)
    puts(buffer_ptr)
endfunc
```

This prints `dynamic text`, `read-only text`, and `fixed text` on separate lines.
Each pointer refers to the source's existing text bytes; no text is copied.
The `.pchar` property is also available on unsized `embstr` references.

If the string's text moves to a different memory location, for example when a
dynamic string grows, the old pointer must no longer be used. Get a new pointer
with `.pchar` after making the change.
A nil `^char` is not valid where a C string is required unless the called API
explicitly defines nil handling. See
[C Interoperability](200-c-interoperability.md#strings-buffers-and-borrowing)
for passing text to C functions.

### Text Descriptors

The compiler carries text pointers together with metadata in small structures
called **descriptors**. For calls with `embstr`, `rostr`, or `strslice` arguments,
it creates temporary descriptors as needed during argument conversion.
The compiler may pass either a pointer to the descriptor or its members directly as function arguments.

These descriptors are invisible to the user, the compiler creates,
updates, and passes them automatically.

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
| `rostr` | Requires existing terminated storage (like a string literal), borrows it. |
| `strslice` | Requires existing storage, borrows a byte range with an always-known length; conversion resolves an unknown source length first. No terminator is promised. |
| `embstr(N)` | Creates an embedded fixed-storage of N bytes. Written texts are truncated to fit. |
| Unsized `embstr` | Shares a fixed-storage, writeable embedded string. |
| `^char` | A raw C pointer to an existing storage with no automatic lifetime extension. |

Slicing a `rostr` by byte offsets returns `strslice`, including full-range and
suffix slices. A `rostr` converts to `strslice`, resolving any unknown length
first. There is no direct conversion from `strslice` to `rostr`, even by a cast.
When terminated storage is needed for a slice, first copy it into an owning
`str` or a fixed-capacity `embstr(N)`.

Both `rostr` and `strslice` convert to owned `str` or fixed-capacity `embstr(N)`.
These conversions copy the byte range, truncating only when the fixed buffer
reached its capacity.

```dq
var owned    : str = "hello"
var borrowed : rostr = owned
var part     : strslice = borrowed[1:4]  // borrows "ell"
var copied   : str = part             // owns a copy of "ell"
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
one Unicode character is `wchar`; empty or longer single-quoted literals are text.

```dq
var path       : str = "/"
var slash      : wchar = '/'
var byte_slash : char = '/'
var greeting   : rostr = "Hello, world!"
var field      : rostr = '"name":"Amy"'
```

The `byte_slash` initialization is accepted because the literal's character code
is known at compile time and is below 256. In general, conversion from `wchar` to
`char` is explicit.
Single quotes around longer text let it contain double quotes without escaping
them, as in `field`.

### Multiline Text

For text that spans several lines, put three double quotes (`"""`) or three
single quotes (`'''`) around it:

```dq
var message : rostr = """
    First line.
    Second line.
    """
PrintLn(message)
```

This prints:

```text
First line.
Second line.
```

The text starts with "F" and ends with the final ".". Putting the quotes on
separate lines does not add an empty line before or after the text. The line
break between `First line.` and `Second line.` is part of the string, just as
if you had written `"First line.\nSecond line."`.

You can indent the text to keep the source code readable. DQ uses the spaces
before the closing quotes to decide how much indentation to remove. Extra
spaces are kept:

```dq
var indented : rostr = '''
    First line.
      Indented second line.
    '''
PrintLn(indented)
```

There are four spaces before the closing quotes, so DQ removes four spaces
from the start of each text line. The second line has six spaces, so two remain:

```text
First line.
  Indented second line.
```

This also makes longer text, such as JSON, easier to write. Ordinary double
quotes inside triple double quotes do not need a backslash:

```dq
var document : rostr = """
    {
      "name": "Amy",
      "year": 1999
    }
    """
PrintLn(document)
```

This prints:

```json
{
  "name": "Amy",
  "year": 1999
}
```

Escape sequences still work inside multiline text: `\n` adds a line break,
`\t` adds a tab, and `\\` adds one backslash. Three matching quotes end the text;
the indentation and line-break rules are the same for both quote styles.
