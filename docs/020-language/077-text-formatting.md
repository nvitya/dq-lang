# Text Formatting

Text formatting inserts values into a string at placeholders such as `{}`.
The format string supplies the surrounding text; a separate argument list
supplies the values. DQ uses this text formatting syntax in multiple places,
such as printing or creating exception messages.

## A Simple Print Example

The examples use `PrintLn` from `use print`. Unless stated otherwise, they
use these single-letter variables. The type-specific subsections repeat the
relevant declarations so their examples can be read independently.

```dq
var i : int = 42
var n : uint = 255
var f : float = 3.5
var s : rostr = "Amy"
var b : bool = true
var c : char = 'A'
var w : wchar = '€'
var p : ^int = %i
var v : anyvalue
v.SetNull()
```

```dq
PrintLn("Hello, {}!", [s]) // = "Hello, Amy!"
```

`{}` is replaced by the first value in `[s]`. The square brackets belong to
the DQ argument-list expression; they are not printed. `PrintLn` adds a newline
after the result. Multiple placeholders consume values from left to right:

```dq
PrintLn("{} has {} messages.", [s, i]) // = "Amy has 42 messages."
```

## Formatting Arguments: `[]anyvalue`

To print formatted text to the standard output use these functions from the `print` package:

```text
Print(fmt : strslice, args : []anyvalue)
PrintLn(fmt : strslice, args : []anyvalue)
```

`fmt` is the format string. `args` is an **array slice of `anyvalue`**.
An `anyvalue` holds one supported value together with its type, so the formatter can tell
whether to print text, a number, a Boolean, or a character.

### Writing the Argument List

The usual syntax is `[s, i, b]`. Separate the elements with commas.
The compiler knows that the formatting call expects `[]anyvalue`, so it
converts each element to `anyvalue` automatically. The elements can have
different types; no explicit conversions to `anyvalue` are needed.

```dq
PrintLn("name={}, count={}, active={}", [s, i, b]) // = "name=Amy, count=42, active=true"
```

Each element can also be an expression:

```dq
PrintLn("next={}, doubled={}", [i + 1, i * 2]) // = "next=43, doubled=84"
```

Even a single value needs brackets, because the second parameter must be an array,
not one value:

```dq
PrintLn("count={}", [i]) // = "count=42"
```

These calls have the wrong argument syntax and do not compile:

```text
PrintLn("count={}", i) // ERROR: second parameter must be []anyvalue
PrintLn("{} {}", s, i) // ERROR: values belong together in [s, i]
```

### Which Values Can Be Passed?

| Value type | Example argument | Default output with `{}` |
| --- | --- | --- |
| Signed integers | `42`, `-7`, an `int32` variable | Decimal number |
| Unsigned integers | A `uint` or `uint32` variable | Decimal number; use `{:u}` for unsigned decimal formatting |
| Floating-point numbers | `3.5`, a `float32`, `float64`, or `float` variable | Decimal number |
| Boolean values | `true`, `false`, a comparison | `true` or `false` |
| `char` | A `char` variable | One byte written as a character |
| `wchar` | `'€'`, a `wchar` variable | One Unicode character encoded as UTF-8 |
| `char16` | A `char16` variable | One Unicode character; converted to `wchar` |
| Text | A literal, `str`, `rostr`, `strslice`, `embstr(N)`, or unsized `embstr` | The text bytes |
| `^char` | A pointer to zero-terminated text | The pointed-to text |
| Other typed or generic pointers | A `^int` or `pointer` variable | Address in hexadecimal with a leading `^` |
| Null stored in `anyvalue` | A value initialized with `.SetNull()` | `null` |
| An existing `anyvalue` | A previously stored supported value | The representation of its stored value |

The formatter preserves the value's type. A text value containing `"42"` is
still text, and an integer containing `42` is still a number. The placeholder
can choose a representation, but it does not perform arbitrary conversions
between these categories.

```dq
PrintLn("{} / {} / {} / {}", [i, f, b, w]) // = "42 / 3.5 / true / €"
```

Objects, structures, function references, and whole arrays cannot be passed as
values for automatic formatting. Select their fields or elements instead.

```dq
var a : [3]int = [10, 20, 30]

PrintLn("first={}, count={}", [a[0], a.length]) // = "first=10, count=3"
```

See [anyvalue](150-anyvalue.md) for its accessors and lifetime rules.

### Reusing an Argument Array

An existing argument slice can be passed on to further functions. Pass the
slice directly:

```dq
var m : rostr = "{} has {} messages."
var a : []anyvalue = [s, i]

PrintLn(m, a) // = "Amy has 42 messages."
```

There is no need to rebuild the list. `PrintLn(m, [a])` would be a compile
error: `[a]` is `[1][]anyvalue`, which is not compatible with `[]anyvalue`.

### Matching Values to Format String Placeholders

Each placeholder uses the next value. To print the same value multiple times, include
it in the `args` multiple times:

```dq
PrintLn("{} + {} = {}", [i, i, i + i]) // = "42 + 42 = 84"
```

One argument is used for each placeholder. There are no named or numbered argument
references. The current formatter writes `null` for a missing argument and
ignores arguments left over after the last placeholder:

```dq
PrintLn("{} / {}", [s]) // = "Amy / null"
PrintLn("{}", [s, i])   // = "Amy"
```

These mismatches do not generate compile-time or runtime errors.

## Format String Placeholders

Ordinary characters in the format string are copied to the output. Each
`{...}` placeholder inserts the next argument. `{}` uses the default
representation; a colon introduces formatting options.

### Main Placeholder Variants

| Form | Accepted values | Meaning |
| --- | --- | --- |
| [`{}` / `{:?}`](#booleans) | Any supported value | Default representation. Use this for `bool` types |
| [`{:d}`](#signed-integers-d) | Integers, characters | Signed decimal number or character code |
| [`{:u}`](#unsigned-integers-u) | Integers, characters | Unsigned decimal number or character code |
| [`{:x}`](#hexadecimal-integers-x-and-x) | Integers, characters | Lowercase hexadecimal |
| [`{:X}`](#hexadecimal-integers-x-and-x) | Integers, characters | Uppercase hexadecimal |
| [`{:p}`](#pointers-p) | Pointer values | Hexadecimal address with a leading `^` |
| [`{:s}`](#text-s) | Text, characters | Text bytes or a character |
| [`{:f}`](#floating-point-numbers-f) | Floating-point values | Decimal floating-point output |
| [`{{`](#literal-braces-and-escaping) | No argument | Literal `{` |
| [`}}`](#literal-braces-and-escaping) | No argument | Literal `}` |

### Signed Integers: `{:d}`

Use `{:d}` for signed decimal output. `{}` gives the same result for a signed
integer.

```dq
var i : int = 42

PrintLn("{:d}", [i])     // = "42"
PrintLn("[{:6d}]", [i])  // = "[    42]": minimum width
PrintLn("{:06d}", [i])   // = "000042": zero padding
PrintLn("{:+d}", [i])    // = "+42": explicit sign
PrintLn("{:+06d}", [i])  // = "+00042": sign followed by zero padding
PrintLn("[{: d}]", [i])  // = "[ 42]": reserve a sign position
PrintLn("{:06d}", [-i])  // = "-00042": negative sign before the zeros
PrintLn("[{:<6d}]", [i]) // = "[42    ]": left alignment
```

### Unsigned Integers: `{:u}`

Use `{:u}` for unsigned decimal output, including the full range of `uint`.

```dq
var n : uint = 255

PrintLn("{:u}", [n])     // = "255"
PrintLn("[{:6u}]", [n])  // = "[   255]"
PrintLn("{:06u}", [n])   // = "000255"
PrintLn("[{:<6u}]", [n]) // = "[255   ]"
```

### Hexadecimal Integers: `{:x}` and `{:X}`

Use `x` for lowercase digits and `X` for uppercase digits. These formats accept
signed and unsigned integers.

```dq
var n : uint = 255

PrintLn("{:x}", [n])   // = "ff"
PrintLn("{:X}", [n])   // = "FF"
PrintLn("{:08X}", [n]) // = "000000FF": zero-padded hexadecimal
PrintLn("0x{:x}", [n]) // = "0xff": add the prefix as literal text
```

### Floating-Point Numbers: `{:f}`

Use `{:f}` for decimal floating-point output.

```dq
var f : float = 3.5

PrintLn("{:f}", [f])       // = "3.5"
PrintLn("{:.2f}", [f])     // = "3.50": two fractional digits
PrintLn("{:.0f}", [f])     // = "4": rounded to a whole number
PrintLn("[{:8.2f}]", [f])  // = "[    3.50]": width includes the decimal point
PrintLn("{:+.2f}", [f])    // = "+3.50"
PrintLn("{:+08.2f}", [f])  // = "+0003.50"
PrintLn("[{:<8.2f}]", [f]) // = "[3.50    ]"
```

An explicit precision fixes the number of digits after the decimal point,
rounding as needed and retaining trailing zeros. The decimal separator is a
dot. Without explicit precision, `{}` and `{:f}` use up to 15 fractional digits
and remove trailing zeros. Explicit precision is limited to 18 fractional digits.

The `f` placeholder requires a floating-point value; it does not convert an
integer to a float.

### Text: `{:s}`

Use `{:s}` or `{}` for text. The same formats accept the other supported
string types and `^char`. Here, `t` contains ten characters.

```dq
var s : rostr = "Amy"
var t : rostr = "abcdefghij"

PrintLn("{:s}", [s])     // = "Amy"
PrintLn("[{:8s}]", [s])  // = "[     Amy]": right alignment is the default
PrintLn("[{:<8s}]", [s]) // = "[Amy     ]": left-aligned text column
PrintLn("[{:>8s}]", [s]) // = "[     Amy]": explicit right alignment
PrintLn("[{:^8s}]", [s]) // = "[  Amy   ]": centered text
PrintLn("[{:8s}]", [t])  // = "[abcdefghij]": longer text is not truncated
```

The `s` kind is optional: `{:8}` and `{:8s}` both reserve a right-aligned field
at least eight Unicode code points wide. Width never truncates longer text.

### Booleans: `{}`

Use `{}` or `{:?}` for `bool`; there is no separate Boolean format letter.

```dq
var b : bool = true

PrintLn("{}", [b])     // = "true"
PrintLn("{}", [not b]) // = "false"
PrintLn("[{:6}]", [b]) // = "[  true]"
```

### Characters: `{:s}`

Default formatting and `{:s}` write a character. Explicit numeric formats
write its code.

```dq
var c : char  = 'A'
var w : wchar = '€'

PrintLn("{}", [c])     // = "A"
PrintLn("{}", [w])     // = "€"
PrintLn("{:s}", [w])   // = "€"
PrintLn("{:d}", [c])   // = "65"
PrintLn("{:u}", [w])   // = "8364"
PrintLn("{:04X}", [c]) // = "0041"
```

A `char` writes one byte; a `wchar` is encoded as UTF-8. A `char16` value is
converted to `wchar` before formatting. A UTF-16 surrogate pair is not combined
automatically when its two code units are passed as two arguments. Convert
UTF-16 text with `StrFromUtf16()` first. An invalid Unicode character value,
including an isolated surrogate, produces `�`.

### Pointers: `{:p}`

Use `{:p}` or `{}` for an address in hexadecimal with a leading `^`.
`%i` takes the address of `i`. The output below uses an example address;
the actual address varies between runs.

```dq
var i  : int = 42
var p  : ^int = %i
var z  : pointer = nil
var pc : ^char = "abc"

PrintLn("{:p}", [p])           // = "^7FFC1234ABCD": address of i
PrintLn("{}", [p])             // = "^7FFC1234ABCD": same address with default formatting
PrintLn("{:p}", [z])           // = "^0": nil pointer
PrintLn("{}", [pc])            // = "abc": ^char is treated as text
PrintLn("{:p}", [pointer(pc)]) // = "^401234": convert to pointer to print the address
```

### Null Values: `{}`

A null `anyvalue` prints `null`, which differs from the `^0` output of a nil
pointer. Initialize a null value with `.SetNull()`.

```dq
var v : anyvalue
v.SetNull()

PrintLn("{}", [v]) // = "null"
```

### Literal Braces and Escaping

Double each brace in a format string to print one literal brace. Escaped braces
do not consume arguments. In a double-quoted string literal, escape embedded
double quotes as `\"`:

```dq
var s : rostr = "Amy"

PrintLn("{{{}}}", [s])              // = "{Amy}"
PrintLn("{{\"name\":\"{}\"}}", [s]) // = '{"name":"Amy"}': escaped double quotes
PrintLn('{{"name":"{}"}}', [s])     // = '{"name":"Amy"}': single outer quotes avoid internal escaping
PrintLn("{{}}", [])                 // = "{}": just the literal braces
PrintLn("{}")                       // = "{}": no doubling need at non-formatting print
```

## Width, Fill, Sign, Precision

The type-specific examples above show the common combinations. The supported
option order is:

```text
{: [alignment] [sign] [0] [width] [.precision] [kind] }
```

Spaces and square brackets in this diagram are explanatory; do not write them
in the placeholder. Each bracketed part is optional.

| Option | Example | Meaning |
| --- | --- | --- |
| `<` | `{:<8s}` | Left-align within the field |
| `>` | `{:>8s}` | Right-align within the field |
| `^` | `{:^8s}` | Center within the field |
| `+` | `{:+d}` | Show `+` for a non-negative signed number or float |
| `-` | `{:-d}` | Show a sign only for negative values; this is the default |
| Space | `{: d}` | Use a leading space for a non-negative signed number or float |
| `0` | `{:06d}` | Use zeros for right-aligned numeric padding |
| Width | `{:8s}` | Reserve a field at least eight Unicode code points wide |
| Precision | `{:.2f}` | Write two digits after the decimal point |

Width and alignment work with text, characters, numbers, Boolean values, null,
and pointers. DQ defaults to **right alignment for every value type**. `<` pads
on the right, `>` pads on the left, and `^` divides padding between both sides.
If centering needs an odd number of spaces, the extra space goes on the right.

Width is a minimum, not a maximum: longer values are never truncated to fit.
Width counts **Unicode code points**, not UTF-8 bytes. For example, `€` counts
as one code point even though its UTF-8 encoding uses three bytes, and `café`
counts as four:

```dq
var w : wchar = '€'
var s : rostr = "café"

PrintLn("[{:>5s}]", [w]) // = "[    €]": four spaces plus one code point
PrintLn("[{:>6s}]", [s]) // = "[  café]": right alignment
PrintLn("[{:<6s}]", [s]) // = "[café  ]": left alignment
PrintLn("[{:^6s}]", [s]) // = "[ café ]": centered text
```

Combining marks count separately. Width does not measure visual character
groups or terminal display columns.

A negative sign is written automatically. `+` also requests a sign for
non-negative signed numbers and floating-point values. A space sign option
reserves a sign position without writing `+`. The `-` option requests the
default sign behavior; use `<` for left alignment.

Zero padding applies to right-aligned numbers and is written after the sign
or pointer prefix. Left and center alignment use spaces even if `0` is present.
Text and character output also use spaces.

Write alignment first, followed by the sign, zero fill, and width. For example,
`{:^+8d}` centers a signed number, and `{:+08d}` enables zero fill with a sign.
The former zero-before-sign spelling `{:0+8d}` is no longer supported.

## Formatting APIs and Destinations

The same placeholders and `[]anyvalue` argument syntax work with each API:

| API | Destination | Availability |
| --- | --- | --- |
| [`Print(fmt, args)` / `PrintLn(fmt, args)`](#print-and-println) | Standard output | `use print` |
| [`Format(fmt, args)`](#format) | A new `str` | Implicit `sys`; requires dynamic-string support |
| [`.AddFmt(fmt, args)` on `str`](#addfmt) | Appends to a dynamic string | Requires dynamic-string support |
| [`.AddFmt(fmt, args)` on `embstr`](#addfmt) | Appends to a fixed buffer | Works without dynamic-string support |
| [`Exception(fmt, args)`](#exception-messages) | Exception message | Requires exception support |

The `fmt` parameter is `strslice`: literals, `str`, `rostr`, `embstr`, and
existing slices can be passed directly. Even a substring can be used as the format:

```dq
var i : int = 42
var s : rostr = "{} ignored"

PrintLn(s[0:2], [i]) // = "42": only the "{}" slice is parsed
```

### Print and PrintLn

```text
Print(text : strslice)
Print(fmt : strslice, args : []anyvalue)
PrintLn()
PrintLn(text : strslice)
PrintLn(fmt : strslice, args : []anyvalue)
```

`Print` writes without adding a newline. `PrintLn` adds a newline, and
`PrintLn()` writes a blank line.

```dq
Print("Hello, {}", [s]) // = "Hello, Amy": without a newline
PrintLn("!")            // = "!": followed by a newline
PrintLn()               // = "": a blank line
```

The one-argument form writes its text **literally**, without interpreting
placeholders or escaped braces. Supplying the second argument selects the
formatting overload:

```dq
PrintLn("{}")       // = "{}"
PrintLn("{{}}")     // = "{{}}"
PrintLn("{{}}", []) // = "{}"
```

### Format

```text
Format(fmt : strslice, args : []anyvalue) -> str
```

Use `Format()` when the result should be stored or returned rather than
immediately printed. `Format()` is included in the implicit `sys`, which
means it is available without a `use` declaration.

```dq
var s : str = Format("item-{:04d}", [i]) // = "item-0042": contents of s
```

`Format()` is also useful when passing formatted text to a function that does
not support formatting itself. For example, a database connection's
`.Execute(sql)` method takes a complete SQL statement. Here, `dbconn` is an open
database connection and `items` is a table with one integer column:

```dq
var i : int = 42
dbconn.Execute(Format("INSERT INTO items VALUES ({:d})", [i]))
```

### .AddFmt

```text
.AddFmt(fmt : strslice, args : []anyvalue)
```

`.AddFmt()` appends to the existing contents. A sized embedded string starts
empty on declaration, so `.AddFmt()` can be called immediately. `.Clear()`
empties an existing string when starting a new message.

For a dynamic string:

```dq
var s : str = "Order "

s.AddFmt("{}: {} items", [i, n]) // = "Order 42: 255 items": contents of s
```

For an embedded string:

```dq
var s : embstr(32)

s.AddFmt("mask = {:04X}", [n]) // = "mask = 00FF": contents of s
```

A `str` grows as needed. An `embstr(N)` keeps its fixed buffer, including space
for the terminating zero, so formatting can truncate the result:

```dq
var s : embstr(8)

s.AddFmt("{}", ["abcdefghij"]) // = "abcdefg": contents of s; seven bytes plus the terminator fit
```

Repeated `.AddFmt()` calls can assemble a larger message. Reserve space with
`.Reserve()` first when the expected size of a dynamic string is known.

### Exception Messages

The formatted `Exception` constructor takes the same format and argument types:

```text
Exception(fmt : strslice, args : []anyvalue)
```

```dq
raise Exception("invalid count: {}", [-i]) // = "invalid count: -42": exception message
```

See [Exceptions and Error Handling](140-exceptions-and-error-handling.md) for
raising, catching, and inspecting exceptions.

## Argument and Error Rules

Formatting uses the stored value's type. For example, `{:f}` requires a float,
while `{:d}` expects an integer or a character code. Convert the value explicitly
before formatting when another category is needed:

```dq
PrintLn("{:.2f}", [float(i)])     // = "42.00"
PrintLn("{:d}", [StrToInt("42")]) // = "42"
```

A type/format mismatch produces a diagnostic marker instead of throwing a
formatting exception:

```dq
PrintLn("{:f}", [i]) // = "<i-f!>": an integer with a float placeholder
PrintLn("{:d}", [s]) // = "<s-f!>": text with a numeric placeholder
PrintLn("{:d}", [b]) // = "<b-f!>": a Boolean with a numeric placeholder
```

The format language is DQ-specific. C `printf` placeholders such as `%d` have
no substitution meaning here. Use `{}` or a supported `{:...}` placeholder.

### Low-Level Formatter Flags

`Format()`, `.AddFmt()`, and the print helpers do not return a formatting status.
For direct control, `rtl/textformat` exposes `OTextFormatter`, which writes
through an `OTextSink` such as `OTextSinkEmbStr`, `OTextSinkStr`, or
`OTextSinkStdOut`. Its `.flags` records `DQFMT_FMT_ERROR` for malformed braces
or unsupported format options, and `DQFMT_ARG_ERROR` for incompatible value
types or invalid Unicode characters. Each formatting call resets the flags.

With `use rtl/textformat`, let `t` be an `OTextFormatter` connected to the
desired sink:

```dq
t.Format("{:f}", [i]) // = "<i-f!>": sets t.flags to DQFMT_ARG_ERROR
```

The runtime also defines `DQFMT_TRUNCATED` and `DQFMT_SINK_ERROR`, but the current
built-in sinks do not set them when a fixed buffer fills. Keep the destination
capacity in mind when complete output is required.
