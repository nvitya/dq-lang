# `strutils`

`strutils` contains small string helpers used throughout the standard packages.

```dq
use strutils
```

## Formatting

```dq
function Format(afmt : strview, aargs : []anyvalue) -> str
```

`Format` returns a new dynamic string using the runtime formatter.

```dq
var s : str = Format("{} = {:04X}", ["value", 255])
```

See [Text Formatting](../rtl/textfmt.md) for the format language.

## Conversion

```dq
function StrToInt(sv : strview, defvalue : int64 = 0) -> int64
```

Parses decimal digits and returns `defvalue` if a non-digit is found.

## Searching And Matching

```dq
function StrIndexOf(astr : strview, aneedle : strview, astart : int = 0) -> int
function StrStartsWith(astr : strview, aprefix : strview) -> bool
function StrEndsWith(astr : strview, asuffix : strview) -> bool
```

`StrIndexOf` returns the first index or `-1` when not found.

```dq
if StrStartsWith(uri, "/api/"):
    ...
endif
```

## ASCII Case Conversion

```dq
function StrUpper(astr : strview) -> str
function StrLower(astr : strview) -> str
```

These helpers convert ASCII `a..z` and `A..Z`. Other characters are copied as
they are.

