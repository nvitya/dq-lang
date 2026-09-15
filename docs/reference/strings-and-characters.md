# Strings and Characters

This page defines DQ text representations and ownership. Common methods are
listed in the [String Methods guide](../rtl/strings.md).

## Character Types

| Type | Meaning |
| --- | --- |
| `char` | unsigned 8-bit byte or UTF-8 code unit |
| `char16` | unsigned 16-bit UTF-16 code unit |
| `wchar` | unsigned 32-bit Unicode scalar value |

These are distinct types, not integer aliases. `Ord` converts a character to an
integer; explicit or checked conversion creates a character from an integer.

## Text Types

| Type | Ownership and representation |
| --- | --- |
| `str` | owned, dynamic, reference-counted copy-on-write byte string |
| `rostr` | read-only borrowed, zero-terminated byte string |
| `strslice` | read-only borrowed view of text bytes; termination is not guaranteed |
| `embstr(N)` | Embedded String: fixed-capacity, mutable, zero-terminated byte storage |
| `embstr` | unsized borrowed reference to a shared Embedded String descriptor |
| `^char` | raw pointer to zero-terminated byte storage |

`str` stores bytes and always has a hidden trailing zero. `.length`, indexing,
slicing, and capacity count bytes; the terminator is not included. A `str` may
contain internal zeroes and is not necessarily valid UTF-8.

On targets built with dynamic strings disabled, `str` and operations that
produce it are unavailable. Text literals, `rostr`, `strslice`, and `embstr(N)` remain
available for non-owning or bounded text processing.

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
A `rostr` converts to `strslice`, owned `str`, or fixed-capacity `embstr(N)`;
it cannot become an unsized writable `embstr` alias.

The descriptor matches `SDqRoStrInfo`: a pointer and a `uint32` byte length,
with bit 31 marking an unknown length. Its naturally aligned storage is 8 bytes
on 32-bit targets and 16 bytes on 64-bit targets. Converting a `^char` builds
this descriptor without scanning or allocating; the first length-dependent
operation scans and caches the length. Default values and nil-derived values
expose a non-null pointer to a static zero byte, representing an empty string.

Known lengths preserve embedded zero bytes. DQ text operations can process
those bytes, while C functions see only the prefix before the first zero.
`rostr` converts automatically to declared `^char` parameters and to `^char`
in C variadic positions, so `printf("%s", borrowed)` passes a pointer.
Raw pointers retain their existing mutability; callers must respect the
borrowed storage's read-only contract.

Standard-library paths, names, commands, messages, and format strings use
`rostr`. Parser input, generic text algorithms, output chunks, and byte payloads
continue to use `strslice` so they accept slices without allocating.

## Byte Indexing and Slicing

`text[index]` reads or writes one `char` byte. `text[start:end]` uses a half-open
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

`embstr(N)` owns exactly `N` bytes of storage. Its final byte is reserved for
the zero terminator, so it stores at most `N - 1` visible bytes. Assignment and
append-style operations truncate to fit rather than growing the value.

```dq
var name : embstr(6) = "abc"
name.Append("def")    // stores "abcde"
```

An unsized `embstr` borrows existing bounded C-string storage through a shared
descriptor. Assigning or passing it preserves that descriptor reference, so
length updates made through one alias are visible to the others. Its lifetime
and writability come from the source.

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
