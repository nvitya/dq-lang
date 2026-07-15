# String Methods

DQ has three main text forms:

| Type | Meaning |
| --- | --- |
| `str` | dynamic heap-managed string |
| `strview` | non-owning string view |
| `cstring(n)` | fixed-size zero-terminated storage |

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

`str` and `cstring` expose `.pchar`, a borrowed `^char` pointer to their
zero-terminated byte storage.

```dq
var p : ^char = s.pchar
```

The pointer is valid only while the source value remains alive and its storage
is not reallocated. Internal zero bytes are preserved in the `str`, but C APIs
using zero-terminated semantics see only the bytes before the first internal
zero.

## `cstring`

`cstring(n)` owns fixed storage of `n` bytes plus the zero terminator.
Appending and assigning truncate to fit.

```dq
var cs : cstring(5) = "abc"
cs.Append("def")  // stores "abcde"
```

`cstring` supports many of the same mutation methods as `str`, including `Set`,
`Append`, `Add`, `AppendChar`, `Prepend`, `Insert`, `Delete`, `Clear`, and
`AddFmt`. It does not support dynamic capacity operations such as `Reserve` or
`Compact`.

An unsized `cstring` is an alias/view over existing C string storage, commonly
used for parameters.
