# String Methods

DQ has four main text forms:

| Type | Meaning |
| --- | --- |
| `str` | dynamic heap-managed string |
| `rostr` | read-only borrowed zero-terminated string |
| `strslice` | non-owning string view, including unterminated slices |
| `embstr(n)` | Embedded String: fixed-size zero-terminated storage |

`str` is an owned byte string with an enforced trailing zero. Its `.length`
counts bytes, not Unicode scalar values, and the hidden terminator is not
included in that length. A string may contain arbitrary bytes, including
internal zero bytes, so a `str` is not automatically valid UTF-8.

`str` is copy-on-write. Assigning a string shares storage until one copy is
mutated.

```dq
var a : str = "abc"
var b : str = a
b[0] = 'X'  // a is still "abc"
```

`rostr` provides read-only byte and Unicode access. It borrows storage without
copying, converts directly to C string pointers, and returns `strslice` slices.
`AddFmt` format arguments use `rostr`; mutation sources still accept arbitrary
text views. See [Strings and Characters](../reference/strings-and-characters.md)
for conversions and lifetime requirements.

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

## Unicode Operations

Unicode-oriented operations decode string bytes as UTF-8 and work with `wchar`,
the 32-bit Unicode scalar type.

| Member | Meaning |
| --- | --- |
| `.wclen` | number of decoded Unicode scalar values |
| `.wchar[index]` | Unicode scalar at a scalar index |
| `.wchar[start:end]` | decoded scalar slice as `[*]wchar` |
| `.wcstr[start:end]` | scalar-indexed slice encoded back to `str` |
| `ToWchars()` | decode UTF-8 to `[*]wchar` |
| `ToUtf16()` | decode UTF-8 to zero-terminated `[*]char16` |

```dq
var text : str = "Aé€"
var n : int = text.wclen
var wc : wchar = text.wchar[1]
var chars : [*]wchar = text.wchar[:]
var utf8_part : str = text.wcstr[1:$end]
```

These operations report a runtime encoding error for malformed UTF-8. For
repeated indexed Unicode processing, convert once with `ToWchars()` and index
the resulting array.

`StrFromWchars(chars)` encodes Unicode scalar values as UTF-8. UTF-16
interoperability uses `ToUtf16()` and `StrFromUtf16(...)`; the UTF-16 array
returned by `ToUtf16()` includes a final zero terminator as an ordinary element.

## C String Access

`str` and `embstr` expose `.pchar`, a borrowed `^char` pointer to their
zero-terminated byte storage.

```dq
var p : ^char = s.pchar
```

The pointer is valid only while the source value remains alive and its storage
is not reallocated. Internal zero bytes are preserved in the `str`, but C APIs
using zero-terminated semantics see only the bytes before the first internal
zero.

## `embstr`

`embstr(n)` owns exactly `n` bytes of fixed storage, including the zero
terminator. Its maximum content length is `n - 1` bytes.
Appending and assigning truncate to fit.

```dq
var cs : embstr(6) = "abc"
cs.Append("def")  // stores "abcde"
```

`embstr` supports many of the same mutation methods as `str`, including `Set`,
`Append`, `Add`, `AppendChar`, `Prepend`, `Insert`, `Delete`, `Clear`, and
`AddFmt`. It does not support dynamic capacity operations such as `Reserve` or
`Compact`.

An unsized `embstr` is an alias/view over existing C string storage, commonly
used for parameters. Aliases share descriptor metadata, including the cached
length, so mutations through one alias are observed by the others.
