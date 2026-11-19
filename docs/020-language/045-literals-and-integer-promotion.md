# Literals and Integer Promotion

DQ applies one set of numeric rules to both runtime code generation and
compile-time constant evaluation. The model is designed for predictable
machine-level behavior on 32-bit and 64-bit targets—especially RISC
architectures—without forcing type suffixes or redundant casts onto ordinary
expressions.

```dq
use print

func *Main():
    var reg : uint32 = 0x80000000
    var half : uint32 = reg div 2         // 0x40000000: 2 starts as uint, so div stays unsigned
    var signed_diff : int = 3 - 5         // -2: subtraction switches calculation mode to signed int
    var rxbuf : [2]byte = [0xFF, 0x04]
    var word : uint16 = (rxbuf[0] << 8) | rxbuf[1] // 0xFF04: bytes widen to native width first
    PrintLn("0x{:X} {} 0x{:04X}", [half, signed_diff, word])
endfunc
```

## Numeric Literals

### Non-Negative Integer Literals

A non-negative decimal (`0`, `42`, `5000000000`) or hexadecimal (`0xFF`,
`0x80000000`, `0x100000000`) literal without an explicit type cast starts as
unsigned:

| Target pointer width | Literal magnitude | Starting type |
| --- | --- | --- |
| 64-bit | `0` .. `0xFFFFFFFFFFFFFFFF` | `uint` (`uint64`) |
| 32-bit | `0` .. `0xFFFFFFFF` | `uint` (`uint32`) |
| 32-bit | `0x100000000` .. `0xFFFFFFFFFFFFFFFF` | `uint64` |

Starting non-negative literals as `uint` keeps unsigned expressions unsigned
when combined with plain numbers:

```dq
var reg : uint32 = 0x80000000
var nonzero : bool = reg > 0          // true: unsigned comparison
var half : uint32 = reg div 2         // 0x40000000: unsigned division
var shifted : uint32 = (reg | 1) >> 1 // 0x40000000: logical right shift
var cleared : uint32 = reg & ~(1 << 5) // stays native 32-bit on 32-bit targets
var all_ones : uint = ~0              // unsigned all-ones mask at native width
```

Signed expressions still work without casts because combining `uint` with a
signed operand promotes to signed `int`:

```dq
var offset : int = -10
var moved : int = offset + 5          // -5: int + uint -> int
var positive : bool = offset > 0      // false: int > uint -> signed comparison
```

### Negated Integer Literals

Applying unary `-` switches the calculation mode to signed. When `-` is applied
directly to a plain non-negative literal:

| Target pointer width | Negated value | Result type |
| --- | --- | --- |
| 64-bit | `-9223372036854775808` .. `0` | `int` (`int64`) |
| 32-bit | `-2147483648` .. `0` | `int` (`int32`) |
| 32-bit | `< -2147483648` (for example `-3000000000`) | `int64` |

On a 32-bit target, a directly negated plain literal such as `-3000000000` or
`-2147483649` promotes to `int64` so that `var x : int64 = -3000000000` keeps
its exact value instead of wrapping in 32 bits. An explicitly typed negation
such as `-uint32(3000000000)` respects the explicit type and negates at native
32-bit `int`.

### Floating-Point Literals and Assignment Context

A literal containing a decimal point or exponent (`2.5`, `1e6`, `0.`) has the
target's default `float` type (`float32` on single-precision FPU targets,
`float64` otherwise).

You do not need type hints like `uint8(3)` or `2.0` when initializing or
assigning variables, passing arguments, or returning values. The compiler
converts the literal or expression to the destination type:

```dq
var limit : uint8 = 255
var scale : float = 2
var wide : uint64 = 0x100000000
var neg_wide : int64 = -3000000000
```

## Minimum Calculation Width

DQ never performs integer calculations narrower than the target's native integer
width:

- **32 bits** (`uint32` / `int32`) on 32-bit targets.
- **64 bits** (`uint64` / `int64`) on 64-bit targets.

Operands wider than the native width (`uint64`, `int64` on a 32-bit target)
keep their 64-bit calculation width.

Before an integer operation runs, any narrower operand (`uint8`/`byte`, `int8`,
`uint16`, `int16`, and on 64-bit targets `uint32`/`int32`) is extended to the
calculation width:

- **Unsigned operands are zero-extended** (high bits filled with `0`).
- **Signed operands are sign-extended** (sign bit replicated into high bits).

```dq
var a : uint8 = 250
var b : uint8 = 10
var sum : int = a + b        // 260: both bytes zero-extend before addition
var high : byte = 0xFF
var low : byte = 0x04
var word : uint16 = (high << 8) | low // 0xFF04: high extends before shifting
```

The destination type of an assignment does not widen a calculation
retroactively; it only narrows or reinterprets the final result when stored.

## Integer and Floating-Point Promotion Rules

For binary operations, DQ promotes operands along the chain
`uint -> int -> int64 / float` (with `uint64` preserved when both operands are
unsigned and at least one is 64-bit):

| Left / Right operand types | Calculation type |
| --- | --- |
| Both unsigned, width `<= native` | `uint` (native width) |
| Both unsigned, either is `uint64` | `uint64` |
| Both signed, width `<= native` | `int` (native width) |
| Both signed, either is `int64` | `int64` |
| One unsigned and one signed, width `<= native` | `int` (native width) |
| One unsigned and one signed, either is 64-bit | `int64` |
| One integer and one floating-point value | The floating-point operand's type |
| Two floating-point values | The wider floating-point type |

### Operator-Specific Rules

| Operators | Signedness and width rule |
| --- | --- |
| `+`, `*`, `div`, `rem`, `mod`, `&`, `\|`, `xor` | Common type from the table above. Unsigned-only stays unsigned. |
| `-` (binary subtraction) | Always **signed** (`int` or `int64`) at the common calculation width, even when both operands are unsigned. |
| `-x` (unary negation) | Always **signed** (`int` or `int64`) at `max(native, width(x))` (or `int64` for a directly negated 32-bit-overflowing plain literal). |
| `~x` (bitwise complement) | Promoted to `max(native, width(x))`, preserving `x`'s signedness. |
| `x << n`, `x >> n` (shifts) | Left operand `x` is promoted to `max(native, width(x))`, preserving `x`'s signedness (`>>` is logical for unsigned, arithmetic for signed). The count `n` does not widen `x` or change its signedness. |
| `/` (floating-point division) | Always floating point. Dividing two integers converts both to the target's default `float`. |
| `==`, `<>`, `<`, `<=`, `>`, `>=` | Operands harmonize using the table above, then compare at that common type (unsigned if both unsigned, signed if either signed, float if either float). |

When an unsigned integer (including `uint64`) converts to floating point—whether
via `/`, mixed integer/float arithmetic, assignment, or an explicit cast—DQ
converts its **unsigned** value (`uint64(0xFFFFFFFFFFFFFFFF)` becomes
`18446744073709551616.0`, not `-1.0`).

## Overflow, Wraparound, and Storage Narrowing

DQ does not perform runtime integer range checking. Fixed-width integer
arithmetic (`+`, `-`, `*`, unary `-`) uses exact two's-complement machine
wraparound.

- **Same-width `int` ↔ `uint` conversion** is a pure bit-copy (zero cost).
- **Narrowing into storage** (`byte`/`uint8`, `int8`, `uint16`, `int16`,
  `uint32`, `int32`) truncates to the low bits. Signed storage interprets the
  retained bits in two's complement.

```dq
var x : uint8 = 250
var y : uint8 = 10
var stored_u8 : uint8 = x + y  // 260 -> low 8 bits -> 4
var stored_i8 : int8 = x + y   // 260 -> low 8 bits -> 4
var wrapped : uint8 = 0 - 1    // -1 -> low 8 bits -> 255
```

### High-Bit Unsigned Subtraction and Bit-Copy Identity

Because subtraction switches the calculation mode to signed `int` (or `int64`)
at the common calculation width, `u - 1` produces a signed value whose bit
pattern is the exact machine subtraction result:

```dq
var u : uint = 0
var u2 : uint = u - 1          // all-ones (~0) via bit-copy
```

The identity below always holds for every integer type `u` (signed or unsigned,
narrow or wide, 32-bit or 64-bit):

```dq
uint(uint(u) - uint(1)) == uint(u - 1)
```

When operating near the top of the unsigned range (bit 31 set on 32-bit, or
bit 63 set on 64-bit), remember that the result of `u - 1` is signed. If you
immediately follow it with a signedness-sensitive operator (`>>`, `div`, `rem`,
`mod`, `<`, `<=`, `>`, `>=`), cast it back to `uint` (or `uint64`) first:

```dq
var addr : uint = 0x80000002
var shifted : uint = uint(addr - 1) >> 1 // logical shift on 0x80000001
```

### Mixing Unsigned and Signed at Equal Width (32-Bit vs 64-Bit)

Mixing an unsigned value and a signed value at the same calculation width
converts the unsigned operand to signed by bit-copy—it does **not** silently
widen 32-bit operations to 64-bit on 32-bit targets.

As a consequence, `uint32(0xFFFFFFFF) > int32(-1)` is:

- **`true` on 64-bit targets**, where both narrow operands first extend to
  native `int64` (`4294967295 > -1`).
- **`false` on 32-bit targets**, where 32 bits is already native width and both
  compare as `int32` (`-1 > -1`).

When a calculation must accommodate the full `uint32` range together with
negative numbers portably across 32-bit and 64-bit targets, widen explicitly to
`int64`:

```dq
var u : uint32 = 0xFFFFFFFF
var s : int32 = -1
var portable : bool = int64(u) > int64(s) // true on both 32-bit and 64-bit
```

## Modify-Assignment and Pointer Offsets

Modify-assignment statements (`+=`, `-=`, `*=`, `=div=`, `=rem=`, `=mod=`,
`&=`, `|=`, `=xor=`, `<<=`, `>>=`) promote and calculate with the same rules as
the corresponding binary expression, then narrow the result back into the
destination's storage width:

```dq
var a : uint8 = 200
a =div= 300        // calculated at native width: 200 div 300 = 0 (not 200 div 44)

var b : uint8 = 10
b =div= -3         // calculated as signed native int: 10 div -3 = -3 -> uint8(253)

var c : uint8 = 1
c <<= 8            // calculated at native width: 1 << 8 = 256 -> uint8(0)
```

Pointer arithmetic (`p + n`, `p - n`, `p += n`, `p -= n`) and pointer indexing
(`p[n]`) extend narrow integer offsets according to their signedness
(zero-extending unsigned types like `uint8`/`uint16`, sign-extending signed
types like `int8`/`int16`) before applying the element offset:

```dq
var buf : [256]byte
var base : ^byte = %buf[0]
var off : uint8 = 200
var elem : ^byte = base + off  // advances +200 bytes (zero-extended, never -56)
```

## Compile-Time Constant Evaluation

Compile-time evaluation in `const`, `#define`, `#if`, and folded subexpressions
uses the **exact same rules** as runtime code generation:

- Literals start with the same `uint` / `uint64` / `int` / `int64` types.
- Intermediate operations run at `max(native, operand_widths)` with the same
  signedness rules and two's-complement wraparound.
- Unsigned-to-float conversions preserve unsigned magnitude.
- Shift counts must be in `0 .. calculation_width - 1` (so `1 << 33` is valid on
  64-bit targets where native width is 64, and rejected on 32-bit targets unless
  the left operand is `uint64(1)`).
- Untyped `#define` constants keep the expression's resulting type (`#define N = 5`
  is `uint`; `#define D = -5` is `int`).

```dq
const UMAX : uint64 = 0xFFFFFFFFFFFFFFFF
const HALF_F : float = UMAX / 2       // matches runtime UMAX / 2 exactly
const WORD : uint16 = (byte(0xFF) << 8) | byte(0x04) // 0xFF04
const MASK : uint = ~0                // matches runtime ~uint(0)
```

## Overload Resolution and `anyvalue`

When resolving overloaded functions, DQ prefers conversions in this order:

1. **Exact type match**
2. **Integer-to-integer conversion** (for example `uint` → `int` or `uint8` → `int`)
3. **Integer-to-float conversion** (`uint` → `float`)
4. **`anyvalue` boxing**

Because integer-to-integer conversion ranks ahead of integer-to-float, calling
an overloaded function `Pick(int)` / `Pick(float)` with a non-negative literal
such as `Pick(5)` selects `Pick(int)`, while `Pick(2.5)` selects `Pick(float)`.

When a non-negative literal is boxed directly into `anyvalue` (for example
`var v : anyvalue = 123`), it is stored as an unsigned native integer: both
`v.IsInt()` and `v.IsUint()` return `true`, and `v.AsInt(0)` / `v.AsUint(0)`
return `123`.
