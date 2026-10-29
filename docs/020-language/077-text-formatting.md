# Text Formatting

Text formatting inserts values into a string at placeholders such as `{}`.
The format string supplies the surrounding text; a separate argument list
supplies the values. DQ uses the same format language for printing, creating
strings, appending text, and exception messages.

## A First Example

```dq
use print

func *Main():
    PrintLn("Hello!")
    PrintLn("Hello, {}!", ["Ada"])
    PrintLn("You have {} messages.", [3])
endfunc
```

Output:

```text
Hello!
Hello, Ada!
You have 3 messages.
```

In `PrintLn("Hello, {}!", ["Ada"])`, `{}` is replaced by the first value in
`["Ada"]`. The square brackets belong to the DQ argument-list expression;
they are not printed. `PrintLn` adds a newline after the result.

Several placeholders consume values from left to right:

```dq
var name : rostr = "Ada"
var count : int = 3
PrintLn("{} has {} messages.", [name, count]) // Ada has 3 messages.
```

## Formatting Arguments: `[]anyvalue`

To print formatted text to the standard output use these variants:

```text
Print(fmt : rostr, args : []anyvalue)
PrintLn(fmt : rostr, args : []anyvalue)
```

`fmt` is the format string. `args` is an **array slice of `anyvalue`**.
An `anyvalue` holds one supported value together with its type, so the formatter can tell
whether to print text, a number, a Boolean, or a character.

### Writing the Argument List

The usual syntax is `[value1, value2, ...]`. Separate elements with commas.
The compiler knows that the formatting call expects `[]anyvalue`, so it
converts each element to `anyvalue` automatically. The elements can have
different types; no explicit conversions to `anyvalue` are needed.

```dq
var name : rostr = "Ada"
var count : int = 3
var active : bool = true
PrintLn("name={}, count={}, active={}", [name, count, active])
// name=Ada, count=3, active=true
```

Each element can also be an expression:

```dq
var count : int = 3
PrintLn("next={}, doubled={}", [count + 1, count * 2]) // next=4, doubled=6
```

Even a single value needs brackets, because the second parameter is an array,
not one value:

```dq
var count : int = 3
PrintLn("count={}", [count]) // count=3
```

These calls have the wrong argument syntax and do not compile:

```text
PrintLn("count={}", count)     // ERROR: second parameter must be []anyvalue
PrintLn("{} {}", name, count)  // ERROR: values belong together in [name, count]
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
var price : float = 3.5
PrintLn("{} / {} / {} / {}", [42, price, true, '€']) // 42 / 3.5 / true / €
```

Objects, structures, function references, and whole arrays cannot be passed as
values for automatic formatting. Select their fields or elements instead:

```dq
var values : [3]int = [10, 20, 30]
PrintLn("first={}, count={}", [values[0], values.length]) // first=10, count=3
```

See [anyvalue](150-anyvalue.md) for its accessors and lifetime rules.

### Reusing an Argument Array

The argument array can be passed on to further functions like the following
example shows:

```dq
use print

func Log(fmt : rostr, args : []anyvalue):
    Print("[info] ")
    PrintLn(fmt, args)
endfunc

func *Main():
    var name : rostr = "Ada"
    var count : int = 3
    Log("{} has {} messages.", [name, count])
    Log("Next count: {}", [count + 1])
endfunc
```

Output:

```text
[info] Ada has 3 messages.
[info] Next count: 4
```

Inside `Log`, `args` is already a slice, so pass it directly as `PrintLn(fmt, args)`.
There is no need to rebuild the list.

Inside the `Log()` the line `PrintLn(fmt, [args])` would be wrong (compile error). The args is already the expected `[]anyvalue`,
`[args]` is `[][]anyvalue` which is not compatible with the formatting functions.

### Matching Values to Format String Placeholders

Each placeholder uses the next value. To print the same value twice, include
it twice. Escaped braces do not consume values.

```dq
var count : int = 3
PrintLn("{} + {} = {}", [count, count, count + count]) // 3 + 3 = 6
```

Use one argument for each placeholder. There are no named or numbered argument
references. The current formatter writes `null` for a missing argument and
ignores arguments left over after the last placeholder:

```dq
PrintLn("{} / {}", ["Ada"])      // Ada / null
PrintLn("{}", ["Ada", 3])        // Ada
```

These mismatches are not compile-time argument-count errors: the function
receives one slice, and the format string is interpreted at runtime.

## Format String Placeholders

Ordinary characters in the format string are copied to the output. `{...}` in the format strings
adds the next argument formatted to the output. `{}` uses the default representation, a colon introduces formatting options.

| Form | Accepted values | Meaning |
| --- | --- | --- |
| `{}` / `{:?}` | Any supported value | Default representation |
| `{:d}` | Integers, characters | Signed decimal number or character code |
| `{:u}` | Integers, characters | Unsigned decimal number or character code |
| `{:x}` | Integers, characters | Lowercase hexadecimal |
| `{:X}` | Integers, characters | Uppercase hexadecimal |
| `{:p}` | Pointer values | Hexadecimal address with a leading `^` |
| `{:s}` | Text, characters | Text bytes or a character |
| `{:f}` | Floating-point values | Decimal floating-point output |
| `{{` | No argument | Literal `{` |
| `}}` | No argument | Literal `}` |

For `bool` types just use `{}`.

### Decimal and Hexadecimal Numbers

```dq
var value : int = 255
var count : uint = 42
PrintLn("decimal={}, hex={:x}, HEX={:X}", [value, value, value])
// decimal=255, hex=ff, HEX=FF
PrintLn("unsigned={:u}", [count]) // unsigned=42
```

### Characters and Character Codes

Default formatting writes a character. Explicit numeric formats write its
code. A `char` writes one byte; a `wchar` is encoded as UTF-8.

```dq
var letter : char = 'A'
PrintLn("letter={}, code={:d}, hex={:X}", [letter, letter, letter])
// letter=A, code=65, hex=41
PrintLn("symbol={}, code={:d}", ['€', '€']) // symbol=€, code=8364
```

A `char16` value is converted to `wchar` before formatting. A UTF-16 surrogate
pair is not combined automatically when its two code units are passed as two
arguments. Convert UTF-16 text with `StrFromUtf16()` first. An invalid Unicode
character value, including an isolated surrogate, produces `�` and sets the
low-level `DQFMT_ARG_ERROR` flag.

### Pointers and Null

A nil pointer and a null `anyvalue` are different values:

```dq
var address : pointer = nil
var missing : anyvalue
missing.SetNull()
PrintLn("address={:p}, missing={}", [address, missing]) // address=^0, missing=null
```

A `^char` is treated as text. To format its address instead, first convert it
to `pointer` or explicitly store it with `anyvalue.SetPointer()`.

### Literal Braces

Double each brace in a format string to print one literal brace:

```dq
PrintLn("{{name: {}}}", ["Ada"]) // {name: Ada}
PrintLn("{{}}", [])             // {}
```

`[]` is an empty argument list. `Format()` and `.AddFmt()` also require their
argument-list parameter even when no values are inserted.

## Width, Fill, Sign, Precision

A placeholder can specify alignment and a minimum field width. Numeric values
can also request a sign, zero padding, and floating-point precision. The
supported option order is:

```text
{: [alignment] [sign] [0] [width] [.precision] [kind] }
```

Spaces and square brackets in this diagram are explanatory; do not write them
in the placeholder. Each bracketed part is optional. Alignment is `<`, `>`, or
`^`; the sign option is `+`, `-`, or a space. For example, `{:+08.2f}` selects
an explicit sign, zero fill, width eight, and two fractional digits.

| Option | Example | Meaning |
| --- | --- | --- |
| `<` | `{:<30s}` | Left-align within the field |
| `>` | `{:>30s}` | Right-align within the field |
| `^` | `{:^30s}` | Center within the field |
| `+` | `{:+d}` | Show `+` for a non-negative signed number or float |
| `-` | `{:-d}` | Show a sign only for negative values; this is the default |
| Space | `{: d}` | Use a leading space for a non-negative signed number or float |
| `0` | `{:05d}` | Use zeros for right-aligned numeric padding |
| Width | `{:30s}` | Reserve at least 30 output bytes |
| Precision | `{:.2f}` | Write two digits after the decimal point |

### Width and Alignment

Width and alignment work with text, characters, numbers, Boolean values, null,
and pointers. `<` pads on the right, `>` pads on the left, and `^` divides the
padding between both sides. If centering needs an odd number of padding spaces,
the extra space goes on the right.

Brackets in these examples make the padding spaces visible:

```dq
PrintLn("[{:<8s}] [{:>8s}] [{:^8s}]", ["Ada", "Ada", "Ada"])
// [Ada     ] [     Ada] [  Ada   ]
```

The same alignment markers work with integers and floating-point values:

```dq
PrintLn("[{:<8d}] [{:>8d}] [{:^8d}]", [42, 42, 42])
// [42      ] [      42] [   42   ]
PrintLn("[{:<8.2f}] [{:>8.2f}] [{:^8.2f}]", [3.5, 3.5, 3.5])
// [3.50    ] [    3.50] [  3.50  ]
```

DQ defaults to **right alignment for every value type**, including text. The
`s` kind is optional for text, so `{:30s}` and `{:30}` both produce a
right-aligned field at least 30 bytes wide. Use `<` explicitly for a left-aligned
text column:

```dq
var name : rostr = "Ada"
PrintLn("[{:6s}] [{:6d}]", [name, 7])  // [   Ada] [     7]
PrintLn("[{:<6}] [{:3d}]", [name, 7])  // [Ada   ] [  7]
```

Width is a minimum, not a maximum: longer values are never truncated to fit.
Width counts output bytes, so a UTF-8 character can occupy more than one byte
of the field:

```dq
PrintLn("[{:3d}] [{:3s}]", [12345, "abcdef"]) // [12345] [abcdef]
PrintLn("[{:>5s}]", ["€"]) // [  €]: two spaces plus three UTF-8 bytes
```

### Signs and Zero Fill

A negative sign is written automatically. `+` also requests a sign for
non-negative signed numbers and floating-point values. A space sign option
reserves a sign position without writing `+`. The `-` option requests the
default sign behavior; use `<` for left alignment.

```dq
PrintLn("{:+d} {:+d} {:05d}", [12, -12, -7]) // +12 -12 -0007
PrintLn("{:+08d}", [42])                    // +0000042
PrintLn("[{: d}] [{: d}]", [12, -12])        // [ 12] [-12]
PrintLn("{:08X}", [255])                    // 000000FF
```

Zero padding applies to right-aligned numbers and is written after the sign
or pointer prefix. Left and center alignment use spaces even if `0` is present.
Text and character output also use spaces.

```dq
PrintLn("[{:>06d}] [{:<06d}] [{:^06d}]", [-12, -12, -12])
// [-00012] [-12   ] [ -12  ]
```

Write alignment first, followed by the sign, zero fill, and width. For example,
`{:^+8d}` centers a signed number, and `{:+08d}` enables zero fill with a sign.
The former zero-before-sign spelling `{:0+8d}` is no longer supported.

### Floating-Point Precision

An explicit precision fixes the number of digits after the decimal point,
rounding as needed and retaining trailing zeros. The output decimal separator
is a dot.

```dq
var price : float = 3.5
PrintLn("default={}, fixed={:.2f}", [price, price]) // default=3.5, fixed=3.50
PrintLn("{:.2f} {:.0f}", [3.14159, 3.5])          // 3.14 4
PrintLn("[{:8.2f}] [{:+08.2f}]", [price, price])  // [    3.50] [+0003.50]
```

Without explicit precision, the formatter uses up to 15 fractional digits and
removes trailing zeros. Explicit precision is limited to 18 fractional digits.
The `f` placeholder requires a floating-point value; an integer does not become
a float merely because the placeholder says `f`.

## Formatting APIs and Destinations

The same placeholders and `[]anyvalue` argument syntax work with each API:

| API | Destination | Availability |
| --- | --- | --- |
| `Print(fmt, args)` / `PrintLn(fmt, args)` | Standard output | `use print` |
| `Format(fmt, args)` | A new `str` | Implicit `sys`; requires dynamic-string support |
| `.AddFmt(fmt, args)` on `str` | Appends to a dynamic string | Requires dynamic-string support |
| `.AddFmt(fmt, args)` on `embstr` | Appends to a fixed buffer | Works without dynamic-string support |
| `Exception(fmt, args)` | Exception message | Requires exception support |

The format parameter is `rostr`: use a literal or existing zero-terminated text.
A `strslice` is not implicitly converted to `rostr`; copy it to a terminated
`str` or `embstr(N)` first when using it as the format string. Text argument
values can still be unterminated `strslice` views.

### Print and PrintLn

```text
Print(text : strslice)
Print(fmt : rostr, args : []anyvalue)
PrintLn()
PrintLn(text : strslice)
PrintLn(fmt : rostr, args : []anyvalue)
```

`Print` writes without adding a newline. `PrintLn` adds a newline, and
`PrintLn()` writes a blank line.

```dq
Print("Hello, {}", ["Ada"])
PrintLn("!")
PrintLn()
// Hello, Ada! followed by a blank line
```

The one-argument form writes its text **literally**, without interpreting
placeholders or escaped braces. Supplying the second argument selects the
formatting overload:

```dq
PrintLn("{}")        // {}
PrintLn("{{}}")      // {{}}
PrintLn("{{}}", [])  // {}
```

### Format

```text
Format(fmt : rostr, args : []anyvalue) -> str
```

Use `Format()` when the result should be stored or returned rather than
immediately printed. It is available without a `use` declaration.

```dq
var label : str = Format("item-{:04d}", [7]) // "item-0007"
PrintLn(label)                             // item-0007
```

### .AddFmt

```text
.AddFmt(fmt : rostr, args : []anyvalue)
```

`.AddFmt()` appends to the existing contents. Initialize an embedded buffer
before the first append. `.Clear()` makes it empty when starting a new message.

```dq
var text : str = "Order "
text.AddFmt("{}: {} items", [17, 3])
PrintLn(text) // Order 17: 3 items

var buffer : embstr(32) = ""
buffer.AddFmt("{} = {:04X}", ["mask", 255])
PrintLn(buffer) // mask = 00FF
```

A `str` grows as needed. An `embstr(N)` keeps its fixed buffer, including space
for the terminating zero, so formatting can truncate the result:

```dq
var buffer : embstr(8) = ""
buffer.AddFmt("{}", ["abcdefghij"])
PrintLn(buffer) // abcdefg: seven text bytes plus the terminating zero fit
```

Repeated `.AddFmt()` calls can assemble a larger message. Reserve space in a
`str` first when its expected size is known:

```dq
var text : str = ""
text.Reserve(64)
text.AddFmt("name={}", ["Ada"])
text.AddFmt(", count={:03d}", [3])
PrintLn(text) // name=Ada, count=003
```

### Exception Messages

The formatted `Exception` constructor takes the same format and argument types:

```text
Exception(fmt : rostr, args : []anyvalue)
```

```dq
try:
    raise Exception("invalid count: {}", [-3])
except Exception as error:
    PrintLn(error.message) // invalid count: -3
endtry
```

See [Exceptions and Error Handling](140-exceptions-and-error-handling.md) for
raising, catching, and inspecting exceptions.

## Argument and Error Rules

Formatting uses the stored value's type. For example, `{:f}` requires a float,
while `{:d}` expects an integer or a character code. Convert the value explicitly
before formatting when another category is needed:

```dq
var count : int = 42
PrintLn("{:.2f}", [float(count)])   // 42.00
PrintLn("{:d}", [StrToInt("42")]) // 42
```

A type/format mismatch produces a diagnostic marker instead of throwing a
formatting exception:

```dq
PrintLn("{:f}", [42])   // <i-f!>: an integer with a float placeholder
PrintLn("{:d}", ["42"]) // <s-f!>: text with a numeric placeholder
PrintLn("{:d}", [true]) // <b-f!>: a Boolean with a numeric placeholder
```

The format language is DQ-specific. C `printf` placeholders such as `%d` have
no substitution meaning here. Use `{}` or a supported `{:...}` placeholder.

### Low-Level Formatter Flags

`Format()`, `.AddFmt()`, and the print helpers do not return a formatting status.
For direct control, `rtl/textformat` exposes `OTextFormatter`, which writes
through an `OTextSink` such as `OTextSinkEmbStr`, `OTextSinkStr`, or
`OTextSinkStdOut`. Its `.flags` records `DQFMT_FMT_ERROR` for malformed braces
or unsupported format options, and `DQFMT_ARG_ERROR` for incompatible value types or invalid Unicode
characters. Each formatting call resets the flags.

```dq
use print
use rtl/textformat

func *Main():
    var buffer : embstr(32) = ""
    var sink <- OTextSinkEmbStr(buffer)
    var formatter <- OTextFormatter(sink)
    formatter.Format("{:f}", [42])
    PrintLn("{} / error={}", [buffer, formatter.flags == DQFMT_ARG_ERROR])
endfunc
```

Output:

```text
<i-f!> / error=true
```

The runtime also defines `DQFMT_TRUNCATED` and `DQFMT_SINK_ERROR`, but the current
built-in sinks do not set them when a fixed buffer fills. Keep the destination
capacity in mind when complete output is required.
