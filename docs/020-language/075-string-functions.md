# String Functions

This chapter is a reference for string methods, standalone helpers, and text
operations. For storage, character types, conversions, and lifetime rules, see
[Character and String Types](070-strings-and-characters.md). See
[Text Formatting](160-text-formatting.md) for `AddFmt()` and `Format()` placeholders.

## Common Operations

Dynamic strings expose:

| Member | Meaning |
| --- | --- |
| `.length` | current byte length |
| `.capacity` | allocated capacity |
| `Set(text)` | replace contents |
| `Append(value)` / `Add(value)` | append text or a `char` byte |
| `AppendChar(ch)` | append one `char` byte |
| `Prepend(value)` | insert at the beginning |
| `Insert(index, value)` | insert text or a `char` byte |
| `Delete(index, count = 1)` | remove bytes |
| `SetLength(length, fill)` | resize, filling new bytes |
| `Truncate(length)` | shrink only |
| `Pop(count)` / `PopFirst(count)` | remove and return text |
| `Pop()` / `PopFirst()` | remove and return one `char` byte |
| `Reserve(capacity)` | ensure capacity |
| `Compact()` | set capacity to current length |
| `Clear(free_storage = false)` | clear contents, optionally free storage |
| `Clone()` | force an independent copy |
| `AddFmt(fmt, args)` | append formatted text |

```dq
var s : str = "abc"
s.Append("def")
s.Insert(0, '>')
s.Delete(0)
s.AddFmt(" {}", [123])
```

String indexing and slicing use byte offsets and return or copy `char` values.
They do not validate UTF-8.

```dq
var b : char = s[0]
var part : str = s[3:10]
```

Indices are normalized by the runtime operations. For example, deleting past
the end is clamped, and inserting with `$end` appends.

### Overlapping Source and Destination

String mutation methods preserve the logical source when their source aliases
the destination. This includes appending or inserting a view into the same
string across a possible reallocation.

## Utility Methods

The following read-only methods are available on `str`, `rostr`, `strslice`,
and `embstr` values. They accept a `char` or a text value where a search,
prefix, suffix, trim set, or fill value is required. Search positions and
lengths are byte-based, just like `.length` and normal indexing.

| Member | Result | Meaning |
| --- | --- | --- |
| `Trim()` / `Trim(set)` | `str` | remove whitespace, or bytes in `set`, from both ends |
| `LTrim()` / `LTrim(set)` | `str` | remove whitespace, or bytes in `set`, from the left end |
| `RTrim()` / `RTrim(set)` | `str` | remove whitespace, or bytes in `set`, from the right end |
| `LPad(length, fill)` | `str` | pad the left side to `length` bytes |
| `RPad(length, fill)` | `str` | pad the right side to `length` bytes |
| `IndexOf(needle, start = 0)` | `int` | first matching byte offset, or `-1` |
| `LastIndexOf(needle)` | `int` | last matching byte offset, or `-1` |
| `Contains(needle)` | `bool` | whether the value contains `needle` |
| `StartsWith(prefix)` | `bool` | whether the value begins with `prefix` |
| `EndsWith(suffix)` | `bool` | whether the value ends with `suffix` |

These methods do not modify the receiver. Assign a returned `str` to retain a
trimmed or padded value.

```dq
var text : str = "  abc  "
text = text.Trim()  // "abc"
```

`Trim`, `LTrim`, and `RTrim` use the ASCII whitespace bytes space, tab,
line feed, carriage return, vertical tab, and form feed by default. With an
argument, that argument is a set of bytes rather than a substring pattern.

```dq
var marked : str = "---abc---"
var name : str = marked.Trim("-")  // "abc"
```

Padding repeats the fill value and truncates the final repetition to produce
exactly the requested length. A target length no greater than the receiver
length returns an unchanged copy. An empty fill value raises a runtime error.

```dq
var code : str = "abc"
code.LPad(8, "01")  // "01010abc"
code.RPad(5, '.')   // "abc.."
```

`IndexOf` clamps `start` to `0 .. text.length`. An empty needle matches at the
normalized start position. `LastIndexOf("")` returns `text.length`; an empty
prefix or suffix matches every text value.

```dq
var text : str = "abcdefabc"
text.IndexOf("abc", 1)  // 6
text.IndexOf("", 100)   // 9
text.LastIndexOf("abc")  // 6
text.StartsWith("abc")   // true
text.EndsWith("abc")     // true
```

The trim and padding methods create a `str`, so they are unavailable on
targets built with dynamic strings disabled. The search methods only return
scalar values and remain available for the supported text views.


## Standalone String Helpers

String helpers are available through the implicit `sys` runtime interface;
ordinary modules need no `use` declaration.

| Function | Result | Meaning |
| --- | --- | --- |
| `Format(fmt : rostr, args : []anyvalue)` | `str` | create text using [formatting placeholders](160-text-formatting.md) |
| `StrToInt(text : strslice, defvalue : int = 0)` | `int` | parse decimal digits and a minus sign |
| `StrToFloat(text : strslice, defvalue : float)` | `float` | parse a number with a dot or comma decimal separator and an optional `e`/`E` exponent |
| `StrUpper(text : strslice)` | `str` | copy text, converting ASCII letters to uppercase |
| `StrLower(text : strslice)` | `str` | copy text, converting ASCII letters to lowercase |

The parsing functions return `defvalue` for empty input or an unrecognized
character. `StrToFloat` requires an explicit default value. The case conversion
functions leave non-ASCII bytes unchanged and do not modify the source.

```dq
var label : str = Format("value={:04X}", [255]) // "value=00FF"
var count : int = StrToInt("42")               // 42
var fallback : int = StrToInt("invalid", -1)   // -1
var value : float = StrToFloat("1.25e2", 0)    // 125
var upper : str = StrUpper("Hello, é!")       // "HELLO, é!"
var lower : str = StrLower("Hello, é!")       // "hello, é!"
```

`Format`, `StrUpper`, and `StrLower` create dynamic strings and require
dynamic-string support. `StrToInt` and `StrToFloat` remain available when
dynamic strings are disabled.

## Text Operators

`+` concatenates compatible text operands into a new `str`. `+=` appends to a
dynamic string. These operations require dynamic-string support.

```dq
var greeting : str = "Hello, " + "world"
greeting += "!"
var expected : rostr = "Hello, world!"
var matches : bool = greeting == expected
```

Using `+` to assemble dynamic strings is not recommended: it creates temporary
strings and can cause repeated heap allocations and copies. Prefer `Add()` for
plain text and `AddFmt()` for formatted text. Call `Reserve()` first when the
expected size is known or can be estimated. Reserving enough capacity improves
performance by avoiding heap reallocations and the associated copying of existing
text as the string grows. Appends reuse that storage until the reserved capacity
is exceeded. When more space is needed, the capacity is doubled repeatedly until
it is large enough for the new contents.

```dq
var text : str = ""
text.Reserve(32)
text.Add("Hello, ")
text.AddFmt("{}!", ["world"])
```

## String Comparison

Text values support `==`, `<>`, `<`, `<=`, `>`, and `>=`. You can compare
different string types, including `str`, `rostr`, `strslice`, and embedded
strings with or without an explicit capacity.

Strings are compared byte by byte, from left to right. The first different byte
determines which string comes first, using unsigned byte values. If one string
is the beginning of the other, the shorter string comes first. For example,
`"alpha" < "beta"` and `"app" < "apple"` are both true.

```dq
var name   : str = "alpha"
var limit  : rostr = "beta"
var before : bool = name < limit       // true
var prefix : bool = "app" < "apple"    // true
var inside : bool = "a" <= name < "z" // true
```

Comparison is case-sensitive: `"Hello"` and `"hello"` are different. It does
not validate UTF-8, normalize Unicode, or apply language-specific sorting rules.
Known string lengths include embedded zero bytes; raw C strings end at their
first zero.

`StrSliceCmp(a : strslice, b : strslice) -> int`, available through implicit
`sys`, provides the same bytewise comparison as a function. It returns a negative
value if `a` comes before `b`, zero if they are equal, and a positive value if
`a` comes after `b`. It accepts the text types through their normal conversions and
compares the entire known byte range, without requiring a zero terminator.

A raw `^char` operand compares text when paired with a string value or, for
ordering, a text literal. Two raw pointer variables compare their addresses;
use `StrSliceCmp` to compare their text contents explicitly.

## Byte Indexing and Slicing

`text[index]` reads one `char` byte; writable text storage also permits replacing it. `text[start:end]` uses a half-open
byte range, while `text[start::end]` includes the end byte. These operations do
not decode or validate UTF-8.

An index must select an existing byte and an invalid index raises a runtime range
error. Slice bounds are clamped to `[0, length]`; reversed normalized bounds
produce an empty slice rather than an error.

`$last` and `$end` are context-specific
values for the string being indexed or sliced. `$last` is the last existing byte
index and `$end` is the one-past-end position.

```dq
use print

func *Main():
    var text : str = "abcdef"
    var first : char = text[0]             // 'a'
    text[0] = 'A'                          // text is now "Abcdef"

    var part : strslice = text[1:3]        // "bc": end byte excluded
    var inclusive : strslice = text[1::3]  // "bcd": end byte included
    var head : strslice = text[:2]         // "Ab": first two bytes
    var tail : strslice = text[$end - 2:]  // "ef": last two bytes
    PrintLn("{} / {} / {} / {} / {}", [first, part, inclusive, head, tail])

    PrintLn("{} / {}", [text[$last], text[-5:100]])  // f / Abcdef
    var empty : strslice = text[4:2]    // reversed bounds: empty slice
    PrintLn("empty length = {}", [empty.length])   // empty length = 0
endfunc
```

The first printed line is `a / bc / bcd / Ab / ef`. The slices borrow the bytes in
`text`; the owning string must remain alive and stable while they are used.

## Unicode Operations

Unicode-oriented operations interpret `str` bytes as UTF-8:

- `.wclen` counts decoded Unicode scalar values;
- `.wchar[index]` selects a scalar;
- `.wchar[start:end]` returns scalar values;
- `.wcstr[start:end]` returns the selected scalars encoded as `str`;
- `ToWchars` and `StrFromWchars` convert between UTF-8 and `[*]wchar`;
- `ToUtf16` and `StrFromUtf16` convert between UTF-8 and UTF-16 code units.

Malformed UTF-8, invalid scalar values, and malformed UTF-16 produce runtime
encoding errors. `ToUtf16` returns an array that includes a final zero code unit.

### UTF-8 Byte Length and Character Count

Byte length and character count differ for non-ASCII UTF-8:

```dq
use print

func *Main():
    var text : str = "Aé€"
    var scalars : [*]wchar = text.ToWchars()
    PrintLn("bytes={} scalars={} second={}", [text.length, text.wclen, scalars[1]])
endfunc
```

This prints `bytes=6 scalars=3 second=é`. Repeated `.wchar[index]` lookup needs
UTF-8 decoding; converting once to `[*]wchar` is useful for repeated indexed
processing. An isolated `char16` surrogate is a code unit, not a complete Unicode
scalar; UTF-16 conversion checks pairs rather than treating every unit as a
character.
