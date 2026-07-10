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
| boolean | `bool` |
| pointer | `pointer` and pointer values |
| floating point | `float`, `float32`, `float64` |
| text | `str`, `strview`, `cstring` |

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
| `AsInt(def)` / `AsUint(def)` | convert integer or float, otherwise return default |
| `IsBool()` / `AsBool(def)` | test/read boolean |
| `IsPointer()` / `AsPointer(def)` | test/read pointer |
| `IsFloat()` / `IsFloat32()` / `IsFloat64()` | test floating point kind |
| `AsFloat(def)` / `AsFloat32(def)` / `AsFloat64(def)` | convert float or integer |
| `IsText()` | true for `str`, `strview`, or `cstring` values |
| `IsStr()` | true only for dynamic `str` |
| `AsStr(def)` | return a dynamic string |
| `AsText(def)` | return a text view-style value |

Example:

```dq
function ToLabel(v : anyvalue) -> str:
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

