# Expressions and Operators

This page defines expression behavior. See the [Expressions guide](../language/expressions.md)
for a compact operator tour.

## Evaluation and Values

Expressions produce typed values or designate storage. Assignment is a
statement, not an expression, so assignments cannot be chained or embedded in a
condition.

Function arguments are evaluated before the call. Programs should not rely on
an unspecified relative order between independent subexpressions that have side
effects.

## Arithmetic

`+`, `-`, and `*` operate on compatible numeric operands. `/` is floating-point
division even when both operands are integers. `IDIV` and `IMOD` perform integer
division and remainder.

```dq
var ratio : float = 3 / 2
var quotient : int = 7 IDIV 3
var remainder : int = 7 IMOD 3
```

Integer overflow, division by zero, signed division corner cases, and floating
behavior follow the compiler/runtime checks and target representation. Use an
explicit destination type when width matters.

## Comparison

`==` and `<>` test equality and inequality. `<`, `<=`, `>`, and `>=` perform
ordering where defined. Operands must be compatible; there is no numeric-to-bool
coercion.

Object references and pointers compare identity/address. General array sequence
equality is not implicitly defined.

## Boolean Logic

`not`, `and`, and `or` accept only `bool`. `and` and `or` short-circuit: the
right operand is evaluated only when required.

## Integer and Bitwise Operators

`NOT`, `AND`, `OR`, `XOR`, `SHL`, and `SHR` operate on integer values. They are
distinct from the lowercase Boolean operators.

## Unary Operators

`-` negates a numeric value, `not` negates a Boolean, `NOT` complements integer
bits, `&` obtains an address, and `^` dereferences a pointer when used in its
postfix form.

## Casts

Type-call syntax requests an explicit conversion:

```dq
var small : uint8 = uint8(value)
var typed : ^byte = ^byte(raw)
```

A cast is valid only when the compiler defines a conversion between the source
and destination categories. It does not make an invalid pointer, ordinal, or
encoding valid.

Object runtime testing and casting use `is`, `TryCast`, or `tryfrom`; see
[Objects and Properties](objects-and-properties.md).

## Inline Conditional

`iif(condition, when_true, when_false)` evaluates the condition and only the
selected result expression. The condition must be Boolean, and both result
expressions must resolve to a common result type.

## Index, Slice, and Member Access

`value[index]` indexes arrays, strings, pointers, or a default indexed property
when defined. `value[start:end]` is a half-open slice. `value.member` selects a
field, method, property, or built-in member. A structure pointer may use member
access without writing an explicit dereference.

## Calls and Named Arguments

Calls use parentheses. Named arguments use `name = expression` and must identify
parameters of the selected overload. Positional arguments cannot ambiguously
skip required parameters.

```dq
Connect(host = "localhost", port = 8080)
```

## Precedence

From tighter to looser binding, the operator groups are:

1. primary expressions;
2. calls, member access, indexing, slicing, and postfix dereference;
3. address-of, unary minus, and integer `NOT`;
4. shifts (`<<`, `SHL`, `>>`, `SHR`);
5. integer `AND`;
6. integer `OR` and `XOR`;
7. `/`, `IDIV`, and `IMOD`;
8. multiplication;
9. addition and subtraction;
10. comparisons and `is`;
11. Boolean `not`;
12. Boolean `and`;
13. Boolean `or`.

Parentheses should be used whenever mixed word/symbol operators would obscure
intent. In particular, do not infer C precedence for DQ word operators.
