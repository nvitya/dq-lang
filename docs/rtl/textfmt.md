# Text Formatting

DQ text formatting is used by:

| API | Result |
| --- | --- |
| `Format(fmt, args)` from the implicit runtime interface | returns a new `str` |
| `Print(fmt, args)` / `PrintLn(fmt, args)` from `print` | writes to stdout |
| `str.AddFmt(fmt, args)` | appends formatted text to a dynamic string |
| `embstr.AddFmt(fmt, args)` | appends formatted text to an Embedded String |
| `Exception.Create(fmt, args)` | formats an exception message |

Formatting arguments are passed as `[]anyvalue`.

```dq
use print

PrintLn("{} = {:+08d}", ["answer", 42])

var s : str = Format("hex={:04X}", [255])
```

`Format` is provided by `rtl/strfunc`, reexported by implicit `sys`, so it needs
no `use` declaration. It is available when dynamic strings are enabled.

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

The formatter supports width, alignment, sign, zero fill, and float precision:

```dq
PrintLn("[{:5d}] [{:<5d}] [{:05d}]", [7, 7, 7])
PrintLn("{:+d} {:.2f} {:08X}", [12, 3.14159, 255])
PrintLn("|{:<6}|{:>6}|{:^6}|", ["abc", "abc", "abc"])
// Prints: |abc   |   abc| abc  |
```

| Part | Example | Meaning |
| --- | --- | --- |
| `0` | `{:05d}` | use zero fill |
| `+` | `{:+d}` | print a plus sign for positive signed numbers |
| `-` | `{:-5d}` | print a sign only for negative numbers (the default) |
| space | `{: 5d}` | reserve a leading space for non-negative signed numbers |
| `<` | `{:<30}` | left align within the width |
| `>` | `{:>30}` | right align within the width |
| `^` | `{:^30}` | center within the width |
| width | `{:8d}` | minimum field width |
| precision | `{:.2f}` | digits after decimal for floats |

Alignment precedes sign, zero fill, width, precision, and type, for example
`{:^+10.2f}` or `{:+08.2f}`. As in C++ and Python, `-` is a sign option;
use `<` for left alignment. The old DQ `-` alignment and zero-before-sign
order (`{:0+8.2f}`) are no longer supported.

Widths apply to text, characters, numbers, booleans, null values, and pointers.
The default alignment is right. Width is a minimum measured in output bytes;
values wider than the field are never truncated. Centering puts any extra padding
byte on the right. Padding uses spaces, except numeric right alignment with `0`,
which adds zeros after the sign or pointer prefix. Left and center alignment use
spaces even when `0` is specified.

Unsupported or mismatched format/type combinations do not throw. The formatter
writes a short marker such as `<i-f!>` and records an internal formatter flag.

## Sinks

The runtime formatter writes through `OTextSink`. The built-in sinks are stdout,
fixed `embstr`, and dynamic `str`.

```dq
var s : str = "value: "
s.AddFmt("{}", [123])

var cs : embstr(31) = ""
cs.AddFmt("{}", [123])
```

`embstr` formatting is bounded by the destination storage. Dynamic `str`
formatting can allocate as needed.
