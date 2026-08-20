# Expressions

This page shows common expression forms. For exact typing, evaluation, and
precedence rules, see [Expressions and Operators](../reference/expressions-and-operators.md).

DQ expressions are statically type-checked. Assignment is a statement, not an
expression.

## Arithmetic

DQ supports the usual arithmetic operators.

```dq
var a : int = 10 + 2 * 3
var b : int = a - 1
var f : float = 3 / 2
```

The `/` operator follows DQ arithmetic rules intended to make common mixed
numeric expressions behave naturally. Integer division and modulo are written
with `div` and `mod`.

```dq
var a : int = 10 div 3
var b : int = 10 mod 3
```

The `/` division operator always produces a floating point result.
Floating point values are not converted automatically to integers, explicit
conversion functions required: `Round`, `Ceil` or `Floor`

## Comparison

Comparison operators produce `bool`.

```dq
a == b
a <> b
a < b
a <= b
a > b
a >= b
```

`<>` tests inequality. The C-style `!=` spelling is not accepted.

## Logical Operators

Logical operators use lowercase words.

```dq
if ready and not failed:
    Run()
endif
```

The logical operators are:

- `and`
- `or`
- `not`

Operands must be `bool`.

## Bitwise Operators

Bitwise operators use symbols except for lowercase `xor`.

```dq
var masked : uint = value & 0xFF
var flags : uint = a | b
var flipped : uint = ~flags
```

The bitwise operators are:

- `&`
- `|`
- `xor`
- `~`
- `<<`
- `>>`

Bitwise operators have higher precedence than logical operators and arithmetic
operators.
Symbolic bitwise modify-assignment uses `&=` and `|=`. The word operator keeps
DQ's leading-`=` form, `=xor=`:

```dq
var i : int = 0xFF00
i |= (1 << 3)
i &= ~(1 << 12)
i =xor= 1
```

Shift modify-assignment uses `<<=` and `>>=`.

```dq
i <<= 1
i >>= 1
```

## Casts

Explicit casts use type-call syntax or `expression as Type`.

```dq
var p : pointer = %value
var ip : ^int = ^int(p)
var f : float64 = float64(value)
var ip2 : ^int = p as ^int
```

Both forms use the same conversion rules. An `as` cast binds at the comparison
level and is not chainable; parenthesize it before member access or another
comparison-level operation.

## Inline If

DQ provides an inline conditional macro named `iif`.

```dq
var text : cstring = iif(ptr == null, "null", "not null")
```

The first argument must be `bool`. The second and third arguments must be
compatible with the expected result type.

## Optional Compile-Time Constants

`FirstInt`, `FirstFloat`, and `FirstBool` select the first declared DQ constant
from a list of optional identifiers. The final argument is a required constant
fallback expression.

```dq
const BUFFER_SIZE : int = FirstInt(BOARD_BUFFER_SIZE, DEFAULT_BUFFER_SIZE, 256)
const SCALE : float = FirstFloat(BOARD_SCALE, 1.0)
const LOGGING : bool = FirstBool(BOARD_LOGGING, false)
```

Optional identifiers are resolved from left to right in the lexical scope at
the call. Missing identifiers are skipped. The first identifier that resolves
must name a constant convertible to the intrinsic's result type; a variable or
an incompatible constant is an error. The fallback must also be a convertible
constant expression and is validated even when an earlier constant is found.

The result types are fixed: `FirstInt` returns `int`, `FirstFloat` returns
`float`, and `FirstBool` returns `bool`. These intrinsics are always evaluated
during compilation and can be used in ordinary expressions as well as `#if`
and `#elif` conditions. Optional identifiers search DQ lexical scopes, not the
`@def` namespace.

## Definition Test

`Defined(NAME)` is a compile-time intrinsic that returns `true` when `NAME`
exists in the preprocessor-definition scope and `false` otherwise. Like
`#ifdef`, a bare name searches `@def`, regardless of the definition's value:

```dq
#if Defined(FEATURE_A) or Defined(FEATURE_B)
    const FEATURE_AVAILABLE : bool = true
#endif
```

A namespace-qualified argument tests a DQ value symbol instead, for example
`Defined(@.MODULE_CONSTANT)` or `Defined(@module.CONSTANT)`. The result is a
constant `bool`, so `Defined` can also be used in ordinary code.

## Object Type Test

The `is` operator checks whether an object reference is compatible with an
object type.

```dq
if obj is OChild:
    // obj is an OChild or derives from OChild
endif
```

`is` returns `false` for `null`.

## Address and Dereference

`%` takes the address of an addressable value.

```dq
var value : int = 10
var p : ^int = %value
```

`^` dereferences a typed pointer.

```dq
p^ = 11
```

Pointers to structs are automatically dereferenced for member access.

```dq
point_ptr.x = 10
```

## Indexing

Arrays, strings, C strings, and typed pointers support indexing.

```dq
var a : [3]int = [1, 2, 3]
var second : int = a[1]
```

For typed pointers, `p[i]` performs pointer indexing and returns a pointer value.
It does not dereference like C's `p[i]`.

```dq
var p : ^char = text.pchar
var next : ^char = p[1]
var ch : char = p[1]^
```

## Slicing

Arrays and strings support slicing.

```dq
var a : [*]int = [1, 2, 3, 4]
var middle : []int = a[1:3]
var all : []int = a[:]
var tail : []int = a[2:]
```

The special `$end` (= length) and `$last` (= length - 1) values can be used by
some indexing and mutation APIs.

```dq
arr.Insert($end, 99)
text.Insert($end, "!")
```

## Operator Precedence

Precedence is listed from highest to lowest.

| Level | Operators and syntax | Meaning |
| --- | --- | --- |
| 1 | literals, identifiers, `@namespace.name`, `(...)`, `[...]`, `Type(expr)`, `new`, builtins such as `Len(...)`, `SizeOf(...)`, `iif(...)` | Primary expressions, array literals, casts, allocation, builtin forms |
| 2 | `expr(args...)`, `expr.member`, `expr[index]`, `expr[start:end]`, `ptr[index]`, `ptr^` | Calls, member access, indexing, slicing, pointer indexing, pointer dereference |
| 3 | `%expr`, `-expr`, `~expr` | Address-of, unary minus, bitwise NOT |
| 4 | `<<`, `>>` | Bit shifts |
| 5 | `&` | Bitwise AND |
| 6 | `|`, `xor` | Bitwise OR, bitwise XOR |
| 7 | `/`, `div`, `mod` | Division, integer division, integer modulo |
| 8 | `*` | Multiplication |
| 9 | `+`, `-` | Addition, subtraction |
| 10 | `==`, `<>`, `<`, `<=`, `>`, `>=`, `is`, `as` | Comparison, object type test, and explicit cast |
| 11 | `not` | Logical NOT |
| 12 | `and` | Logical AND |
| 13 | `or` | Logical OR |

This order is intentionally different from C in the bitwise levels. For
example, `value & mask <> 0` is parsed as `(value & mask) <> 0`.

Assignment operators are statements, not expressions, so they are outside the
precedence table. Supported modify-assignment forms include `+=`, `-=`, `*=`,
`/=`, `<<=`, `>>=`, `&=`, `|=`, `=div=`, `=mod=`, and `=xor=`.

## Member Access

`.` accesses struct fields, object fields, methods, properties, enum values,
and namespace members.

```dq
point.x = 1
object.Method()
box.property = 10
var c : NColor = NColor.red
```

Inside object methods, members can be used without `self.`.

## Function Calls

Functions and function references are called with parentheses.

```dq
var x : int = Add(1, 2)
var y : int = callback(x)
```

Object methods are called through object values.

```dq
obj.Update()
```

## Object Allocation

`new` allocates an object on the heap and returns an object reference.

```dq
var obj : OThing = new OThing(1, "name")
```

Embedded object allocation uses `<-`.

```dq
var obj <- OThing(1, "stack or global storage")
```

See [Objects](objects.md) and [Memory and Pointers](memory-and-pointers.md).
