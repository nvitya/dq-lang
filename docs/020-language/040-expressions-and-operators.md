# Expressions and Operators

An expression computes a typed value, such as `count + 1`, `ready and valid`,
or a function call. Some expressions also identify storage that can be assigned.
Assignment itself is a statement, so it cannot be used as a Boolean condition
or nested to create chained assignments.

```dq
use print

func *Main():
    var ratio : float = 3 / 2
    var quotient : int = 7 div 3
    var remainder : int = -7 rem 3
    var modulo : int = -7 mod 3
    PrintLn("{} {} {} {}", [ratio, quotient, remainder, modulo])
endfunc
```

This prints `1.5 2 -1 2`.

## Arithmetic and Comparison

| Operator | Meaning |
| --- | --- |
| `+`, `-`, `*` | Numeric addition, subtraction, multiplication. |
| `/` | Floating-point division, including for integer operands. |
| `div` | Integer division, truncating toward zero. |
| `rem` | Integer remainder with the dividend's sign. |
| `mod` | Nonnegative integer modulo. |
| `==`, `<>` | Equality and inequality. |
| `<`, `<=`, `>`, `>=` | Ordering where defined for the operand types. |

`!=` is not accepted. Enum equality requires the same enum type; use ordinals
for enum ordering. Pointers and object references compare addresses/identity.
General array sequence equality is not implicit, although dynamic arrays can be
compared with `[]` to test emptiness. Text supports concatenation with `+` and
byte-content equality with `==` and `<>`; see
[Strings and Characters](070-strings-and-characters.md#text-operators).

Range comparisons such as `lower <= value < upper` and `high >= value >= low`
evaluate the middle expression once. The last endpoint is evaluated only when
the first comparison succeeds.

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

A signed right shift propagates the sign bit; an unsigned right shift fills with
zeroes. Shift counts must be nonnegative and less than the calculation width.
Integer division by zero and signed minimum divided by `-1` have no defined
result; guard those cases rather than relying on a runtime check.

## Boolean and Bitwise Operations

`not`, `and`, and `or` require `bool`. `and` and `or` short-circuit, evaluating
the right operand only when necessary. There is no numeric truthiness.

Bitwise operations are distinct: `~`, `&`, `|`, `xor`, `<<`, and `>>` operate
on integers. Their precedence is higher than arithmetic and comparison:
`value & mask <> 0` means `(value & mask) <> 0`.

## Unary and Storage Operations

`-value` negates a number; `~value` complements integer bits; `not value` negates
a Boolean. `%value` obtains the address of addressable storage, and postfix
`pointer^` dereferences a typed pointer. Prefix `++value` and `--value` update
an assignable number or typed pointer as statements; they do not introduce C-style postfix
increment expressions.

Member access, calls, indexing, and slicing are described with their types.
In particular, `p[i]` on a typed pointer returns another pointer; use `p[i]^`
to access the pointed-to element.

## Explicit Conversion and Runtime Type Tests

`Type(expression)` and `expression as Type` request the same defined conversion.
`as` binds at comparison precedence and cannot be chained with another
comparison-level operator. Parenthesize its result before further operations.

`value is OType` tests an object's runtime type and returns `false` for `nil`.
`TryCast` and `tryfrom` perform checked object casts; see
[Objects and Properties](120-objects-and-properties.md#runtime-type-tests-and-casts).
A cast does not validate an arbitrary pointer, encoding, or enum ordinal.

## Inline Conditional

`iif(condition, when_true, when_false)` requires a Boolean condition and two
branches that resolve to a common result type. Only the selected branch runs:

```dq
var count : int = 0
var label : rostr = iif(count == 0, "empty", "nonempty")
```

## Calls and Evaluation

Calls use parentheses and pass arguments positionally, in declaration order.
Named arguments are not supported. Arguments are evaluated before the function is entered.
Avoid relying on a relative evaluation order between independent expressions
with side effects unless a construct explicitly specifies that order.

Built-ins such as `Len`, `SizeOf`, `OffsetOf`, `Ord`, `Round`, `Defined`, and
`FirstInt` use call-like syntax but follow their own type and constant-expression
rules. [Compiler Directives](190-compiler-directives.md) explains compile-time
existence tests and optional constants.

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

Modify-assignment statements are `+=`, `-=`, `*=`, `/=`, `<<=`, `>>=`, `&=`,
`|=`, `=xor=`, `=div=`, `=rem=`, and `=mod=`. A word operator uses the leading
`=` spelling shown here. These are statements outside the precedence table.
