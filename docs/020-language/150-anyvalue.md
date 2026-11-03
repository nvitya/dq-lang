# anyvalue

`anyvalue` holds one value that can be a number, text, a boolean, or another
supported basic type. It remembers which kind of value it holds. Use it when a
function needs to accept different kinds of input, as formatting functions do.

```dq
use print

func *Main():
    var value : anyvalue = 42
    PrintLn("{}", [value.AsInt(0)])

    value = "hello"
    PrintLn("{}", [value.AsStr("")])
endfunc
```

This prints `42` and then `hello`. The same variable first holds an integer,
then text. The accessor methods, such as `AsInt`, let you read a value in the
form you need.

## Checked Access

Use an `Is...` method to check the kind of value. Use an `As...` method to read
it, supplying a default for values that cannot be converted:

```dq
var value : anyvalue = "hello"
var number : int = value.AsInt(-1) // -1: text is not converted to a number
var text : str = value.AsStr("")  // "hello"
```

The default is also a possible real value. If you need to distinguish a stored
`-1` from a failed conversion, check the kind first:

```dq
if value.IsInt():
    var number : int = value.AsInt(0)
    // use the stored integer
endif
```

Some accessors accept more than one related kind; for example, `AsFloat` can
read an integer as a floating-point value. They do not perform arbitrary
conversions such as parsing text into numbers.

## Array Parameters

A `[]anyvalue` parameter accepts a list containing different kinds of values.
This is how `PrintLn` and `Format` receive their formatting arguments:

```dq
use print

func Log(format : rostr, values : []anyvalue):
    PrintLn(format, values)
endfunc

func *Main():
    Log("{}: {} items, ready={}", ["Shopping", 3, true])
endfunc
```

This prints `Shopping: 3 items, ready=true`. The bracketed list supplies a
temporary array whose elements are converted to `anyvalue`. `Log` can pass that
list to another function directly. Do not keep its slice for use after the
call returns, because the temporary array will no longer exist.

## Null

`anyvalue` can also represent **null**, meaning a missing value. Use `SetNull`
to set it and `IsNull` to test it:

```dq
var reading : anyvalue = 21.5
reading.SetNull()
var missing : bool = reading.IsNull() // true
```

Null is different from zero, `false`, an empty string, and a nil pointer.

## Supported Values

An `anyvalue` can hold these basic values:

| Values | Notes |
| --- | --- |
| Signed and unsigned integers | Integer width and signedness are recorded. |
| Floating-point numbers | `float32`, `float64`, and `float`. |
| Booleans | `true` or `false`. |
| Characters | `char` and `wchar`; `char16` is stored as `wchar`. |
| Text | `str`, `rostr`, `strslice`, and `embstr`. |
| Pointers | Typed pointers and generic `pointer` values. |
| Null | A missing value, set with `SetNull()`. |

Objects, structures, function references, and arbitrary arrays cannot be stored
as general values inside it. The temporary array in a `[]anyvalue` call works
because each supported element is converted separately.

## Methods

Use this table to choose a check or accessor. `def` means the default value
to return when the requested conversion is unavailable.

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

A helper can use these methods to produce a label for different kinds of data:

```dq

func ToLabel(value : anyvalue) -> str:
    if value.IsNull():
        return "<missing>"
    elif value.IsText():
        return value.AsStr("")
    elif value.IsNumber():
        return Format("{}", [value])
    else:
        return "<value>"
    endif
endfunc
```

`Format` is available through the implicit `sys` runtime interface. See
[String Functions](075-string-functions.md#format).

## Setters and Text Views

In addition to assignment, explicit setters choose what kind of value to store:

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

## Lifetime

Storing a managed `str` keeps its text alive. Storing borrowed text or a pointer
does not keep the source alive. For example, a slice into a local buffer must
not be used after that buffer has disappeared, even if stored in an `anyvalue`.

A view returned by `AsText` or `AsStrSlice` is also borrowed. Keep its source
valid for as long as you use the view. Copying and destroying an `anyvalue`
automatically performs the appropriate storage handling for its current kind.

## Character Details

Characters have their own kind: `IsNumber` and `IsInt` return `false` for them.
`AsWChar` can widen a stored `char`; `AsChar` returns its default for a stored
`wchar`.

Formatting with `{}`, `{:?}`, or `{:s}` writes a `char` byte directly and
encodes a `wchar` as UTF-8. The formats `{:d}`, `{:u}`, `{:x}`, and `{:X}`
instead write the character code:

```dq
PrintLn('wchar="{}"', [wchar(0x20AC)]) // wchar="€"
```

An invalid Unicode scalar, including an isolated `char16` surrogate, produces
`�` and sets `DQFMT_ARG_ERROR`.

## Runtime Type Information

Internally, each value records a broad kind, a more specific representation,
and its data. Use the public predicates and accessors rather than depending on
the private memory layout. The methods are backed by the runtime's `AnyVal...`
functions.
