# `strutils`

`strutils` contains small string helpers used throughout the standard packages.

```dq
use strutils
```

## Formatting

```dq
func Format(afmt : rostr, aargs : []anyvalue) -> str
```

`Format` returns a new dynamic string using the runtime formatter.

```dq
var s : str = Format("{} = {:04X}", ["value", 255])
```

See [Text Formatting](../rtl/textfmt.md) for the format language.

## Conversion

```dq
func StrToInt(sv : strslice, defvalue : int64 = 0) -> int64
```

Parses decimal digits and returns `defvalue` if a non-digit is found.

## Searching And Matching

```dq
func StrIndexOf(astr : strslice, aneedle : strslice, astart : int = 0) -> int
func StrStartsWith(astr : strslice, aprefix : strslice) -> bool
func StrEndsWith(astr : strslice, asuffix : strslice) -> bool
```

`StrIndexOf` returns the first index or `-1` when not found.

```dq
if StrStartsWith(uri, "/api/"):
    ...
endif
```

## ASCII Case Conversion

```dq
func StrUpper(astr : strslice) -> str
func StrLower(astr : strslice) -> str
```

These helpers convert ASCII `a..z` and `A..Z`. Other characters are copied as
they are.
