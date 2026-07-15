# Expressions

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
with `IDIV` and `IMOD`.

```dq
var a : int = 10 IDIV 3
var b : int = 10 IMOD 3
```

The `/` division operator always produces a floating point result.
Floating point values are not converted automatically to integers, explicit
conversion functions required: `Round`, `Ceil` or `Floor`

## Comparison

Comparison operators produce `bool`.

```dq
a == b
a != b
a <> b
a < b
a <= b
a > b
a >= b
```

`<>` and `!=` are both accepted for inequality, `<>` is preferred.

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

Bitwise operators use uppercase words.

```dq
var masked : uint = value AND 0xFF
var flags : uint = a OR b
var flipped : uint = NOT flags
```

The bitwise operators are:

- `AND`
- `OR`
- `XOR`
- `NOT`
- `SHL` or `<<`
- `SHR` or `>>`

Bitwise operators have higher precedence than logical operators and arithmetic
operators.
Bitwise `AND`, `OR` and `XOR` require a leading `=` when used in modify-assign statements:

```dq
var i : int = 0xFF00
i =OR= (1 << 3)
i =AND= NOT (1 << 12)
```

Shift modify-assignment uses `<<=` and `>>=`.

```dq
i <<= 1
i >>= 1
```

## Casts

Explicit casts use type-call syntax.

```dq
var p : pointer = &value
var ip : ^int = ^int(p)
var f : float64 = float64(value)
```

Only conversions accepted by the type checker are valid casts.

## Inline If

DQ provides an inline conditional macro named `iif`.

```dq
var text : cstring = iif(ptr == null, "null", "not null")
```

The first argument must be `bool`. The second and third arguments must be
compatible with the expected result type.

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

`&` takes the address of an addressable value.

```dq
var value : int = 10
var p : ^int = &value
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
| 3 | `&expr`, `-expr`, `NOT expr` | Address-of, unary minus, bitwise NOT |
| 4 | `<<`, `SHL`, `>>`, `SHR` | Bit shifts |
| 5 | `AND` | Bitwise AND |
| 6 | `OR`, `XOR` | Bitwise OR, bitwise XOR |
| 7 | `/`, `IDIV`, `IMOD` | Division, integer division, integer modulo |
| 8 | `*` | Multiplication |
| 9 | `+`, `-` | Addition, subtraction |
| 10 | `==`, `!=`, `<>`, `<`, `<=`, `>`, `>=`, `is` | Comparison and object type test |
| 11 | `not` | Logical NOT |
| 12 | `and` | Logical AND |
| 13 | `or` | Logical OR |

This order is intentionally different from C in the bitwise levels. For
example, `value AND mask <> 0` is parsed as `(value AND mask) <> 0`.

Assignment operators are statements, not expressions, so they are outside the
precedence table. Supported modify-assignment forms include `+=`, `-=`, `*=`,
`/=`, `<<=`, `>>=`, `=IDIV=`, `=IMOD=`, `=AND=`, `=OR=`, and `=XOR=`.

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
