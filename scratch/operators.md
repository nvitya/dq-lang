# DQ Operator Precedence

Precedence is listed from highest to lowest.

| Precedence | Operators / syntax | Meaning | Associativity |
|------------|--------------------|---------|---------------|
| 1 | literals, identifiers, `@namespace.name`, `(...)`, `[...]`, `Type(expr)`, `new`, builtins such as `Len(...)`, `SizeOf(...)`, `iif(...)` | primary expressions, array literals, parenthesized expressions, explicit casts, builtin forms | n/a |
| 2 | `expr(args...)`, `expr.member`, `expr[index]`, `expr[start:end]`, `expr[start::end]`, `ptr[index]`, `ptr^` | calls, member access, indexing, slicing, pointer indexing, pointer dereference | left-to-right chain |
| 3 | `&expr`, `-expr`, `NOT expr` | address-of, unary minus, bitwise NOT | right-to-left |
| 4 | `<<`, `SHL`, `>>`, `SHR` | bit shifts | left-to-right |
| 5 | `AND` | bitwise AND | left-to-right |
| 6 | `OR`, `XOR` | bitwise OR, bitwise XOR | left-to-right |
| 7 | `/`, `IDIV`, `IMOD` | division, integer division, integer modulo | left-to-right |
| 8 | `*` | multiplication | left-to-right |
| 9 | `+`, `-` | addition, subtraction | left-to-right |
| 10 | `==`, `!=`, `<>`, `<`, `<=`, `>`, `>=`, `is` | comparison, object type test | non-associative |
| 11 | `not` | logical NOT | right-to-left |
| 12 | `and` | logical AND | left-to-right |
| 13 | `or` | logical OR | left-to-right |

## Notes

- Assignment is statement syntax, not expression syntax. It is parsed outside
  the expression precedence ladder. The Assignment operators are: `=`, `+=`, `-=`, `*=`, `/=`, `<<=`, `>>=`,
  `=IDIV=`, `=IMOD=`, `=AND=`, `=OR=`, `=XOR=`.
- Only one comparison operator is parsed per comparison expression. Use
  parentheses or logical operators to combine comparisons.
- Logical operators are lowercase: `not`, `and`, `or`.
- Bitwise/integer operators are uppercase where written as words: `NOT`,
  `AND`, `OR`, `XOR`, `IDIV`, `IMOD`, `SHL`, `SHR`.
- The current implementation gives shifts and bitwise operators higher
  precedence than arithmetic division, multiplication, addition, and
  subtraction.
