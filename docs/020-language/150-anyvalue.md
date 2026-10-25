# anyvalue

`anyvalue` stores a value together with enough type information for a generic
API to inspect it. This allows a single parameter list to contain numbers,
Boolean values, characters, text, pointers, or database null. It does not turn
DQ into a dynamically typed language or box arbitrary objects and structures.

```dq
use print

func *Main():
    var value : anyvalue = 42
    var text : anyvalue = "hello"
    PrintLn("{} {} {}", [value.IsInt(), value.AsInt(0), text.AsStr("")])
endfunc
```

This prints `true 42 hello`.

## Supported Values

An `anyvalue` can represent:

- database null;
- signed and unsigned integers;
- `char` byte characters and `wchar` Unicode scalars (`char16` boxes as `wchar`);
- Boolean values;
- `float32`, `float64`, and `float`;
- typed and generic pointers;
- `str`, `rostr`, `strslice`, and `embstr` text values.

Objects, structures, function references, and arbitrary arrays are not boxed as
general secondary values. An array literal may nevertheless convert element by
element to `[]anyvalue` or another accepted array context.

```dq
PrintLn("{}: {}", ["answer", 42])
```

## Runtime Type Information

The value records a broad kind, a subtype describing width or representation,
and data sufficient for the stored value. The public contract is exposed through
predicates and accessors; programs must not depend on the private bit layout.

## Lifetime

Boxing a managed `str` retains the string storage correctly. Borrowed text and
pointer values do not extend the lifetime of the storage they refer to. The
caller must ensure such storage remains valid for every use of the boxed value.

Copying or destroying an `anyvalue` performs the corresponding handler action
for its stored kind.

## Null

Database null is distinct from numeric zero, Boolean false, an empty string, and
a nil pointer. Use `SetNull()` and `IsNull()`.

## Checked Access

Predicates such as `IsInt`, `IsFloat`, `IsPointer`, and `IsText` inspect the
stored category. `As...` methods return a permitted conversion or the supplied
default value. They do not define unrestricted casts between unrelated kinds.

For APIs that need to distinguish failure from a legitimate default value, test
the kind before calling the accessor.

## Array Parameters

Formatting-style functions normally accept `[]anyvalue`. A contextual array
literal creates the temporary boxed elements for the duration of the call.
Retaining a view of that temporary array beyond the call is invalid.

## Methods

The builtin `anyvalue` type exposes convenience methods backed by the RTL
`AnyVal...` functions.

| Method | Meaning |
| --- | --- |
| `IsNull()` / `SetNull()` | test or set the null value |
| `IsNumber()` | true for integer or floating point values |
| `IsInt()` / `IsSInt()` / `IsUint()` | test integer kind |
| `AsInt(def)` / `AsUint(def)` | convert integer, float, or character code, otherwise return default |
| `IsChar()` / `AsChar(def)` / `SetChar(value)` | test, read, or set a byte character |
| `IsWChar()` / `AsWChar(def)` / `SetWChar(value)` | test, read, or set a Unicode character; setter also accepts `char16` |
| `IsBool()` / `AsBool(def)` | test/read boolean |
| `IsPointer()` / `AsPointer(def)` | test/read pointer |
| `IsFloat()` / `IsFloat32()` / `IsFloat64()` | test floating point kind |
| `AsFloat(def)` / `AsFloat32(def)` / `AsFloat64(def)` | convert float or integer |
| `IsText()` | true for `str`, `rostr`, `strslice`, or `embstr` values |
| `IsStr()` | true only for dynamic `str` |
| `AsStr(def)` | return a dynamic string |
| `AsText(def)` | return a text view-style value |

Example (with `use strutils` for `Format`):

```dq
func ToLabel(v : anyvalue) -> str:
    if v.IsNull():
        return "<null>"
    elif v.IsText():
        return v.AsStr("")
    elif v.IsNumber():
        return Format("{}", [v])
    else:
        return "<value>"
    endif
endfunc
```

The `As...` methods use their default argument when the stored kind cannot be
converted by the runtime helper.

`AsWChar` also widens a stored `char`. `AsChar` returns its default for a stored
`wchar`. Characters are distinct from numbers: `IsNumber` and `IsInt` return false
for them.

Formatting with `{}`, `{:?}`, or `{:s}` writes a `char` byte directly and encodes
a `wchar` as UTF-8. Explicit `{:d}`, `{:u}`, `{:x}`, and `{:X}` formats write the
character code. An invalid Unicode scalar (including an isolated `char16`
surrogate) produces the replacement character `�` and sets `DQFMT_ARG_ERROR`.

```dq
PrintLn('wchar="{}"', [wchar(0x20AC)])  // wchar="€"
```


## Setters and Text Views

In addition to assignment, explicit setters select the stored kind:

| Methods | Stored category |
| --- | --- |
| `SetInt(value)`, `SetUInt(value)` | Signed/unsigned integer. |
| `SetBool(value)`, `SetPointer(value)` | Boolean or raw address. |
| `SetFloat(value)`, `SetFloat32(value)`, `SetFloat64(value)` | Floating-point value. |
| `SetChar(value)`, `SetWChar(value)` | Byte character or Unicode scalar. |
| `SetText(value)`, `SetEmbStr(value)` | Borrowed text representation. |
| `SetStr(value)` | Managed dynamic string. |
| `SetNull()` | Database null. |

`AsStrSlice(default)` is the text-view alias of `AsText(default)`. A borrowed
text view returned by an accessor still depends on the stored/source value's
lifetime. `SetStr`, `AsStr`, and `IsStr` require dynamic-string support.
Method names are case-sensitive; in particular the unsigned setter is
`SetUInt`, while its predicate and accessor are `IsUint` and `AsUint`.
