# Enumerations

An enumeration, or **enum**, gives names to a set of choices. Use one for values
such as an order's status, a traffic light's color, or a game's difficulty.
Each choice is called an **enumerator**.

Enum type names conventionally start with a capital `N`, as in `NOrderStatus`.

Here is a small program that changes an order's status:

```dq
use print

enum NOrderStatus = (pending, shipped, delivered)

func *Main():
    var status : NOrderStatus = pending
    status = shipped
    if status == shipped:
        PrintLn("Your order is on its way.")
    endif
endfunc
```

This prints `Your order is on its way.`. The enum makes the choices clear and
prevents an unrelated number or a value from another enum being assigned to
`status`.

## Declaration

Write `enum`, a type name, and the choices in parentheses:

```dq
enum NColor = (red, green, blue)
```

The choices have integer values behind the scenes. By default, the first is
zero and each following choice adds one: `red` is `0`, `green` is `1`, and
`blue` is `2`. You normally work with the names rather than these numbers.

### Explicit Values and Storage

In the declaration, assign an integer value with `=` after a choice's name.
You can also select the storage type with `: type` after the enum type name:

```dq
enum NState : uint16 = (
    idle = 0,
    running = 10,
    stopped = 20,
)

enum NSigned : int8 = (negative = -1, zero, positive)
```

Explicit values must be compile-time integer constants. After an explicit
value, the next implicit value is one higher: `NSigned.zero` is `0` and
`NSigned.positive` is `1`. Values must be unique and increase in declaration
order; a later explicit value cannot move backward.

Only declared values belong to the enum. For example, `NState` has values
`0`, `10`, and `20`; `11` is not an enum member. Explicit values are useful
when matching a file format or an external interface.

The default storage type is `uint8`. You can select `int8`, `uint8`, `int16`,
`uint16`, `int32`, `uint32`, `int64`, or `uint64` to fit the required range.
For example, `NState` above occupies two bytes, while `NSigned` occupies one
byte and allows negative values. Every choice, including an implicit value,
must fit the storage type. The enum remains a separate type from the integer
used to store it.

## Naming and Lookup

When the expected enum type is clear, use the choice's name directly. You can
also include the enum type name:

```dq
var color : NColor = green
color = NColor.blue
var is_blue : bool = color == blue // true
```

The shorter form works in assignments, function arguments, return values, and
comparisons when the compiler can identify one expected enum type. Write
`NColor.green` when that context is missing or the name could be ambiguous.

## Type Safety

Compare values of the same enum with `==`, `<>`, `<`, `<=`, `>`, or `>=`.
Ordering follows their integer values, so `red < blue` when both are used as
`NColor` values.

Enums do not support arithmetic or automatic conversion to integers. For
example, assigning `1` to an `NColor` variable is an error even though `green`
has that integer value. Use the conversion helpers below when reading numbers
from a file or another program.

## Ordinal Conversion

Use the read-only `.ord` property or `Ord(value)` to get a choice's integer
value:

```dq
var color : NColor = green
var code : uint8 = color.ord // 1
```

To turn a number into an enum, use the enum type's methods. These methods check
that the number belongs to a declared choice:

| Method | Use it when |
| --- | --- |
| `NColor.FromOrd(number)` | The number must be valid; an unknown value raises a runtime error. |
| `NColor.FromOrd(number, fallback)` | An unknown value should use a fallback choice. |
| `NColor.TryFromOrd(number, output)` | You want to check success before using the result. |

For example, decode a saved color with a fallback:

```dq
var saved_code : int = 99
var color : NColor = NColor.FromOrd(saved_code, red) // red: 99 is unknown

if NColor.TryFromOrd(2, color):
    // color is now blue
endif
```

`TryFromOrd` returns `true` and updates `output` for a valid number. Otherwise it
returns `false` and leaves `output` unchanged. Use this method or a fallback
when a file or message might contain choices added by a newer program.
