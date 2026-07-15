# DQ String Handling v2

## 1. Scope

This specification defines the revised DQ character and string model.

The design goals are:

- simple and predictable byte-string handling;
- zero-cost interoperability with C string input parameters;
- explicit Unicode scalar processing;
- practical UTF-16 conversion for Windows APIs;
- no variable-width internal character storage inside `str`;
- no dedicated `wstr` or UTF-16 string type.

---

## 2. Character Types

```dq
char    // 8-bit value
char16  // 16-bit UTF-16 code unit
wchar   // 32-bit Unicode scalar value
```

### 2.1 `char`

`char` is an arbitrary unsigned 8-bit value.

It can represent:

- an ASCII character;
- one UTF-8 code unit;
- arbitrary binary data.

A `char` does not necessarily represent a complete Unicode character.

### 2.2 `char16`

`char16` is an unsigned 16-bit value intended primarily as a UTF-16 code unit.

A single Unicode scalar may require either one or two `char16` values.

The type exists mainly for Windows API and other UTF-16 interoperability.

### 2.3 `wchar`

`wchar` is an unsigned 32-bit Unicode scalar value.

Valid values are:

```text
U+0000 .. U+D7FF
U+E000 .. U+10FFFF
```

Surrogate values and values above `U+10FFFF` are invalid.

`wchar` is always 32-bit and is independent of the platform C/C++ `wchar_t` type.

---

## 3. String Types

The ordinary DQ string type is:

```dq
str
```

There is no dedicated:

```text
wstr
str16
```

UTF-32 and UTF-16 data are represented using dynamic arrays:

```dq
[*]wchar
[*]char16
```

### 3.1 `str` representation

`str` is an owned dynamic byte string, conceptually similar to:

```dq
[*]char
```

It has an additional enforced trailing-zero invariant:

```dq
s[s.length] == 0
```

The trailing zero is not included in `s.length`.

A `str` may contain arbitrary bytes, including internal zero bytes.

Therefore, `str` is not inherently guaranteed to contain valid UTF-8.

Example:

```dq
var s : str = "abc"
```

Conceptual storage:

```text
'a' 'b' 'c' 0
```

Logical length:

```text
3
```

A string may also contain:

```text
'a' 0 'b' 0
```

with a logical length of `3`; the final zero is the enforced terminator.

---

## 4. Byte Access

Normal indexing operates on bytes:

```dq
var b : char = s[n]
```

This is an O(1) operation.

Normal slicing also uses byte indexes:

```dq
var part : str = s[3:10]
```

The result contains the selected bytes and receives its own enforced trailing zero.

Byte indexing and slicing do not validate UTF-8.

---

## 5. Unicode Scalar Access

Unicode scalar access always interprets the `str` bytes as UTF-8.

Unicode scalar access uses the `wchar` accessor:

```dq
var wc : wchar = s.wchar[n]
```

The index is a Unicode scalar index, not a byte index.

The `wchar` accessor does not use any explicit encoding identifier and cannot be used directly on ISO-8859-1 or other non-UTF-8 byte strings. Such strings must first be converted with an explicit encoding operation such as `s.Decode(encIso8859_1)`.

Unicode scalar slicing uses:

```dq
var part : [*]wchar = s.wchar[3:10]
```

The result type of `s.wchar[a:b]` is always `[*]wchar`, not `str`. The slice contains decoded Unicode scalar values and does not include a zero terminator.

When the selected Unicode scalar range should remain encoded as a `str`, use the `wcstr` accessor:

```dq
var part : str = s.wcstr[3:10]
```

The result is a UTF-8 encoded `str` containing the selected Unicode scalar range. It receives the normal `str` hidden trailing zero.

The name `wcstr` indicates a `str` result selected by `wchar` indexes. It does not introduce a separate wide-string type.

Unicode scalar count uses:

```dq
var count : int = s.wclen
```

This count is the number of decoded Unicode scalar values, not the number of bytes and not the number of user-perceived grapheme clusters.

The name `wclen` is preferred over `charcount` or `charlen` because `char` already names an 8-bit byte-like type in DQ, while `wchar` names the decoded Unicode scalar type being counted.

Approximate complexity:

```text
s[n]             O(1)
s.wclen          O(n)
s.wchar[n]       O(n)
s.wchar[a:b]     O(n) plus result allocation
s.wcstr[a:b]     O(n) plus result allocation
```

For repeated indexed Unicode processing, conversion to `[*]wchar` is recommended.

### 5.1 Invalid UTF-8

Because `str` may contain arbitrary bytes, Unicode-oriented operations can encounter invalid UTF-8.

The following operations must report a runtime encoding error when malformed UTF-8 is encountered:

```dq
s.wchar[n]
s.wchar[a:b]
s.wcstr[a:b]
s.wclen
s.ToWchars()
s.ToUtf16()
```

Malformed input must not be silently replaced or ignored by the default operations.

Optional validation helpers may be provided:

```dq
s.IsUtf8() -> bool
s.EnsureUtf8(fallback_wchar)
```

---

## 6. UTF-32 Conversion

Conversion from `str` to UTF-32:

```dq
var wchars : [*]wchar = s.ToWchars()
```

Signature:

```dq
function str.ToWchars() -> [*]wchar
```

The result contains one `wchar` element for every decoded Unicode scalar.

No zero terminator is added.

Conversion from UTF-32 to `str` uses a global function:

```dq
var s : str = StrFromWchars(wchars)
```

Signature:

```dq
function StrFromWchars(chars : []wchar) -> str
```

The result is UTF-8 encoded and receives the normal `str` hidden trailing zero.

Because DQ has no const input parameters, the input slice type is `[]wchar`.

---

## 7. Explicit Encoding Conversion

Explicit encoding conversion supports UTF-8 and legacy 8-bit encodings such as ISO-8859-1.

`str` itself does not carry an encoding tag. A `str` remains an owned byte string. The encoding is an explicit argument to conversion operations.

The string encoding identifier type is an enum:

```dq
enum NTextEncoding = (encUtf8, encIso8859_1)
```

Additional encodings may be added later by extending this enum.

Conversion from an explicitly encoded `str` to Unicode scalar values:

```dq
var wchars : [*]wchar = s.ToWchars(encIso8859_1)
```

Signature:

```dq
function str.ToWchars(enc : NTextEncoding) -> [*]wchar
```

The selected encoding determines how input bytes are decoded. For ISO-8859-1, bytes `0x00 .. 0xFF` map directly to Unicode scalar values `U+0000 .. U+00FF`.

Conversion from Unicode scalar values to an explicitly encoded `str`:

```dq
var s : str = StrFromWchars(wchars, encIso8859_1)
```

Signature:

```dq
function StrFromWchars(chars : []wchar, enc : NTextEncoding) -> str
```

The result is a byte string encoded with the selected encoding and receives the normal `str` hidden trailing zero.

The result type is still `str`. There is no separate `str8` type; the selected encoding only describes how the output bytes are produced.

If a `wchar` value cannot be represented in the selected encoding, the conversion must report a runtime encoding error.

Fallback conversion is supported with:

```dq
var s : str = StrFromWchars(wchars, encIso8859_1, '?')
```

Signature:

```dq
function StrFromWchars(chars : []wchar, enc : NTextEncoding, fallback_char : char) -> str
```

When a `wchar` value cannot be represented, `fallback_char` is appended to the result instead. The fallback value is an already-encoded byte in the target encoding.

If `fallback_char` is zero, unrepresentable values are removed.

The default overload remains UTF-8:

```dq
StrFromWchars(wchars)                  // UTF-8
StrFromWchars(wchars, enc)             // selected encoding, may raise
StrFromWchars(wchars, enc, fallback)   // selected encoding, with fallback byte
```

Passing `encUtf8` is equivalent to using the default UTF-8 overload.

---

## 8. UTF-16 Conversion

UTF-16 support exists primarily for Windows API interoperability.

Conversion from `str` to UTF-16:

```dq
var s16 : [*]char16 = s.ToUtf16()
```

Signature:

```dq
function str.ToUtf16() -> [*]char16
```

The returned array includes a final zero terminator as an ordinary array element.

Therefore:

```dq
s16[s16.length - 1] == 0
```

The array length is one greater than the number of encoded UTF-16 code units.

This permits direct pointer use with Windows APIs:

```dq
SomeWindowsApi(s16.pdata)
```

When the terminator is not required, the user may remove it:

```dq
s16.Pop()
```

### 8.1 UTF-16 to `str`

Two global overloads are supported.

Owned dynamic-array form:

```dq
function StrFromUtf16(chars : [*]char16) -> str
```

Pointer-and-count form:

```dq
function StrFromUtf16(chars : ^char16, count : int) -> str
```

#### Dynamic-array form

The `[*]char16` overload is intended to consume values returned by `ToUtf16()` and similar zero-terminated UTF-16 buffers.

If the final array element is zero, exactly that final zero is treated as the terminator and is not converted into the resulting `str`.

Internal zero values are preserved as Unicode U+0000 values.

The function does not stop at the first internal zero.

#### Pointer-and-count form

The pointer-and-count overload decodes exactly `count` UTF-16 code units:

```dq
var s = StrFromUtf16(ptr, count)
```

It does not require a trailing zero and does not stop at an internal zero.

This form is suitable for Windows APIs that return a pointer and an explicit UTF-16 code-unit count.

### 8.2 Invalid UTF-16

`StrFromUtf16()` must report a runtime encoding error for malformed UTF-16, including:

- an isolated high surrogate;
- an isolated low surrogate;
- an invalid surrogate sequence.

Malformed UTF-16 must not be silently replaced or ignored by the default conversion functions.

`StrFromUtf16(fallback_wchar)` automatically replaces the malformed UTF-16 with the provided `fallback_wchar` argument. If the `fallback_wchar` is zero, then the invalid character will be removed.

---

## 9. C String Access

Every `str` has an enforced trailing zero, so it can expose its storage without copying:

```dq
var p : ^char = s.pchar
```

`pchar` is a zero-cost borrowed pointer to the first byte of the string.

Because DQ has no const input type, its result type is:

```dq
s.pchar -> ^char
```

The pointer is intended for input use by C-compatible functions.

### 9.1 Internal zeroes

A `str` may contain internal zero bytes.

`s.pchar` does not detect or handle them.

A C function using zero-terminated string semantics sees only the bytes preceding the first internal zero.

Example:

```text
DQ logical string:  'a' 'b' 0 'c' 'd'
C-visible string:   "ab"
```

### 9.2 Pointer lifetime and mutation

The pointer returned by `s.pchar` is valid only while:

- the source `str` remains alive;
- the source `str` storage is not reallocated;
- the source `str` is not modified in a way that changes its storage.

Foreign code should treat the pointer as read-only, even though its DQ type is `^char`.

Writable C buffers should use a dedicated mutable buffer such as:

```dq
cstring(n)
[*]char
```

`cstring` also exposes a `.pchar` property:

```dq
var p : ^char = cs.pchar
```

For `cstring(n)`, `.pchar` points to the first byte of its fixed zero-terminated storage. For an unsized `cstring` alias, `.pchar` points to the aliased storage.

---

## 10. `strview`

`strview` is a borrowed byte-string view.

Unlike `str`, a `strview` is not always zero-terminated at its logical end.

The internal string-view flags should include a zero-termination flag:

```text
ZEROTERM
```

The flag means:

```dq
view.data[view.length] == 0
```

A view covering an entire `str` can normally retain this flag.

A substring view generally cannot:

```dq
var whole : strview = s
var part  : strview = s[3:10]
```

Typical state:

```text
whole: ZEROTERM set
part:  ZEROTERM not set
```

Zero-cost C string access from a `strview` is valid only when its internal `ZEROTERM` flag is set.

Otherwise, conversion to an owned `str` is required before obtaining a C string pointer.

`strview` also exposes a `.pchar` property:

```dq
var p : ^char = view.pchar
```

Its result type is:

```dq
view.pchar -> ^char
```

For now, `strview.pchar` is a raw borrowed pointer to the first byte of the view and does not check whether the view is zero-terminated. Unlike `str.pchar`, it does not guarantee that `view.data[view.length] == 0`.

Callers must use `strview.pchar` with C zero-terminated string APIs only when they already know the view is zero-terminated. Otherwise, convert the view to an owned `str` first, because `str` guarantees the hidden trailing zero.

---

## 11. Recommended Usage

Ordinary text and byte data:

```dq
var s : str
```

Byte-level processing:

```dq
var b : char = s[n]
var part : str = s[3:10]
```

Sequential or occasional Unicode scalar processing:

```dq
var count  : int      = s.wclen
var wc     : wchar    = s.wchar[n]
var wchars : [*]wchar = s.wchar[3:10]
var part   : str      = s.wcstr[3:10]
```

Repeated indexed Unicode processing:

```dq
var wchars : [*]wchar = s.ToWchars()
```

Explicit encoding conversion:

```dq
var wchars : [*]wchar = bytes.ToWchars(encIso8859_1)
var bytes  : str      = StrFromWchars(wchars, encIso8859_1, '?')
```

Windows UTF-16 interoperability:

```dq
var utf16 : [*]char16 = s.ToUtf16()
SomeWindowsApi(utf16.pdata)
```

C string input:

```dq
SomeCFunction(s.pchar)
```

Borrowed C string input from a known zero-terminated view:

```dq
SomeCFunction(view.pchar)
```

Mutable C string storage:

```dq
SomeCFunction(cs.pchar)
```

---

## 12. Final API Summary

```dq
// Character types
char
char16
wchar

// Main string type
str

// Byte access
s[n]
s[begin:end]

// Unicode scalar access
s.wclen
s.wchar[n]
s.wchar[begin:end]
s.wcstr[begin:end]

// UTF-32 conversion
function str.ToWchars() -> [*]wchar
function StrFromWchars(chars : []wchar) -> str

// Explicit encoding conversion
enum NTextEncoding = (encUtf8, encIso8859_1)
function str.ToWchars(enc : NTextEncoding) -> [*]wchar
function StrFromWchars(chars : []wchar, enc : NTextEncoding) -> str
function StrFromWchars(chars : []wchar, enc : NTextEncoding, fallback_char : char) -> str

// UTF-16 conversion
function str.ToUtf16() -> [*]char16
function StrFromUtf16(chars : [*]char16) -> str
function StrFromUtf16(chars : ^char16, count : int) -> str

// C interoperability
s.pchar -> ^char
view.pchar -> ^char
cs.pchar -> ^char
```

No dedicated `wstr` or `str16` type is required.
