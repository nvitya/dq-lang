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
| `strview` | read-only borrowed view of text bytes |
| `cstring(N)` | fixed-capacity, mutable, zero-terminated byte storage |
| `cstring` | unsized borrowed bounded C-string descriptor |
| `^char` | raw pointer to zero-terminated byte storage |

`str` stores bytes and always has a hidden trailing zero. `.length`, indexing,
slicing, and capacity count bytes; the terminator is not included. A `str` may
contain internal zeroes and is not necessarily valid UTF-8.

On targets built with dynamic strings disabled, `str` and operations that
produce it are unavailable. Text literals, `strview`, and `cstring(N)` remain
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

## Fixed C Strings

`cstring(N)` owns space for `N` visible bytes plus a terminator. Assignment and
append-style operations truncate to fit rather than growing the value.

```dq
var name : cstring(5) = "abc"
name.Append("def")    // stores "abcde"
```

An unsized `cstring` borrows existing bounded C-string storage and is commonly
used at ABI boundaries. Its lifetime and writability come from the source.

## Raw C String Pointers

`.pchar` returns a borrowed `^char` to compatible zero-terminated storage. It is
valid only while the source is alive and unchanged in a way that can move its
storage. C functions stop at the first zero byte even when the originating
`str` contains later bytes.

A null `^char` is not valid where a C string is required unless the called API
explicitly defines null handling.

## Overlap

String mutation methods preserve the logical source when their source aliases
the destination. This includes appending or inserting a view into the same
string across a possible reallocation.
