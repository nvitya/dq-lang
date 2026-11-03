# `print`

Import `print` to write text to standard output:

```dq
use print

func *Main():
    Print("Items: ")
    PrintLn("{}", [3])
    PrintLn()
endfunc
```

This writes `Items: 3`, followed by an empty line.

## Functions

```dq
func Print(fmt : strslice)
func Print(fmt : rostr, args : []anyvalue)
func PrintLn()
func PrintLn(fmt : strslice)
func PrintLn(fmt : rostr, args : []anyvalue)
func PrintFlush()
func PrintSetUnbuffered()
```

`Print` writes without a newline. `PrintLn` appends a newline; its no-argument
form writes only that newline.

The one-argument forms write text literally. Braces have a formatting meaning
only when you supply the `[]anyvalue` argument list:

```dq
PrintLn("{name}")              // prints {name}
PrintLn("{} = {:04X}", ["id", 255]) // prints id = 00FF
```

Use a bracketed list even when there is only one formatted argument. See
[Text Formatting](../020-language/077-text-formatting.md) for widths, precision,
number formats, escaping braces, and formatting error behavior.

## Buffering

`PrintFlush()` asks the C runtime to flush standard output. This is useful for a
prompt without a newline. `PrintSetUnbuffered()` disables stdout buffering via
the runtime; configure buffering before normal output begins.

These functions use the shared stdout sink and formatter. For custom output
sinks or building a string instead of printing, see
[Text Formatting](../020-language/077-text-formatting.md) and [`strutils`](030-strutils.md).
