# `anyvalue`

`anyvalue` is the boxed value type used by formatting APIs and other generic
interfaces.

```dq
var v : anyvalue = 123
var s : str = v.AsStr("")
```

Arrays of `anyvalue` are written with normal array literal syntax:

```dq
PrintLn("{}: {}", ["answer", 42])
```

## Stored Kinds

`anyvalue` can hold:

| Kind | Common source values |
| --- | --- |
| null | `v.SetNull()` |
| integer | `int`, `uint`, fixed-width integers |
| byte character | `char` |
| Unicode character | `wchar`, `char16` (widened to `wchar`) |
| boolean | `bool` |
| pointer | `pointer` and pointer values |
| floating point | `float`, `float32`, `float64` |
| text | `str`, `strslice`, `embstr` |

Dynamic strings stored in an `anyvalue` keep the correct reference-counted string
lifetime.

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
| `IsText()` | true for `str`, `strslice`, or `embstr` values |
| `IsStr()` | true only for dynamic `str` |
| `AsStr(def)` | return a dynamic string |
| `AsText(def)` | return a text view-style value |

Example:

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
