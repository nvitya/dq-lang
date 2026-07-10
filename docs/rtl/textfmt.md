# Text Formatting

DQ text formatting is used by:

| API | Result |
| --- | --- |
| `Format(fmt, args)` from `strutils` | returns a new `str` |
| `Print(fmt, args)` / `PrintLn(fmt, args)` from `print` | writes to stdout |
| `str.AddFmt(fmt, args)` | appends formatted text to a dynamic string |
| `cstring.AddFmt(fmt, args)` | appends formatted text to fixed C string storage |
| `Exception.Create(fmt, args)` | formats an exception message |

Formatting arguments are passed as `[]anyvalue`.

```dq
use print
use strutils

PrintLn("{} = {:+08d}", ["answer", 42])

var s : str = Format("hex={:04X}", [255])
```

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

Unsupported or mismatched format/type combinations do not throw. The formatter
writes a short marker such as `<i-f!>` and records an internal formatter flag.

## Sinks

The runtime formatter writes through `OTextSink`. The built-in sinks are stdout,
fixed `cstring`, and dynamic `str`.

```dq
var s : str = "value: "
s.AddFmt("{}", [123])

var cs : cstring(31) = ""
cs.AddFmt("{}", [123])
```

`cstring` formatting is bounded by the destination storage. Dynamic `str`
formatting can allocate as needed.

