# String Functions

Use the reference tables to find string properties, methods, helpers, and
operators, then follow their links to detailed explanations and examples. For
storage, conversions, and lifetime rules, see
[Character and String Types](070-strings-and-characters.md).

## Properties and Methods Listing

In the **Types** column, `*` means all string types: `str`, `rostr`, `strslice`
and `embstr`.

Properties are read-only. A result of `—` means that the method modifies the
string and returns no value. Lengths, positions, and counts are in **bytes**,
except for the explicitly Unicode-oriented members `.wclen` and `.wchar[...]`.

| Property or method | Types | Result | Purpose |
| --- | --- | --- | --- |
| [`.Add(value)`](#add-append-and-appendchar) | `str`, `embstr` | — | Append text or one `char` byte |
| [`.AddFmt(fmt, args)`](#addfmt) | `str`, `embstr` | — | Append formatted values |
| [`.Append(value)`](#add-append-and-appendchar) | `str`, `embstr` | — | Alias for `.Add(value)` |
| [`.AppendChar(ch)`](#add-append-and-appendchar) | `str`, `embstr` | — | Append one `char` byte |
| [`.capacity`](#capacity) | `str` | `int` | Allocated capacity for text bytes |
| [`.Clear()`](#clear) | `str`, `embstr` | — | Remove all contents; retain storage |
| [`.Clear(free_storage)`](#clear) | `str` | — | Optionally release the string's storage |
| [`.Clone()`](#clone) | `str` | `str` | Create an independent copy immediately |
| [`.Compact()`](#setcapacity-and-compact) | `str` | — | Reduce capacity to the current length |
| [`.Contains(needle)`](#contains-startswith-and-endswith) | `*` | `bool` | Test whether text contains a value |
| [`.Delete(index, count = 1)`](#delete) | `str`, `embstr` | — | Remove bytes |
| [`.EndsWith(suffix)`](#contains-startswith-and-endswith) | `*` | `bool` | Test the end of the text |
| [`.IndexOf(needle, start = 0)`](#indexof-and-lastindexof) | `*` | `int` | First matching byte offset, or `-1` |
| [`.Insert(index, value)`](#prepend-and-insert) | `str`, `embstr` | — | Insert text or a byte at a byte position |
| [`.LastIndexOf(needle)`](#indexof-and-lastindexof) | `*` | `int` | Last matching byte offset, or `-1` |
| [`.length`](#length) | `*` | `int` | Current byte length, excluding the terminator |
| [`.LPad(length, fill)`](#lpad-and-rpad) | `*` | `str` | Copy with padding on the left |
| [`.LTrim()` / `.LTrim(set)`](#trim-ltrim-and-rtrim) | `*` | `str` | Copy without matching bytes at the left end |
| [`.maxlength`](#embedded-storage-size) | `embstr` | `int` | Maximum number of text bytes that fit |
| [`.pchar`](#raw-byte-pointer) | `*` | `^char` | Borrow a pointer to the text bytes |
| [`.Pop()`](#pop-and-popfirst) | `str` | `char` | Remove and return the last byte |
| [`.Pop(count)`](#pop-and-popfirst) | `str` | `str` | Remove and return bytes from the end |
| [`.PopFirst()`](#pop-and-popfirst) | `str` | `char` | Remove and return the first byte |
| [`.PopFirst(count)`](#pop-and-popfirst) | `str` | `str` | Remove and return bytes from the beginning |
| [`.Prepend(value)`](#prepend-and-insert) | `str`, `embstr` | — | Insert text or a byte at the beginning |
| [`.refcount`](#reference-count) | `str` | `int` | Number of references sharing the string handler |
| [`.Reserve(capacity)`](#reserve) | `str` | — | Ensure room for at least this many text bytes |
| [`.RPad(length, fill)`](#lpad-and-rpad) | `*` | `str` | Copy with padding on the right |
| [`.RTrim()` / `.RTrim(set)`](#trim-ltrim-and-rtrim) | `*` | `str` | Copy without matching bytes at the right end |
| [`.Set(value)`](#set) | `str`, `embstr` | — | Replace the contents |
| [`.SetCapacity(capacity)`](#setcapacity-and-compact) | `str` | — | Set capacity, shortening the text if necessary |
| [`.SetLength(length, fill)`](#setlength-and-truncate) | `str` | — | Resize, filling new bytes with a `char` |
| [`.StartsWith(prefix)`](#contains-startswith-and-endswith) | `*` | `bool` | Test the beginning of the text |
| [`.storage_size`](#embedded-storage-size) | `embstr` | `int` | Buffer size including space for the terminator |
| [`.ToWchars()`](#towchars) | `*` | `[*]wchar` | Decode the whole UTF-8 string into Unicode characters |
| [`.Trim()` / `.Trim(set)`](#trim-ltrim-and-rtrim) | `*` | `str` | Copy without matching bytes at either end |
| [`.Truncate(length)`](#setlength-and-truncate) | `str` | — | Shorten without growing |
| [`.wchar[index]`](#unicode-indexing-and-slicing) | `*` | `wchar` | Read one Unicode character |
| [`.wchar[start:end]`](#unicode-indexing-and-slicing) | `*` | `[*]wchar` | Decode a range of Unicode characters |
| [`.wclen`](#unicode-character-count) | `*` | `int` | Count Unicode characters in UTF-8 text |

Methods returning `str` require dynamic-string support even when called on an
embedded string or borrowed view. Search methods, embedded-string mutations,
and Unicode decoding to Unicode characters remain available with dynamic strings
disabled. Using `str` always requires dynamic-string support.

## Standalone String Helpers Listing

These functions are available through the implicit `sys` runtime interface;
ordinary modules need no `use` declaration. Text parameters accept the string
types through their normal conversions. Functions returning `str` require
dynamic-string support.

| Function | Result | Purpose |
| --- | --- | --- |
| [`Format(fmt : rostr, args : []anyvalue)`](#format) | `str` | Create formatted text |
| [`StrFromUtf16(chars : []char16)`](#strfromutf16-and-strfromutf16ptr) | `str` | Convert a UTF-16 array to UTF-8 |
| [`StrFromUtf16Fallback(chars : []char16, fallback_wchar : wchar)`](#utf16-fallback-conversion) | `str` | Convert UTF-16, replacing malformed sequences |
| [`StrFromUtf16Ptr(chars : ^char16, count : int)`](#strfromutf16-and-strfromutf16ptr) | `str` | Convert an explicit number of UTF-16 code units |
| [`StrFromUtf16PtrFallback(chars : ^char16, count : int, fallback_wchar : wchar)`](#utf16-fallback-conversion) | `str` | Convert a UTF-16 pointer range with replacement |
| [`StrFromWchars(chars : []wchar)`](#strfromwchars) | `str` | Encode Unicode characters as UTF-8 |
| [`StrLower(text : strslice)`](#strupper-and-strlower) | `str` | Copy with ASCII letters in lowercase |
| [`StrSlice(data : ^char, length : int)`](#strslice) | `strslice` | Borrow a pointer and an explicit byte count |
| [`StrSliceCmp(a : strslice, b : strslice)`](#strslicecmp) | `int` | Compare two byte ranges |
| [`StrToFloat(text : strslice, defvalue : float)`](#strtoint-and-strtofloat) | `float` | Parse a floating-point number |
| [`StrToInt(text : strslice, defvalue : int = 0)`](#strtoint-and-strtofloat) | `int` | Parse a decimal integer |
| [`StrUpper(text : strslice)`](#strupper-and-strlower) | `str` | Copy with ASCII letters in uppercase |

## Text Operators

`+` concatenates compatible text operands into a new `str`. `+=` appends to a
dynamic string. Both require dynamic-string support.

```dq
var greeting : str = "Hello, " + "world"
greeting += "!"           // "Hello, world!"
```

Using `+` repeatedly to assemble text creates temporary strings and can cause
repeated heap allocations and copies. Prefer `.Add()` for plain text and
`.AddFmt()` for formatted values, with `.Reserve()` when the expected size can be
estimated.

## String Comparison

Text values support `==`, `<>`, `<`, `<=`, `>`, and `>=` across different string
types, including embedded strings with or without an explicit capacity.
Strings are compared byte by byte, using unsigned byte values. The first
difference determines the order; when one string is a prefix of the other,
the shorter string comes first.

```dq
var name : str = "alpha"
var limit : rostr = "beta"
var before : bool = name < limit       // true
var prefix : bool = "app" < "apple"    // true
var inside : bool = "a" <= name < "z" // true
var equal : bool = name == "alpha"     // true
```

Comparison is case-sensitive. It does not validate UTF-8, normalize Unicode,
or apply language-specific sorting rules. Known string lengths include internal
zero bytes; raw C strings end at their first zero.

### StrSliceCmp

```text
StrSliceCmp(a : strslice, b : strslice) -> int
```

`StrSliceCmp(a, b)` provides the same bytewise comparison as a function. It
returns a negative value if `a` comes before `b`, zero if equal, and a positive
value if `a` comes after `b`. It compares the entire known byte range without
requiring a terminator.

```dq
var order : int = StrSliceCmp("app", "apple") // negative
var same : int = StrSliceCmp("abc", "abc")   // 0
var a : ^char = "alpha"
var b : ^char = "beta"
var before : bool = StrSliceCmp(a, b) < 0     // true: compares text
```

A raw `^char` operand compares text when paired with a string value or, for
ordering, a text literal. Two raw pointer variables compare their addresses;
use `StrSliceCmp` to compare their text contents explicitly.

## Standalone String Helpers

### Format

```text
Format(fmt : rostr, args : []anyvalue) -> str
```

`Format(fmt, args)` creates a new string using the same placeholders as
`.AddFmt()`. See [Text Formatting](077-text-formatting.md) for the full syntax.

```dq
var label : str = Format("value={:04X}", [255]) // "value=00FF"
```

### StrToInt and StrToFloat

```text
StrToInt(text : strslice, defvalue : int = 0) -> int
StrToFloat(text : strslice, defvalue : float) -> float
```

`StrToInt` accepts decimal digits and a minus sign. `StrToFloat` accepts a dot or
comma decimal separator and an optional `e`/`E` exponent. Both return `defvalue`
for empty input or an unrecognized character; neither trims surrounding spaces.
`StrToFloat` requires an explicit default. These helpers are permissive parsers,
so successful conversion does not imply strict validation of a numeric grammar.

```dq
var count : int = StrToInt("42")               // 42
var fallback : int = StrToInt("invalid", -1)   // -1
var value : float = StrToFloat("1.25e2", 0)    // 125
var comma : float = StrToFloat("1,5", 0)       // 1.5
```

### StrUpper and StrLower

```text
StrUpper(text : strslice) -> str
StrLower(text : strslice) -> str
```

These functions copy the text, converting only ASCII letters. They leave
non-ASCII bytes unchanged and do not modify the source.

```dq
var text : rostr = "Hello, é!"
var upper : str = StrUpper(text) // "HELLO, é!"
var lower : str = StrLower(text) // "hello, é!"
```

### StrSlice

```text
StrSlice(data : ^char, length : int) -> strslice
```

`StrSlice(data, length)` borrows exactly `length` bytes without scanning for a
terminator or copying the text. The memory must remain alive and stable. A
negative length raises a runtime error; a `nil` pointer is valid only with a
zero length.

```dq
var data : [3]char = ['A', char(0), 'B']
var view : strslice = StrSlice(%data[0], data.length)
var bytes : int = view.length // 3: the internal zero does not end the view
```

### StrFromWchars

```text
StrFromWchars(chars : []wchar) -> str
```

`StrFromWchars(chars)` encodes Unicode characters as UTF-8. Invalid Unicode
character values, such as surrogate values, raise a runtime encoding error.

```dq
var chars : [3]wchar = ['A', 'é', '€']
var text : str = StrFromWchars(chars[:]) // "Aé€"
```

### StrFromUtf16 and StrFromUtf16Ptr

```text
StrFromUtf16(chars : []char16) -> str
StrFromUtf16Ptr(chars : ^char16, count : int) -> str
```

`StrFromUtf16(chars)` converts a UTF-16 array slice to UTF-8, ignoring one final
zero code unit if present. Internal zeros are preserved. `StrFromUtf16Ptr(chars,
count)` converts exactly `count` code units, including any zeros in that range.
Surrogate pairs represent one Unicode character; malformed UTF-16 raises a
runtime encoding error. The pointer form returns empty text for `nil` or a
non-positive count.

```dq
var units : [4]char16 = [char16(65), char16(0), char16(66), char16(0)]
var text : str = StrFromUtf16(units[:])          // "A\0B": 3 bytes
var prefix : str = StrFromUtf16Ptr(%units[0], 1) // "A"
```

### UTF16 Fallback Conversion

```text
StrFromUtf16Fallback(chars : []char16, fallback_wchar : wchar) -> str
StrFromUtf16PtrFallback(chars : ^char16, count : int, fallback_wchar : wchar) -> str
```

`StrFromUtf16Fallback` and `StrFromUtf16PtrFallback` use the same input bounds
as their strict counterparts but replace malformed UTF-16 sequences with
`fallback_wchar`. Passing `wchar(0)` as the fallback skips malformed sequences.

```dq
var units : [3]char16 = [char16(0xD800), char16(65), char16(0)]
var replaced : str = StrFromUtf16Fallback(units[:], '?') // "?A"
var skipped : str = StrFromUtf16PtrFallback(%units[0], 2, wchar(0)) // "A"
```

## String Properties

### Length

`.length` counts text bytes, including internal zero bytes when the source has a
known length. It excludes the trailing terminator. `str` and `strslice` store
their lengths; embedded strings and `rostr` may need to scan for the first zero
byte when the length is unknown.

```dq
var text : str = "Aé€"
var bytes : int = text.length  // 6 bytes
var empty : str
var zero : int = empty.length // 0, even without an allocated handler
```

### Capacity

`.capacity` is the number of text bytes a `str` can hold before it needs more
storage. It excludes the extra terminating zero byte and can exceed `.length`.
An empty string with no allocated handler has capacity zero.

```dq
var text : str = "abc"
text.Reserve(100)
var length : int = text.length     // 3
var has_room : bool = text.capacity >= 100 // true
```

### Embedded Storage Size

For `embstr(N)`, `.storage_size` is `N` and `.maxlength` is `N - 1`.
An unsized `embstr` reports these values for the buffer it references.

```dq
var buffer : embstr(8) = "abc"
var size : int = buffer.storage_size // 8
var limit : int = buffer.maxlength   // 7
buffer.Add("defghi")                 // buffer is "abcdefg": truncated to fit
```

### Reference Count

`.refcount` reports how many references share a dynamic string handler. Assigning
one `str` to another shares the handler; modifying one value detaches its storage
when necessary. A string with no handler has reference count zero. Temporary
values and function arguments can also retain references.

```dq
var a : str = "abc"
var b : str = a
var shared : int = a.refcount // 2
b[0] = 'X'                   // a is "abc"; b is "Xbc"
var unique : int = a.refcount // 1
```

### Raw Byte Pointer

`.pchar` borrows a pointer to the first text byte without copying or retaining
storage. `str`, `rostr`, and embedded strings provide terminated storage; a
`strslice` pointer is **not guaranteed to be zero-terminated**. Copy a slice to
`str` or `embstr(N)` before passing it to a C API that expects a C string.

```dq
var text : str = "hello"
var pointer : ^char = text.pchar
var first : char = pointer^ // 'h'

var part : strslice = text[:2] // "he", followed by more source bytes
var terminated : str = part
var c_text : ^char = terminated.pchar // points to "he" plus a zero terminator
```

Keep the source alive and its storage stable while using the pointer. Obtain a
new pointer after a mutation that can move the text. For `str`, `.pchar` returns
terminated empty text even when the internal handler pointer is `nil`. C string
functions see only the prefix before the first zero byte. See
[C Interoperability](200-c-interoperability.md#strings-buffers-and-borrowing).

## String Methods

These methods modify `str` or embedded string storage. Dynamic strings grow as
needed and preserve other values that share their storage. Embedded strings
keep their fixed capacity and truncate inserted or appended text to fit. An
unsized `embstr` modifies the original buffer through its shared descriptor.

Text-source arguments accept `str`, `rostr`, `strslice`, `embstr(N)`, unsized
`embstr`, or `^char`. `.Set`, `.Add`, `.Append`, `.Prepend`, and `.Insert` also accept
one `char` byte. A `wchar` variable must be encoded as UTF-8 to append its text;
it is not a single-byte source.

### .Set

```text
.Set(value : strslice)
.Set(value : char)
```

`.Set(value)` replaces the contents. For text values, assignment provides the
same replacement syntax. Embedded destinations truncate the source to fit.

```dq
var text : str = "old"
text.Set("new")           // "new"
text.Set('!')             // "!"
var buffer : embstr(8) = ""
buffer.Set("abcdefghij")  // "abcdefg"
```

### .Add, .Append, and .AppendChar

```text
.Add(value : strslice)
.Add(value : char)
.Append(value : strslice)
.Append(value : char)
.AppendChar(ch : char)
```

`.Add(value)` and `.Append(value)` append text or a `char` byte. `.AppendChar(ch)`
accepts only a `char`. These methods modify the string and return no value.

```dq
var text : str = "abc"
text.Add("def")           // "abcdef"
text.Append('!')          // "abcdef!"
text.AppendChar('?')      // "abcdef!?"
```

### .AddFmt

```text
.AddFmt(fmt : rostr, args : []anyvalue)
```

`.AddFmt(fmt, args)` appends text containing formatted
values. Embedded destinations truncate the output to fit. Placeholder syntax
and supported value types are explained in [Text Formatting](077-text-formatting.md).

```dq
var text : str = "value="
text.AddFmt("{:04X}", [255]) // "value=00FF"
var buffer : embstr(16) = ""
buffer.AddFmt("{} items", [3]) // "3 items"
```

### .Prepend and .Insert

```text
.Prepend(value : strslice)
.Prepend(value : char)
.Insert(index : int, value : strslice)
.Insert(index : int, value : char)
```

`.Prepend(value)` inserts at the beginning. `.Insert(index, value)` inserts before
the specified byte position. The insertion position is clamped to
`0 .. text.length`; `$end` inserts at the end.

```dq
var text : str = "bc"
text.Prepend('a')         // "abc"
text.Insert(1, "--")      // "a--bc"
text.Insert($end, '!')    // "a--bc!"
text.Insert(100, '?')     // "a--bc!?": position clamped to the end
```

**Performance:** `.Insert()` shifts the bytes after the insertion position to
make room. Inserting near the beginning of a large string can take significant
time. `.Prepend()` moves all existing text bytes.

### .Delete

```text
.Delete(index : int, count : int = 1)
```

`.Delete(index, count = 1)` removes up to `count` bytes starting at `index`.
The position is clamped to `0 .. text.length`, and the count is limited to the
remaining bytes. A non-positive count or a position at the end removes nothing.

```dq
var text : str = "abcdef"
text.Delete(1)            // "acdef"
text.Delete(2, 100)       // "ac": removes the entire remaining suffix
text.Delete(100)          // "ac": unchanged
```

**Performance:** `.Delete()` shifts the bytes after the removed range to close
the gap. Deleting near the beginning of a large string can take significant
time because most of the remaining text must move.

### .Clear

```text
.Clear(free_storage : bool = false)  // str
.Clear()                           // embstr
```

`.Clear()` makes the string empty and retains its storage for reuse. For `str`,
this is equivalent to `.Clear(false)`. `.Clear(true)` releases that value's
reference to the dynamic storage and leaves its length and capacity at zero.
Other strings sharing the old storage retain their text. Embedded strings
support only the no-argument form.

```dq
var text : str = "abc"
text.Reserve(100)
text.Clear()              // empty; capacity is retained
text.Add("reused")
text.Clear(true)          // empty; capacity is 0
var buffer : embstr(16) = "abc"
buffer.Clear()            // empty; the 16-byte buffer remains
```

### .SetLength and .Truncate

```text
.SetLength(length : uint, fill : char)
.Truncate(length : uint)
```

`.SetLength(length, fill)` sets the byte length, using the supplied `char` for
new bytes when growing. Shortening discards the suffix. `.Truncate(length)` only
shortens: a length greater than the current length leaves the text unchanged.
Lengths must be non-negative.

```dq
var text : str = "abc"
text.SetLength(5, '.')    // "abc.."
text.SetLength(4, ' ')    // "abc."
text.Truncate(2)          // "ab"
text.Truncate(100)        // "ab": does not grow
```

### .Pop and .PopFirst

```text
.Pop() -> char
.Pop(count : int) -> str
.PopFirst() -> char
.PopFirst(count : int) -> str
```

`.Pop()` removes and returns the last `char`; `.PopFirst()` removes and returns
the first. These no-argument forms raise a runtime error on an empty string.

With a count argument, both methods return a `str` containing the removed bytes.
Counts greater than the length remove everything. A non-positive count or an
empty string returns empty text.

```dq
var text : str = "abcdef"
var tail : str = text.Pop(2)        // "ef"; text is "abcd"
var head : str = text.PopFirst(2)   // "ab"; text is "cd"
var last : char = text.Pop()        // 'd'; text is "c"
var first : char = text.PopFirst()  // 'c'; text is empty
var empty : str = text.Pop(10)      // "": count form accepts an empty string
```

**Performance:** Both forms of `.PopFirst()` shift all remaining text bytes
to the beginning of the string. This can take significant time for large
strings, especially when repeatedly removing one byte at a time.

### Overlapping Source and Destination

Dynamic string mutation preserves the logical source when it aliases the
destination, including a view into the same string across a reallocation.

```dq
var text : str = "abcd"
text.Insert(2, text[1:3]) // "abbccd": inserts the original "bc"
text.Add(text)           // "abbccdabbccd"
```

## Managing Dynamic Storage

### .Reserve

```text
.Reserve(capacity : uint)
```

`.Reserve(capacity)` ensures space for at least the specified number of text
bytes without changing the length or contents. It does not reduce existing
capacity. Reserving an estimated final size avoids repeated reallocations and
copying while assembling text. The runtime doubles capacity as needed when a
growing string exceeds its current allocation.

```dq
var text : str
text.Reserve(32)
text.Add("Hello, ")
text.AddFmt("{}!", ["world"]) // "Hello, world!"
```

### .SetCapacity and .Compact

```text
.SetCapacity(capacity : uint)
.Compact()
```

`.SetCapacity(capacity)` sets the capacity to the requested number of text bytes.
Reducing it below `.length` also truncates the contents. `.SetCapacity(0)` releases
the storage. `.Compact()` sets capacity to the current length. Call these after
building text when reducing retained storage is useful.

```dq
var text : str = "abcdef"
text.SetCapacity(4)       // "abcd"; capacity is 4
text.Reserve(100)
text.Compact()            // "abcd"; capacity is 4 again
```

### .Clone

```text
.Clone() -> str
```

`.Clone()` returns an independent copy immediately. Ordinary `str` assignment
shares storage and detaches on mutation; use `.Clone()` when the copy should
already have its own storage before either value changes.

```dq
var original : str = "abc"
var copy : str = original.Clone()
copy[0] = 'X'             // copy is "Xbc"; original is "abc"
```

## Searching, Trimming, and Padding

These methods work on every string type. They do not modify the original string.
Needles, prefixes, suffixes, trim sets, and padding accept text or a `char` byte.
Searches compare bytes exactly, with case sensitivity and no Unicode
normalization. Trimming and padding return a new `str`; assign the result to
keep it.

### .IndexOf and .LastIndexOf

```text
.IndexOf(needle : strslice, start : int = 0) -> int
.IndexOf(needle : char, start : int = 0) -> int
.LastIndexOf(needle : strslice) -> int
.LastIndexOf(needle : char) -> int
```

`.IndexOf(needle, start = 0)` returns the first matching byte offset at or after
`start`. `.LastIndexOf(needle)` searches for the final occurrence in the entire
text. Both return `-1` when no match exists.

`.IndexOf` clamps `start` to `0 .. text.length`. An empty needle matches at the
normalized start position; `.LastIndexOf("")` returns `text.length`.

```dq
var text : rostr = "abcdefabc"
var first : int = text.IndexOf("abc")      // 0
var next : int = text.IndexOf("abc", 1)    // 6
var last : int = text.LastIndexOf("abc")   // 6
var byte : int = text.IndexOf('d')         // 3
var missing : int = text.IndexOf("xyz")    // -1
var end : int = text.IndexOf("", 100)      // 9
```

### .Contains, .StartsWith, and .EndsWith

```text
.Contains(needle : strslice) -> bool
.Contains(needle : char) -> bool
.StartsWith(prefix : strslice) -> bool
.StartsWith(prefix : char) -> bool
.EndsWith(suffix : strslice) -> bool
.EndsWith(suffix : char) -> bool
```

`.Contains(needle)` tests for a match anywhere. `.StartsWith(prefix)` and
`.EndsWith(suffix)` test only the corresponding end. An empty needle, prefix,
or suffix matches every text value, including empty text.

```dq
var path : embstr(32) = "docs/reference.md"
var found : bool = path.Contains("reference") // true
var prefix : bool = path.StartsWith("docs/")  // true
var suffix : bool = path.EndsWith(".md")      // true
var case_match : bool = path.EndsWith(".MD")  // false
```

### .Trim, .LTrim, and .RTrim

```text
.Trim() -> str
.Trim(set : strslice) -> str
.Trim(set : char) -> str
.LTrim() -> str
.LTrim(set : strslice) -> str
.LTrim(set : char) -> str
.RTrim() -> str
.RTrim(set : strslice) -> str
.RTrim(set : char) -> str
```

`.Trim()` removes ASCII whitespace from both ends; `.LTrim()` removes it only
from the left, and `.RTrim()` only from the right. Default whitespace is space,
tab, line feed, carriage return, vertical tab, and form feed.

An explicit argument is a **set of bytes**, not a substring pattern. Bytes in
that set are removed repeatedly from the selected ends. Interior bytes remain.

```dq
var text : strslice = "  abc  "
var both : str = text.Trim()       // "abc"
var left : str = text.LTrim()      // "abc  "
var right : str = text.RTrim()     // "  abc"
var marked : rostr = "-=-abc-=-"
var name : str = marked.Trim("-=") // "abc"
var dashes : str = "---abc---"
dashes = dashes.Trim('-')          // "abc": assign to replace the original
```

### .LPad and .RPad

```text
.LPad(length : int, fill : strslice) -> str
.LPad(length : int, fill : char) -> str
.RPad(length : int, fill : strslice) -> str
.RPad(length : int, fill : char) -> str
```

`.LPad(length, fill)` and `.RPad(length, fill)` return text padded to the requested
byte length. They repeat the fill value and truncate the last repetition as
needed. If the requested length is no greater than the source length, the
result has the original contents. An empty fill value raises a runtime error.
Padding operates on bytes and can split a multibyte UTF-8 fill value.

```dq
var code : rostr = "abc"
var left : str = code.LPad(8, "01") // "01010abc"
var right : str = code.RPad(5, '.') // "abc.."
var same : str = code.LPad(2, ' ')  // "abc": no truncation
```

## Byte Indexing and Slicing

`text[index]` reads one `char` byte from any string type. `str` and embedded
strings also permit replacing a byte; `rostr` and `strslice` are read-only.
Indices are zero-based. An index outside the existing bytes raises a runtime
range error. These operations do not decode or validate UTF-8.

`text[start:end]` borrows a half-open byte range as `strslice`, while
`text[start::end]` includes the end byte. Slicing is supported on `str`, `rostr`,
and `strslice`. For embedded strings, first create a `rostr` or `strslice` view.
Slice bounds are clamped to `[0, length]`; reversed normalized bounds produce
an empty slice.

`$last` is the last existing byte index and `$end` is the one-past-end position
for the string being indexed or sliced. Omitting a slice bound selects the
beginning or end. Negative slice bounds are clamped, rather than counted from
the end; use `$end - count` to select a suffix.

```dq
var text : str = "abcdef"
var first : char = text[0]             // 'a'
text[0] = 'A'                          // text is "Abcdef"
var last : char = text[$last]          // 'f'
var part : strslice = text[1:3]        // "bc": end byte excluded
var inclusive : strslice = text[1::3]  // "bcd": end byte included
var head : strslice = text[:2]         // "Ab"
var tail : strslice = text[$end - 2:]  // "ef"
var all : strslice = text[-5:100]      // "Abcdef": bounds clamped
var empty : strslice = text[4:2]       // ""
```

Slices borrow the source bytes. Keep the owner alive and its storage and
contents stable while using them. Assign a slice to `str` to make an owned copy.

## Unicode Operations

The following members decode UTF-8 and report a runtime encoding error for
malformed input. A displayed symbol can combine multiple Unicode characters,
for example a letter and a combining accent.
`.wclen` and `.wchar[...]` are available on every string type, including
`embstr(N)` and unsized `embstr`.

### Unicode Character Count

`.wclen` counts decoded Unicode characters. Unlike `.length`, it requires
UTF-8 decoding rather than reading a stored byte count.

```dq
var text : embstr(16) = "Aé€"
var bytes : int = text.length // 6
var characters : int = text.wclen // 3
```

### Unicode Indexing and Slicing

`.wchar[index]` reads one Unicode character as `wchar`. `.wchar[start:end]`
returns a decoded range as `[*]wchar`; the `start::end` form includes the final
character. In this context, `$last` and `$end` refer to Unicode character
positions. Slice bounds are clamped; an invalid character index raises a
runtime range error.

```dq
var text : embstr(16) = "Aé€"
var second : wchar = text.wchar[1]          // 'é'
var chars : [*]wchar = text.wchar[1:$end]   // ['é', '€']
var part : str = StrFromWchars(chars[:])   // "é€": encode the selected characters
```

### .ToWchars

```text
.ToWchars() -> [*]wchar
```

`.ToWchars()` decodes the whole string into `[*]wchar`. It works on every
string type, including `strslice`, `embstr(N)`, and unsized `embstr`. For repeated
indexed Unicode processing, convert once and index the resulting array to avoid
decoding the UTF-8 prefix for every lookup.

```dq
var text : embstr(16) = "Aé€"
var chars : [*]wchar = text.ToWchars()
var second : wchar = chars[1] // 'é'
var view : strslice = text
var from_view : [*]wchar = view.ToWchars() // ['A', 'é', '€']
```
