# Expressions and Operators

An expression computes a typed value, such as `count + 1`, `ready and valid`,
or a function call. Some expressions also identify storage that can be assigned.

```dq
use print

func *Main():
    var ratio : float = 3 / 2
    var quotient : int = 7 div 3
    var remainder : int = -7 rem 3
    var modulo : int = -7 mod 3
    PrintLn("{}, {}, {}, {}", [ratio, quotient, remainder, modulo])
endfunc
```

This prints `1.5, 2, -1, 2`.

## Assignments

`=` stores a value in an assignable destination. The destination must be
writable, and the value must be compatible with its type. Use `==` to compare
values for equality.

```dq
var count : int = 3
count = 10
count += 2       // count is now 12
count =div= 3    // count is now 4
```

Modify-assignment reads the destination's current value, performs an operation,
and stores the result back. The destination must therefore already be
initialized and both readable and writable.

Modify-assignment statements are `+=`, `-=`, `*=`, `/=`, `<<=`, `>>=`, `&=`,
`|=`, `=xor=`, `=div=`, `=rem=`, and `=mod=`. A word operator uses the leading
`=` spelling shown here.

Assignment is a statement and does not produce a value. It cannot be used as a
Boolean condition or nested to create chained assignments such as `a = b = 1`.
Assignment and modify-assignment are outside the operator precedence table.

## Arithmetic and Comparison

The tables below use *integer* for any signed or unsigned integer type, and
*number* for an integer or floating-point value. `bool`, characters, enums, and
pointers are separate categories; they are not implicitly treated as numbers.

### Arithmetic Operators

| Operator | Operands | Result | Meaning |
| --- | --- | --- | --- |
| `+` | Two numbers | Common numeric type | Addition. |
| `-` | Two numbers | Common numeric type; integer subtraction is signed | Subtraction. |
| `*` | Two numbers | Common numeric type | Multiplication. |
| `/` | Two numbers | Floating point | Floating-point division, including for two integers. |
| `div` | Two integers | Common integer type | Integer division, truncating toward zero. |
| `rem` | Two integers | Common integer type | Division remainder; a nonzero result has the dividend's sign. |
| `mod` | Two integers | Common integer type | Nonnegative modulo, including with a negative divisor. |

`+` also concatenates compatible text into a `str`; see
[Strings and Characters](070-strings-and-characters.md#text-operators).
Typed pointers support separate address arithmetic described in
[Pointers](110-pointers.md#pointer-arithmetic-and-indexing).

### Numeric Result Types

For ordinary numeric operations, DQ first converts operands to a common type:

| Operand types | Common type |
| --- | --- |
| Two signed integers | Signed integer at the wider operand width. |
| Two unsigned integers | Unsigned integer at the wider operand width. |
| One signed and one unsigned integer | Signed integer at the wider operand width, regardless of operand order. |
| One integer and one floating-point value | The floating-point operand's type. |
| Two floating-point values | The wider floating-point type. |

There are two arithmetic exceptions: integer subtraction always uses a signed
result at the wider width, and `/` always returns floating point. Dividing two
integers produces the target's default `float` type. Shifts have their own rule:
the result keeps the left operand's integer type.

```dq
var small : uint8 = 10
var signed_value : int16 = -3
var sum : int16 = small + signed_value  // 7; reversing the operands keeps int16
var difference : int8 = small - uint8(12) // -2, although both operands are unsigned
var ratio : float = 3 / 2               // 1.5
```

A common signed type is not necessarily large enough to represent every value
of an unsigned operand of the same width. Conversion retains its bits; it does
not add a spare sign bit. Choose a wider type explicitly when the full unsigned
range must participate in a signed calculation. Calculation widths and overflow
are explained in [Integer Calculations](#integer-calculations).

### Division, Remainder, and Modulo

`/` performs floating-point division. `div` instead discards the fractional
part toward zero, so `7 div 3` is `2` and `-7 div 3` is `-2`.

`rem` is the remainder associated with that truncating division. For valid
integer division, it satisfies `a rem b == a - ((a div b) * b)`.
`mod` starts with that remainder and, when it is negative, adds the magnitude
of the divisor to make it nonnegative:

| `a` | `b` | `a div b` | `a rem b` | `a mod b` |
| --- | --- | --- | --- | --- |
| `7` | `3` | `2` | `1` | `1` |
| `-7` | `3` | `-2` | `-1` | `2` |
| `7` | `-3` | `-2` | `1` | `1` |
| `-7` | `-3` | `2` | `-1` | `2` |

For a nonzero divisor, the modulo result is between zero, inclusive, and the
divisor's magnitude, exclusive. This is useful for wrapping a possibly negative
position into a positive range:

```dq
var position : int = -1
var slot : int = position mod 8  // 7
```

DQ uses `rem`, not `%`, for integer remainder. `%` obtains an address.
`div`, `rem`, and `mod` reject floating-point operands; use an explicit rounding
operation when an integer calculation is intended. Integer division by zero and
signed minimum divided by `-1` have no defined result; guard these cases.

### Comparison Operators

Every comparison returns `bool`.

| Operator | Operands | Meaning |
| --- | --- | --- |
| `==` | Compatible values | Equality. |
| `<>` | Compatible values | Inequality. |
| `<`, `<=`, `>`, `>=` | Numbers, matching character types, or values of the same enum type | Less than, less than or equal, greater than, greater than or equal. |

Numeric comparisons use the common numeric type rules above. Two integers
compare as signed if either is signed, and as unsigned if both are unsigned.
If either operand is floating point, comparison is floating point.

Boolean equality requires two Boolean values. Numbers are not converted to
Boolean values for comparison: `true == 1` is invalid. `!=` is not accepted.
Enums compare only with the same enum type; ordering follows their ordinal
values. Character comparison follows character codes, without case folding.

```dq
var count : int = 3
var limit : float = 3.5
var below_limit : bool = count < limit  // true: numeric conversion is allowed
var ready : bool = true
var unchanged : bool = ready == true   // true
```

Compatible pointers and object references support identity comparisons with
`==` and `<>`, including comparison with `nil`; this does not compare pointed-to
contents. Compatible string-family values support byte-content equality, but
not ordering. See [Strings and Characters](070-strings-and-characters.md#text-operators).
General array sequence equality is not implicit. Dynamic arrays and slices can
be compared with `[]` to test emptiness.

### Range Comparisons

DQ supports a chain of two ordering comparisons with three operands. Both
comparisons must point in the same direction:

| Direction | Supported forms |
| --- | --- |
| Ascending | `x < y < z`, `x <= y < z`, `x < y <= z`, `x <= y <= z` |
| Descending | `x > y > z`, `x >= y > z`, `x > y >= z`, `x >= y >= z` |

```dq
var temperature : int = 20
var comfortable : bool = 18 <= temperature < 25  // true
var within_limits : bool = 25 > temperature >= 18 // true
```

`lower <= value < upper` tests the same bounds as
`(lower <= value) and (value < upper)`, but evaluates the middle expression only
once. The final endpoint is evaluated only if the first comparison succeeds:

```dq
use print

var calls : int = 0

func MiddleValue() -> int:
    ++calls
    return 2
endfunc

func *Main():
    var inside : bool = 1 < MiddleValue() < 3 // calls becomes 1
    var skipped : bool = 3 < 2 < MiddleValue() // the function is not called
    PrintLn("{} {} {}", [inside, skipped, calls]) // true false 1
endfunc
```

Longer chains such as `w < x < y < z`, direction changes such as `x < y > z`,
and equality chains such as `a == b == c` are invalid. Write separate
comparisons joined with `and` when the condition needs those forms.

## Integer Calculations

DQ integer arithmetic uses fixed widths and has no overflow checks. The widest
operand determines the calculation width. A whole-number literal has the normal
`int` calculation type and can widen a mixed calculation:

```dq
var left : uint8 = 250
var right : uint8 = 10
var narrow_sum : int = left + right // 4: the calculation uses eight bits
var wide_sum : int = left + 10     // 260: the literal makes this an int calculation
left = left + 10                   // 4: storing into uint8 keeps the low bits
```

The destination does not widen a calculation retroactively. Signed narrowing
interprets the retained bits as a two's-complement value. The same rules apply
to compile-time and runtime expressions.

Subtraction always produces a signed result at the widest operand width, even
when both operands are unsigned. Thus `uint(3) - uint(5)` is signed `-2` at the
native width. An assignment to unsigned storage reinterprets the retained bits.

## Boolean and Bitwise Operations

### Logical Operators

| Operator | Operands | Result | Meaning |
| --- | --- | --- | --- |
| `not value` | One `bool` | `bool` | Reverse the Boolean value. |
| `left and right` | Two `bool` values | `bool` | True when both operands are true. |
| `left or right` | Two `bool` values | `bool` | True when at least one operand is true. |

`and` and `or` short-circuit. `and` evaluates its right operand only if the left
is true; `or` evaluates its right operand only if the left is false. This allows
a condition to guard a later operation:

```dq
func IsPositive(address : ^int) -> bool:
    return address <> nil and address^ > 0
endfunc
```

For `IsPositive(nil)`, the result is `false` and `address^` is not evaluated.
An `or` condition can supply the opposite guard:

```dq
var address : ^int = nil
var missing_or_zero : bool = address == nil or address^ == 0 // true
```

There is no numeric truthiness: `not 0` and `1 and 2` are invalid. Use a
comparison such as `count <> 0` to produce a Boolean value.

### Bitwise Operators

Bitwise operators work on integer bits, not Boolean truth values. They require
integer operands and do not implicitly round floating-point values.

| Operator | Operands | Result | Meaning |
| --- | --- | --- | --- |
| `~value` | One integer | Operand's integer type | Complement every bit. |
| `left & right` | Two integers | Common integer type | Set bits present in both operands. |
| `left \| right` | Two integers | Common integer type | Set bits present in either operand. |
| `left xor right` | Two integers | Common integer type | Set bits present in exactly one operand. |
| `value << count` | Two integers | Left operand's type | Shift left, filling low bits with zeroes. |
| `value >> count` | Two integers | Left operand's type | Shift right; signed values extend the sign bit, unsigned values fill with zeroes. |

For `&`, `|`, and `xor`, signedness and width follow the common integer rules.
For shifts, the count does not change the left operand's type or signedness.
The count must be nonnegative and smaller than that type's bit width. Bits
shifted out are discarded; shifts do not report overflow.

```dq
var a : uint8 = 0x0C       // 00001100
var b : uint8 = 0x0A       // 00001010
var both : uint8 = a & b   // 00001000 = 8
var either : uint8 = a | b // 00001110 = 14
var different : uint8 = a xor b // 00000110 = 6
var inverted : uint8 = ~a  // 11110011 = 243
var shifted : uint8 = a << 1 // 00011000 = 24; result still has type uint8
```

To test whether any flag bits are set, compare the masked integer with zero:

```dq
var flags : uint = 0x12
var mask : uint = 0x02
var enabled : bool = flags & mask <> 0 // true
```

Bitwise operations bind more tightly than arithmetic and comparison.
`flags & mask <> 0` therefore means `(flags & mask) <> 0`. Logical `and` and
`or` bind more loosely than comparisons. Neither `&` nor `|` short-circuits.

## Unary and Storage Operations

A unary operator acts on one operand. Most are written before it; pointer
dereference is written after it:

| Form | Operand | Result | Meaning |
| --- | --- | --- | --- |
| `-value` | A number | Same floating-point type, or signed integer of the operand's width | Numerical negation. |
| `not value` | `bool` | `bool` | Logical negation. |
| `~value` | An integer | Same integer type | Bitwise complement. |
| `%value` | Addressable storage of type `T` | `^T` | Obtain its address. |
| `address^` | A typed pointer `^T` | The referenced `T` | Access the value at that address. |

Negating an unsigned integer produces a signed result at the same width. Like
other fixed-width integer calculations, this can overflow when the mathematical
result does not fit. Widen the operand before negation when necessary:

```dq
var magnitude : uint8 = 7
var negative : int8 = -magnitude // -7
var value : int = 10
var address : ^int = %value
address^ = 42                   // value becomes 42
```

The prefix `^` in `^int` is a **type designator**, not an expression operator.
`^int` means pointer to `int`, and `^^int` means pointer to pointer to `int`.
The postfix `^` in `address^` instead dereferences a pointer value. `%` requires
an addressable operand; it cannot obtain the address of a literal or a property.

Prefix `++value` and `--value` update an assignable number or typed pointer as
statements. They produce no value and do not introduce C-style postfix
increment expressions.

Member access and indexing extend an expression:

| Form | Operand | Result |
| --- | --- | --- |
| `value.member` | A structure, object, or supported pointer | The named field, method, or property. |
| `array[index]` | An array and integer index | The selected element. |
| `pointer[index]` | A typed pointer `^T` and integer offset | Another `^T`, offset by that many elements. |

Pointer indexing does not dereference. Write `pointer[index]^` to access the
pointed-to element:

```dq
var values : [3]int = [10, 20, 30]
var first : ^int = %values[0]
var second : ^int = first[1] // points to values[1]
var value : int = second^   // 20
```

See [Pointers](110-pointers.md) for address arithmetic and lifetime rules, and
[Arrays and Slices](080-arrays-and-slices.md) for element access and slicing.

## Explicit Conversion and Runtime Type Tests

`Type(expression)` and `expression as Type` request the same defined conversion.
`as` binds at comparison precedence and cannot be chained with another
comparison-level operator. Parenthesize its result before further operations.

`value is OType` tests an object's runtime type and returns `false` for `nil`.
`TryCast` and `tryfrom` perform checked object casts; see
[Objects and Properties](120-objects-and-properties.md#runtime-type-tests-and-casts).
A cast does not validate an arbitrary pointer, encoding, or enum ordinal.

## Inline Conditional

`iif(condition, when_true, when_false)` selects a value within an expression.
It evaluates `condition` first, then evaluates and returns `when_true` if the
condition is `true`, or `when_false` otherwise. The condition must have type
`bool`.

```dq
var count : int = 0
var label : rostr = iif(count == 0, "empty", "nonempty")
```

Here `label` receives `"empty"`. The alternatives can also be numeric or other
compatible value types. For example, an integer and a floating-point alternative
resolve to a floating-point result:

```dq
var use_default : bool = false
var scale : float = iif(use_default, 1, 2.5) // 2.5; true would select 1
```

The alternatives must resolve to a common result type under DQ's conversion
rules. Numeric alternatives use the numeric type rules, and a compatible typed
pointer can be paired with `nil`. The result must also be assignable to its
destination: a floating-point result does not
implicitly become an integer merely because the destination is `int`.

Only the selected alternative runs. This makes it possible to guard an operation
that would be invalid for the other case:

```dq
func ValueOrZero(address : ^int) -> int:
    return iif(address <> nil, address^, 0)
endfunc
```

`ValueOrZero(nil)` returns `0` without dereferencing `nil`. Calls and other side
effects in the unselected alternative do not occur. An error in the selected
alternative still occurs normally; `iif` does not catch exceptions.

Both alternatives are parsed and type-checked, even when the condition is a
constant. An unselected alternative cannot contain an unknown name or an
incompatible type. The following uses are compile errors:

| Invalid expression | Reason |
| --- | --- |
| `iif(1, 2, 3)` | The condition is an integer, not `bool`. |
| `iif(true, 1, "text")` | The alternatives have no compatible common type. |
| `iif(true, 1)` | The false alternative is missing. |
| `iif(true, 1, 2, 3)` | Exactly three arguments are required. |

Each alternative must produce a value; a call to a function with no result
cannot serve as an alternative. Use an `if` statement when choosing between
actions that do not produce values.

## Calls and Evaluation

Calls use parentheses and pass arguments positionally, in declaration order.
Arguments may themselves be expressions or calls. Trailing arguments can be
omitted when the function declares defaults:

```dq
use print

func Subtract(left : int, right : int = 1) -> int:
    return left - right
endfunc

func *Main():
    var difference : int = Subtract(5 + 5, 3)           // 7
    var previous : int = Subtract(10)                 // 9: right defaults to 1
    var nested : int = Subtract(Subtract(10, 3), 2)    // 5
    PrintLn("{} {} {}", [difference, previous, nested])
endfunc
```

The first argument supplies `left`, and the second supplies `right`. Named
arguments such as `Subtract(right = 3, left = 10)` are not supported. In the
nested example, the inner call returns `7` before the outer call subtracts `2`.

Arguments are evaluated before the called function is entered.
Avoid relying on a relative evaluation order between independent expressions
with side effects unless a construct explicitly specifies that order. When
order matters, use separate statements. With `Subtract` defined above:

```dq
func Next(counter : ref int) -> int:
    ++counter
    return counter
endfunc

func OrderedDifference() -> int:
    var counter : int = 0
    var first : int = Next(counter)   // counter becomes 1
    var second : int = Next(counter)  // counter becomes 2
    return Subtract(first, second)    // -1
endfunc
```

This explicitly orders the two updates instead of depending on the argument
evaluation order in `Subtract(Next(counter), Next(counter))`. Reference
parameters such as `ref int` are described in
[Functions](060-functions.md#parameter-modes).

Built-ins such as `Len`, `SizeOf`, `OffsetOf`, `Ord`, `Round`, `Defined`, and
`FirstInt` use call-like syntax but follow their own type and constant-expression
rules:

```dq
var values : [3]int = [10, 20, 30]
var count : int = Len(values)       // 3 elements
var bytes : uint = SizeOf(int32)    // 4 bytes
var rounded : int = Round(3 / 2)    // 2
```

`SizeOf` accepts a type name as shown here, while an ordinary value parameter
expects a value expression. [Compiler Directives](190-compiler-directives.md)
explains compile-time existence tests and optional constants.

## Associativity

Associativity determines grouping when operators have the same precedence.
Arithmetic and bitwise binary operators group from left to right:

```dq
var left_grouped : int = 10 - 3 - 2   // (10 - 3) - 2 = 5
var right_grouped : int = 10 - (3 - 2) // 10 - 1 = 9
var quotient : int = 20 div 3 div 2    // (20 div 3) div 2 = 3
```

Comparisons do not form ordinary left-associative chains. The three-operand
range comparison is the specific exception described in
[Range Comparisons](#range-comparisons). `is` and `as` likewise do not form
comparison chains; parenthesize a conversion before applying another operator
at that precedence level.

Grouping does not establish a general evaluation order for independent calls
with side effects. Use separate statements when their order matters, as shown
in [Calls and Evaluation](#calls-and-evaluation).

## Parentheses

`(expression)` groups an expression as one operand and overrides the usual
precedence. Parentheses can be nested. They do not by themselves convert its
type or turn a value into addressable storage.

```dq
var ordinary : int = 2 + 3 * 4    // 14
var grouped : int = (2 + 3) * 4   // 20
var divided : int = 8 * 3 div 2   // 8 * (3 div 2) = 8
var multiplied : int = (8 * 3) div 2 // 12
```

The last two lines illustrate DQ's division precedence: `div` binds more tightly
than `*`. Bitwise operators also bind more tightly than arithmetic:

```dq
var shifted_first : int = 1 << 2 + 1 // (1 << 2) + 1 = 5
var added_first : int = 1 << (2 + 1) // 1 << 3 = 8
```

Use parentheses to make the intended grouping clear when an expression mixes
operator groups. This is especially helpful when translating code from a
language with different precedence rules.

## Operator Precedence

From tightest to loosest binding:

| Level | Operators and forms |
| --- | --- |
| 1 | Literals, names, parenthesized expressions, casts, array literals, `new`, built-ins. |
| 2 | Calls, `.member`, indexing, slicing, postfix `^`. |
| 3 | Address-of `%`, unary `-`, bitwise `~`. |
| 4 | `<<`, `>>` |
| 5 | `&` |
| 6 | `\|`, `xor` |
| 7 | `/`, `div`, `rem`, `mod` |
| 8 | `*` |
| 9 | `+`, `-` |
| 10 | `==`, `<>`, `<`, `<=`, `>`, `>=`, `is`, `as` |
| 11 | `not` |
| 12 | `and` |
| 13 | `or` |

Division binds more tightly than multiplication. Bitwise operations also bind
more tightly than arithmetic. Use parentheses whenever mixed operators obscure
the intended grouping rather than assuming another language's precedence.
