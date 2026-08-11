# Functions Implemented in Assembly Language

This is the practical assembly guide. The language contract is summarized in
[Assembly Functions](../reference/assembly-functions.md).

DQ supports two forms of assembly function:

| Attributes | Meaning |
| --- | --- |
| `[[asm]]` | Define a complete function in target assembly language |
| `[[inline, asm]]` | Define a typed inline-assembly template that is emitted at each call site |

Both forms use a colon-delimited body closed by `endfunc`. A declaration without
a body and a brace-delimited body are not allowed.

## Complete Assembly Functions

With `[[asm]]`, the assembly body implements the complete function, including
returning to its caller.

```dq
function Return42() -> int [[asm]]:
    mov     eax, 42
    ret
endfunc
```

This example uses x86-64 Intel syntax. DQ uses the unified
`instruction destination, source` operand order and does not support AT&T
syntax.

The declared DQ signature determines the function's calling ABI. The assembly
implementation is responsible for obeying that ABI: it must read arguments from
the correct registers or stack locations, place a return value in the correct
location, preserve all required registers, and return to the caller. Parameter
names and `result` are not available as operands in a complete assembly
function.

The compiler emits an `[[asm]]` function as naked and non-inline. Consequently,
it does not generate a prologue, epilogue, or DQ `return` operation. `[[asm]]`
cannot be combined with `[[external]]`, `[[abstract]]`, or `[[always_inline]]`.

## Inline Assembly Functions

Combining `inline` and `asm` defines an assembly operation with a DQ function
signature:

```dq
function Add(a : int, b : int) -> int [[inline, asm]]:
    lea     $result, [$a + $b]
endfunc

var sum : int = Add(19, 23)
```

On x86-64, inline assembly uses Intel syntax. ARM, AArch64, and RISC-V use their
native assembly syntax. DQ does not reorder instruction operands or translate
architecture-specific syntax.

An inline assembly function does not produce a separately callable function.
Every direct call emits the assembly template at the call site. It therefore
cannot be converted to a function reference or have its address taken.

Inline assembly functions must be ordinary module-level functions. Methods and
special functions such as `*Main` and `*ModuleInit` are not supported. They also
cannot use `[[external]]`, `[[noinline]]`, `[[weak]]`, `[[section]]`, or native
export attributes. `[[overload]]` is allowed, and inline assembly definitions in
a module's public interface remain available to importing modules.

### Named Operands

Use `$result` for the return value and `$parameter` for a function parameter.
The compiler replaces these names with the target assembly operands allocated
for the call.

```dq
function ReverseBits(value : uint32) -> uint32 [[inline, asm]]:
    rbit    $result, $value
endfunc
```

`$result` is valid only when the function has a return type. An unknown name is
a compile error. A literal dollar sign in assembly text must be written as `$$`
because the underlying assembly template syntax reserves `$`.

Without an operand hint, each parameter is a register input. Integer, Boolean,
character, enumeration, pointer, and floating-point scalar operands are
supported. The result is always a register output and must also have a supported
scalar type. Integer and pointer values use the target's general-purpose
register class; floating-point values use its native floating-point register
class. Availability of a particular floating-point register class may depend on
the selected CPU features.

### Operand Hints

An inline assembly function may have one trailing hint list immediately before
`endfunc`:

```dq
function MulImmediate(value : int32, factor : int32) -> int32 [[inline, asm]]:
    imul    $result, $value, $factor
    [[immediate(factor), clobber(flags)]]
endfunc
```

The hint list is DQ metadata, not assembly text. It supports these clauses:

| Hint | Meaning |
| --- | --- |
| `immediate(parameter, ...)` | Require compile-time integer-like arguments and use them as immediate operands |
| `memread(parameter, ...)` | Describe memory read through typed pointer or readable reference arguments |
| `memwrite(parameter, ...)` | Describe memory written through typed pointer or writable reference arguments |
| `memreadwrite(parameter, ...)` | Describe memory both read and written through typed pointer or read/write reference arguments |
| `clobber(resource, ...)` | Declare registers or other target resources modified by the assembly |

A clause can name multiple operands. Clause and argument order do not affect the
meaning. Each parameter may occur in at most one hint; parameters omitted from
the list remain register inputs.

An `immediate` argument must be a compile-time constant at every call site. Its
type must be an integer, Boolean, character, or enumeration type.

Memory operands must be typed pointers or references so that the compiler knows
the accessed element type. The reference mode must permit the described access:
`refin` can be read but not written, `refout` can be written but not read, and
`ref` supports all three memory hints. The programmer remains responsible for
passing a valid address.

The special clobber `memory` is supported on every inline-assembly target and can
be used for a compiler barrier:

```dq
function CompilerBarrier() [[inline, asm]]:
    [[clobber(memory)]]
endfunc
```

`clobber(flags)` declares the condition-code state on x86, ARM, and AArch64
targets. It is not available on RISC-V. A physical register can also be named as
a clobber, using its lowercase target name or a recognized conventional alias:

```dq
function AddOneThroughRax(value : int) -> int [[inline, asm]]:
    mov     rax, $value
    lea     $result, [rax + 1]
    [[clobber(rax)]]
endfunc
```

Declare every memory access and every modified resource that is not already an
output. The optimizer can only account for effects described by the signature,
memory hints, and clobbers.

### Memory Examples

Memory hints make a named parameter expand as an assembly memory operand rather
than a register containing an address.

```dq
function Load32(address : ^int32) -> int32 [[inline, asm]]:
    mov     $result, $address
    [[memread(address)]]
endfunc

function Store32(address : ^int32, value : int32) [[inline, asm]]:
    mov     $address, $value
    [[memwrite(address)]]
endfunc

function AddTo32(address : ^int32, value : int32) [[inline, asm]]:
    add     $address, $value
    [[memreadwrite(address), clobber(flags)]]
endfunc
```

These examples use x86-64 Intel syntax. An instruction may still require an
explicit size qualifier when its other operands do not determine the access
width:

```dq
function Inc32(address : ^int32) [[inline, asm]]:
    inc     dword ptr $address
    [[memreadwrite(address), clobber(flags)]]
endfunc
```

## Source Processing

Assembly bodies participate in normal DQ source processing:

- `//` and `/* ... */` comments are removed;
- compiler directives are processed, and inactive conditional branches are
  omitted;
- `endfunc` closes the body only when it is active and outside a comment;
- comments and skipped branches preserve the separation between assembly tokens
  and instructions.

This permits target-specific implementations selected with conditional
compilation:

```dq
function Pause() [[inline, asm]]:
    #ifdef TARGET_HAS_PAUSE
        pause
    #else
        nop
    #endif
endfunc
```

Inside an assembly body, `#` starts a DQ directive only when it is the first
non-whitespace, non-comment character on a physical source line. After assembly
text has started on the line, `#` remains target assembly syntax. For example,
the immediate in `mov r0, #1` is preserved. Keep an instruction and any
line-leading immediate operand on the same physical line; a continuation line
starting with `#` would be parsed as a directive.

Because DQ comments are removed before assembly is parsed, do not use `//` or
`/* ... */` when those characters are intended as target assembler syntax. A
line-leading `#` likewise cannot be used as a target assembler comment. In a
complete `[[asm]]` body, `[[...]]` has no special meaning. In an
`[[inline, asm]]` body, a line beginning with `[[` is parsed as the single
trailing operand-hint list, after which only comments, whitespace, and `endfunc`
may follow.

## Supported Inline-Assembly Targets

Typed inline assembly is supported for these target identifiers:

- x86-64: `x86_64`;
- ARM: `arm_m0`, `arm_m3`, `arm_m4`, `arm_m4f`, `arm_m33`, `arm_m7f`, `arm`,
  and ARM-A identifiers beginning with `arm_a`;
- AArch64: `aarch64`, `arm64`;
- RISC-V 32-bit: `riscv32`, `rv32i`;
- RISC-V 64-bit: `riscv64`, `rv64g`.

Complete `[[asm]]` functions are not restricted to this list, because their
signatures do not require the compiler to construct typed inline-assembly
constraints. In either form, the instructions themselves must be valid for the
selected target and CPU features.
