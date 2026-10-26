# Strings and Characters

A string stores text as a sequence of bytes. A character represents one byte,
one UTF-16 code unit, or one Unicode scalar, depending on its type. DQ provides
owned strings, borrowed views, and fixed-capacity buffers so code can choose
how storage is allocated and who keeps it alive.

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
| `str` | Dynamic, heap-allocated, reference-counted copy-on-write byte string.<br>May contain internal zero bytes and is not necessarily valid UTF-8.<br>A managed type that cannot be a `struct` field.<br>The current length is always stored. |
| `embstr(N)` | Embedded string: fixed-capacity, mutable, zero-terminated byte storage.<br>A zero-byte scan may be required to calculate the length. |
| `embstr` | Writable, appendable borrowed reference to a shared embedded string descriptor, which carries the storage capacity. Typically used for function parameters.<br>A zero-byte scan may be required to calculate the length. |
| `rostr` | Read-only borrowed, zero-terminated byte string.<br>A zero-byte scan may be required to calculate the length. |
| `strslice` | Read-only borrowed view of text bytes. Zero-termination is not guaranteed.<br>Length is always known; conversion from raw or embedded storage calculates it before creating the view. |
| `^char` | Raw pointer to text bytes; C string operations expect a terminating zero byte. |

On targets built with dynamic strings disabled, `str` and operations that
produce it are unavailable. Text literals, `rostr`, `strslice`, and `embstr(N)` remain
available for text processing with borrowed or fixed storage.

### Choosing a String Type

DQ provides two forms of mutable string storage: `str` and `embstr(N)`.

`str` allocates heap memory for its contents. As the contents grow, the backing
memory block may be reallocated. The current length is always known. A trailing
zero byte maintains C string compatibility. The contents can also hold arbitrary
binary data, including internal zero bytes. It is a **managed type**: the runtime
tracks references and frees the memory when the string data is no longer used.
A `str` cannot be a `struct` field. Its maximum text length is `2147483647` bytes.

`embstr(N)` provides fixed storage of `N` bytes. It holds up to `N - 1` text
bytes plus a terminating zero byte, making it compatible with C strings. The
position of the terminating zero determines the text length, so calculating
the length may require a scan at runtime. DQ can cache the length to avoid
repeated scans. Embedded strings can be stored in the program's data segment,
on the stack, or inside structures and objects. Their maximum text length is
`16777215` bytes.

Use `rostr` or `strslice` to pass text for reading without copying its bytes or
retaining its storage. Prefer `rostr` when zero-termination is required and
`strslice` for byte ranges with explicit bounds. Use an unsized
`embstr` parameter to modify an existing fixed buffer through its shared
descriptor. Use `^char` for raw C interfaces; the pointer carries no length or
ownership information.

See [Memory Management and Ownership](130-memory-management-and-ownership.md)
for managed values and borrowed lifetimes.

### Length Calculation and Zero-Byte Scanning

A zero-byte scan reads successive bytes until the terminating zero is found.
Its cost grows with the text length. Keeping or supplying a known byte length
avoids this work:

- `str` stores its current length and updates it during mutation, so `.length`
  does not scan the text.
- `rostr` carries a pointer and either a known length or an indication that the
  length is unknown. Borrowing a literal or a `str` preserves its known length.
  Borrowing a raw `^char` does not scan immediately; the first operation needing
  the length, such as `.length`, scans and caches it in that `rostr` descriptor.
- `strslice` always carries a known length, so `.length` reads it directly.
  Conversion from raw or embedded storage resolves an unknown length before
  creating the slice; embedded-string scans stop at the buffer's capacity.
  `StrSlice(data, length)` supplies the length explicitly and avoids scanning.
- `embstr(N)` stores its bytes inline, without a length field in that storage.
  The DQ runtime handles it through an `SDqTextInfo` descriptor, which carries
  the capacity and caches the length when available.
- `^char` carries only a pointer. Converting it to a borrowed string cannot
  recover a known length from the pointer alone. If the length is already known,
  use `StrSlice(data, length)` to pass it explicitly.

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

Prefer `rostr` for read-only string parameters: its descriptor can preserve a
known length and avoid repeated scanning.

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

## Literals

Double quotes always represent text. A single-quoted literal containing exactly
one Unicode scalar is `wchar`; empty or longer single-quoted literals are text.

```dq
var path : str = "/"
var slash : wchar = '/'
var byte_slash : char = '/'
```

The last initialization is accepted because the literal is a compile-time
scalar below 256. In general, conversion from `wchar` to `char` is explicit.

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

## Text Operators

`+` concatenates compatible text operands into a new `str`. `+=` appends to a
dynamic string. These operations require dynamic-string support.

```dq
var greeting : str = "Hello, " + "world"
greeting += "!"
var expected : rostr = "Hello, world!"
var matches : bool = greeting == expected
```

Using `+` to assemble dynamic strings is not recommended: it creates temporary
strings and can cause repeated heap allocations and copies. Prefer `Add()` for
plain text and `AddFmt()` for formatted text. Call `Reserve()` first when the
expected size is known or can be estimated. Reserving enough capacity improves
performance by avoiding heap reallocations and the associated copying of existing
text as the string grows. Appends reuse that storage until the reserved capacity
is exceeded.

```dq
var text : str = ""
text.Reserve(32)
text.Add("Hello, ")
text.AddFmt("{}!", ["world"])
```

## String Comparison

Text values support `==`, `<>`, `<`, `<=`, `>`, and `>=`. You can compare
different string types, including `str`, `rostr`, `strslice`, and embedded
strings with or without an explicit capacity.

Strings are compared byte by byte, from left to right. The first different byte
determines which string comes first, using unsigned byte values. If one string
is the beginning of the other, the shorter string comes first. For example,
`"alpha" < "beta"` and `"app" < "apple"` are both true.

```dq
var name : str = "alpha"
var limit : rostr = "beta"
var before : bool = name < limit       // true
var prefix : bool = "app" < "apple"    // true
var inside : bool = "a" <= name < "z" // true
```

Comparison is case-sensitive: `"Hello"` and `"hello"` are different. It does
not validate UTF-8, normalize Unicode, or apply language-specific sorting rules.
Known string lengths include embedded zero bytes; raw C strings end at their
first zero.

`StrSliceCmp(a : strslice, b : strslice) -> int`, available through implicit
`sys`, provides the same bytewise comparison as a function. It returns a negative
value if `a` comes before `b`, zero if they are equal, and a positive value if
`a` comes after `b`. It accepts the text types through their normal conversions and
compares the entire known byte range, without requiring a zero terminator.

A raw `^char` operand compares text when paired with a string value or, for
ordering, a text literal. Two raw pointer variables compare their addresses;
use `StrSliceCmp` to compare their text contents explicitly.

## Read-Only Zero-Terminated Strings

`rostr` borrows terminated storage from text literals, `str`, `embstr(N)`,
unsized `embstr`, or `^char`. It does not copy bytes or retain the owner.
The owner must stay alive and its storage and contents must remain stable while
borrowed. Assigning a `rostr` copies its descriptor; the descriptor itself may
be reassigned, but character writes and writable character references are rejected.

```dq
func strlen(text : ^char) -> uint [[external]]
func Inspect(text : rostr):
    var size : uint = strlen(text)  // passes the underlying pointer directly
    var part : strslice = text[1:4]
endfunc

var owned : str = "hello"
var borrowed : rostr = owned
Inspect(borrowed)
```

`.length` counts bytes and `.pchar` exposes the borrowed pointer. Indexing,
slicing, comparison, and Unicode access follow the existing string rules.
Every byte slice returns `strslice`, including full-range and suffix slices.
There is no direct conversion from `strslice` to `rostr`, even by a cast. When
terminated storage is needed for a slice, first assign it to an owning `str`.
A `rostr` converts to `strslice`, resolving any unknown length before creating
the slice, or to owned `str` or fixed-capacity `embstr(N)`;
it cannot become an unsized writable `embstr` alias.
A `strslice` converts to owned `str` or fixed-capacity `embstr(N)`, copying its
byte range into the destination and truncating only when the destination lacks capacity.

Known lengths preserve embedded zero bytes. DQ text operations can process
those bytes, while C functions see only the prefix before the first zero.
`rostr` converts automatically to declared `^char` parameters and to `^char`
in C variadic positions, so `printf("%s", borrowed)` passes a pointer.
Raw pointers retain their existing mutability; callers must respect the
borrowed storage's read-only contract.

Standard-library paths, names, commands, messages, and format strings use
`rostr`. Parser input, generic text algorithms, output chunks, and byte payloads
continue to use `strslice` so they accept slices without allocating.

Use `StrSlice(data, length)` to borrow a known-length byte range from a
`^char`, including an unterminated range or one containing zero bytes. The
caller keeps the storage alive and stable for as long as the view is used.

```dq
var view : strslice = StrSlice(data, data_length)
buffer.Append(view)
```

## Byte Indexing and Slicing

`text[index]` reads one `char` byte; writable text storage also permits replacing it. `text[start:end]` uses a half-open
byte range, while `text[start::end]` includes the end byte. These operations do
not decode or validate UTF-8.

An index must select an existing byte and an invalid index raises a runtime range
error. Slice bounds are clamped to `[0, length]`; reversed normalized bounds
produce an empty slice rather than an error. `$last` is the last existing byte
index and `$end` is the one-past-end position.

## Unicode Operations

Unicode-oriented operations interpret `str` bytes as UTF-8:

- `.wclen` counts decoded Unicode scalar values;
- `.wchar[index]` selects a scalar;
- `.wchar[start:end]` returns scalar values;
- `.wcstr[start:end]` returns the selected scalars encoded as `str`;
- `ToWchars` and `StrFromWchars` convert between UTF-8 and `[*]wchar`;
- `ToUtf16` and `StrFromUtf16` convert between UTF-8 and UTF-16 code units.

Malformed UTF-8, invalid scalar values, and malformed UTF-16 produce runtime
encoding errors. `ToUtf16` returns an array that includes a final zero code unit.

## Embedded Strings

An embedded string, `embstr(N)`, owns exactly `N` bytes of storage. Its final byte
is reserved for the zero terminator, so it stores at most `N - 1` visible bytes.
Assignment and append-style operations truncate to fit rather than growing the
value.

```dq
var fixbuf : embstr(8) = "abc"
fixbuf.Append("defghi") // stores "abcdefg"
```

An unsized `embstr` borrows existing C-string storage with a fixed capacity
through a shared [`SDqTextInfo` descriptor](#text-descriptors). Assigning or passing
it preserves that descriptor reference, so length updates made through one alias
are visible to the others.
It remains writable and appendable: character writes, assignment, `Add()`, and
`AddFmt()` modify the original buffer within its fixed capacity.

The compiler carries the storage capacity with the reference, including when it
is passed to another function. Although the type is written without `N`, the
descriptor retains the buffer's size and maximum text length. For storage from
an `embstr(N)`, `.storage_size` is `N` and `.maxlength` is `N - 1`, reserving one
byte for the terminating zero. Append operations use this limit and truncate to
fit. The source storage must remain alive and writable while the reference is used.

## Raw C String Pointers

`.pchar` returns a borrowed `^char` to compatible zero-terminated storage. It is
valid only while the source is alive and unchanged in a way that can move its
storage. C functions stop at the first zero byte even when the originating
`str` contains later bytes.

A nil `^char` is not valid where a C string is required unless the called API
explicitly defines nil handling.

## Overlap

String mutation methods preserve the logical source when their source aliases
the destination. This includes appending or inserting a view into the same
string across a possible reallocation.

## Standalone String Helpers

String helpers are available through the implicit `sys` runtime interface;
ordinary modules need no `use` declaration.

| Function | Result | Meaning |
| --- | --- | --- |
| `Format(fmt : rostr, args : []anyvalue)` | `str` | create text using [formatting placeholders](160-text-formatting.md) |
| `StrToInt(text : strslice, defvalue : int = 0)` | `int` | parse decimal digits and a minus sign |
| `StrToFloat(text : strslice, defvalue : float)` | `float` | parse a number with a dot or comma decimal separator and an optional `e`/`E` exponent |
| `StrUpper(text : strslice)` | `str` | copy text, converting ASCII letters to uppercase |
| `StrLower(text : strslice)` | `str` | copy text, converting ASCII letters to lowercase |

The parsing functions return `defvalue` for empty input or an unrecognized
character. `StrToFloat` requires an explicit default value. The case conversion
functions leave non-ASCII bytes unchanged and do not modify the source.

```dq
var label : str = Format("value={:04X}", [255]) // "value=00FF"
var count : int = StrToInt("42")               // 42
var fallback : int = StrToInt("invalid", -1)   // -1
var value : float = StrToFloat("1.25e2", 0)    // 125
var upper : str = StrUpper("Hello, é!")       // "HELLO, é!"
var lower : str = StrLower("Hello, é!")       // "hello, é!"
```

`Format`, `StrUpper`, and `StrLower` create dynamic strings and require
dynamic-string support. `StrToInt` and `StrToFloat` remain available when
dynamic strings are disabled.

## Common Operations

Dynamic strings expose:

| Member | Meaning |
| --- | --- |
| `.length` | current byte length |
| `.capacity` | allocated capacity |
| `Set(text)` | replace contents |
| `Append(value)` / `Add(value)` | append text or a `char` byte |
| `AppendChar(ch)` | append one `char` byte |
| `Prepend(value)` | insert at the beginning |
| `Insert(index, value)` | insert text or a `char` byte |
| `Delete(index, count = 1)` | remove bytes |
| `SetLength(length, fill)` | resize, filling new bytes |
| `Truncate(length)` | shrink only |
| `Pop(count)` / `PopFirst(count)` | remove and return text |
| `Pop()` / `PopFirst()` | remove and return one `char` byte |
| `Reserve(capacity)` | ensure capacity |
| `Compact()` | set capacity to current length |
| `Clear(free_storage = false)` | clear contents, optionally free storage |
| `Clone()` | force an independent copy |
| `AddFmt(fmt, args)` | append formatted text |

```dq
var s : str = "abc"
s.Append("def")
s.Insert(0, '>')
s.Delete(0)
s.AddFmt(" {}", [123])
```

String indexing and slicing use byte offsets and return or copy `char` values.
They do not validate UTF-8.

```dq
var b : char = s[0]
var part : str = s[3:10]
```

Indices are normalized by the runtime operations. For example, deleting past
the end is clamped, and inserting with `$end` appends.

## Utility Methods

The following read-only methods are available on `str`, `rostr`, `strslice`,
and `embstr` values. They accept a `char` or a text value where a search,
prefix, suffix, trim set, or fill value is required. Search positions and
lengths are byte-based, just like `.length` and normal indexing.

| Member | Result | Meaning |
| --- | --- | --- |
| `Trim()` / `Trim(set)` | `str` | remove whitespace, or bytes in `set`, from both ends |
| `LTrim()` / `LTrim(set)` | `str` | remove whitespace, or bytes in `set`, from the left end |
| `RTrim()` / `RTrim(set)` | `str` | remove whitespace, or bytes in `set`, from the right end |
| `LPad(length, fill)` | `str` | pad the left side to `length` bytes |
| `RPad(length, fill)` | `str` | pad the right side to `length` bytes |
| `IndexOf(needle, start = 0)` | `int` | first matching byte offset, or `-1` |
| `LastIndexOf(needle)` | `int` | last matching byte offset, or `-1` |
| `Contains(needle)` | `bool` | whether the value contains `needle` |
| `StartsWith(prefix)` | `bool` | whether the value begins with `prefix` |
| `EndsWith(suffix)` | `bool` | whether the value ends with `suffix` |

These methods do not modify the receiver. Assign a returned `str` to retain a
trimmed or padded value.

```dq
var text : str = "  abc  "
text = text.Trim()  // "abc"
```

`Trim`, `LTrim`, and `RTrim` use the ASCII whitespace bytes space, tab,
line feed, carriage return, vertical tab, and form feed by default. With an
argument, that argument is a set of bytes rather than a substring pattern.

```dq
var marked : str = "---abc---"
var name : str = marked.Trim("-")  // "abc"
```

Padding repeats the fill value and truncates the final repetition to produce
exactly the requested length. A target length no greater than the receiver
length returns an unchanged copy. An empty fill value raises a runtime error.

```dq
var code : str = "abc"
code.LPad(8, "01")  // "01010abc"
code.RPad(5, '.')   // "abc.."
```

`IndexOf` clamps `start` to `0 .. text.length`. An empty needle matches at the
normalized start position. `LastIndexOf("")` returns `text.length`; an empty
prefix or suffix matches every text value.

```dq
var text : str = "abcdefabc"
text.IndexOf("abc", 1)  // 6
text.IndexOf("", 100)   // 9
text.LastIndexOf("abc")  // 6
text.StartsWith("abc")   // true
text.EndsWith("abc")     // true
```

The trim and padding methods create a `str`, so they are unavailable on
targets built with dynamic strings disabled. The search methods only return
scalar values and remain available for the supported text views.


## Working with Unicode

Byte length and character count differ for non-ASCII UTF-8:

```dq
use print

func *Main():
    var text : str = "Aé€"
    var scalars : [*]wchar = text.ToWchars()
    PrintLn("bytes={} scalars={} second={}", [text.length, text.wclen, scalars[1]])
endfunc
```

This prints `bytes=6 scalars=3 second=é`. Repeated `.wchar[index]` lookup needs
UTF-8 decoding; converting once to `[*]wchar` is useful for repeated indexed
processing. An isolated `char16` surrogate is a code unit, not a complete Unicode
scalar; UTF-16 conversion checks pairs rather than treating every unit as a
character.

## Conversion and Lifetime Summary

| Destination | Relevant rule |
| --- | --- |
| `str` | Owns/retains its text value; accepts compatible text views. |
| `rostr` | Requires terminated storage, borrows it, and does not retain its owner. |
| `strslice` | Borrows a byte range with an always-known length; conversion resolves an unknown source length first. No terminator is promised. |
| `embstr(N)` | Copies into fixed storage and truncates to fit. |
| Unsized `embstr` | Shares a writable embedded string descriptor; requires suitable existing storage. |
| `^char` | A raw C pointer with no automatic lifetime extension. |

Borrowed text must not outlive its source. Neither a cast nor storing a view in
`anyvalue` makes its owner live longer. Returning a view into a local buffer is
invalid. See [C Interoperability](200-c-interoperability.md) for C-string calls
and [Text Formatting](160-text-formatting.md) for `AddFmt` and placeholders.
