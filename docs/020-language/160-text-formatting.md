# Text Formatting

Formatting inserts values into text using placeholders. DQ's print functions,
string formatting helpers, string append methods, and exception messages use
the same format language.

```dq
use print
use strutils

func *Main():
    PrintLn("{} = {:0+8d}", ["answer", 42])
    var label : str = Format("hex={:04X}", [255])
    PrintLn(label)
endfunc
```

This prints `answer = +0000042` and `hex=00FF` on separate lines.

DQ text formatting is used by:

| API | Result |
| --- | --- |
| `Format(fmt, args)` from `strutils` | returns a new `str` |
| `Print(fmt, args)` / `PrintLn(fmt, args)` from `print` | writes to stdout |
| `str.AddFmt(fmt, args)` | appends formatted text to a dynamic string |
| `embstr.AddFmt(fmt, args)` | appends formatted text to an Embedded String |
| `Exception.Create(fmt, args)` | formats an exception message |

Formatting arguments are passed as `[]anyvalue`.

## Placeholders

| Form | Meaning |
| --- | --- |
| `{}` | next argument, default formatting |
| `{{` | literal `{` |
| `}}` | literal `}` |
| `{:?}` | default/debug formatting |
| `{:d}` | signed decimal integer |
| `{:u}` | unsigned decimal integer |
| `{:x}` | lowercase hexadecimal integer |
| `{:X}` | uppercase hexadecimal integer |
| `{:p}` | pointer style hexadecimal |
| `{:s}` | text value |
| `{:f}` | floating point |

Each placeholder consumes the next argument. There is no explicit positional
argument syntax.

## Width, Fill, Sign, Precision

The formatter supports a small, printf-like subset:

```dq
PrintLn("[{:5d}] [{:-5d}] [{:05d}]", [7, 7, 7])
PrintLn("{:+d} {:.2f} {:08X}", [12, 3.14159, 255])
```

| Part | Example | Meaning |
| --- | --- | --- |
| `0` | `{:05d}` | use zero fill |
| `+` | `{:+d}` | print a plus sign for positive signed numbers |
| `-` | `{:-5d}` | left align within the width |
| width | `{:8d}` | minimum field width |
| precision | `{:.2f}` | digits after decimal for floats |

Flags are parsed in the order zero fill, plus sign, left alignment, width,
precision, and kind. For example, `{:0+8d}` requests zero fill and an explicit
sign; `{:+08d}` does not select zero fill because its `0` is parsed as part of
the width. Boolean values use `{}` or `{:?}`, not a separate Boolean specifier.

Unsupported or mismatched format/type combinations do not throw. The formatter
writes a short marker such as `<i-f!>` and records an internal formatter flag.

## Sinks

The runtime formatter writes through `OTextSink`. The built-in sinks are stdout,
fixed `embstr`, and dynamic `str`.

```dq
var s : str = "value: "
s.AddFmt("{}", [123])

var cs : embstr(32) = ""
cs.AddFmt("{}", [123])
```

`embstr` formatting is bounded by the destination storage. Dynamic `str`
formatting can allocate as needed.


## Argument and Error Rules

The format string is terminated read-only text (`rostr`), and values are passed
as `[]anyvalue`. A temporary list such as `[name, count]` is valid for the call;
code must not retain its array view afterward. A single placeholder can use one
value in a list, for example `PrintLn("{}", [count])`.

`{}` selects the representation from the stored kind. Character defaults write
characters; explicit decimal or hexadecimal formats write character codes.
Numeric formatting uses the stored numeric type; it does not perform arbitrary
text-to-number conversion. Width is a minimum, so larger output is not truncated
merely to fit the width.

The low-level formatter reports `DQFMT_TRUNCATED`, `DQFMT_FMT_ERROR`,
`DQFMT_ARG_ERROR`, and `DQFMT_SINK_ERROR` flags. A bounded sink can truncate its
output. Type/format mismatches produce diagnostic markers instead of throwing
an exception; check formatter flags when a low-level formatting operation needs
strict validation. Formatting is not the C `printf` format language: `%d` has no
placeholder meaning here.
