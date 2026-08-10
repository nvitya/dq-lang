# Inline-Asm support for dq

For the embedded applications it is frequently required to support special CPU instructions, like SSAT, CLZ or RBIT.

In order to reach the maximal performance these special instructions should be integrated into the normal program flow,
including register optimization.

## C definitions

See the cmsis_gcc.h for C definitions of such instructions.

__STATIC_FORCEINLINE uint32_t __RBIT(uint32_t value)
{
  uint32_t result;
  __ASM ("rbit %0, %1" : "=r" (result) : "r" (value) );
  return result;
}

## DQ Version

In DQ a human-friendly register usage hinting is required.

### Assembly dialect

The assembly body uses the target architecture's established dialect. X64 uses Intel syntax, while ARM Cortex-M,
AArch64, RV32I and RV64G use their native syntax. DQ does not reorder operands, add architecture-specific sigils or
otherwise translate the assembly language.

Inline assembly recognizes the current target identifiers and their DQ aliases: `x86_64`; the Cortex-M identifiers
`arm_m0`, `arm_m3`, `arm_m4`, `arm_m4f`, `arm_m33` and `arm_m7f`; `arm` and `arm_a*`; `aarch64` and `arm64`;
`riscv32` and `rv32i`; and `riscv64` and `rv64g`. Integer and pointer operands use LLVM's `r` constraint. Floating
operands use `x` on x86-64, `w` on ARM/AArch64, and `f` on RISC-V. The selected LLVM CPU and feature set determine
whether the requested register class is available.

`clobber(memory)` is available on every supported target. `clobber(flags)` names the condition codes on x86 and ARM,
but is rejected on RISC-V, which has no corresponding condition-code register. Explicit lowercase scalar register
names and the conventional ARM and RISC-V aliases are accepted according to the selected architecture.

After the DQ source processing described below, the active assembly text is passed through unchanged except for
`$symbolname` substitution. `$symbolname` references a named result or function argument, and the compiler translates
it to one of LLVM's positional operands (`$0`, `$1`, ...).

### Source processing and comments

Every assembly body is part of the normal DQ source stream rather than an opaque byte range copied line-by-line. The
parser consumes both `[[asm]]` and `[[inline, asm]]` bodies through the normal `OScFeederDq` / `scf->SkipWhite()`
processing before deciding whether the next active item is assembly text, an inline-asm hint list or `endfunc`.

Consequently:

- DQ line comments (`// ...`) and block comments (`/* ... */`) are recognized and removed. A block comment behaves as
  whitespace, including when it occurs between assembly tokens. Assembly-looking text, `[[...]]` or `endfunc` inside
  a comment has no effect.
- DQ compiler directives retain their normal behavior. Conditional compilation selects the assembly text from the
  active `#if`, `#ifdef`, `#elif`, `#else`, etc. branch; directives and inactive source are not sent to LLVM.
- A trailing asm hint list in `[[inline, asm]]`, and `endfunc` in either form, are recognized only when active and
  outside comments. `[[...]]` has no special meaning in a non-inline `[[asm]]` body.
- Source processing must retain a separator between tokens separated by skipped trivia and preserve instruction line
  boundaries. Removing a comment or an inactive branch must not concatenate tokens or adjacent instructions.
- Once an active assembly instruction has started, its target-specific punctuation remains assembly text. Only DQ
  comment syntax is removed; other target-specific assembly syntax is left for the target assembler.

#### `#` handling

Outside assembly blocks, `#` is handled exactly as it is currently. Inside an assembly block, its
meaning is determined by its position on the physical source line:

- If `#` is the first non-whitespace, non-comment character, it starts a normal DQ compiler directive. Indented
  directives are therefore supported.
- After any active assembly text has occurred on the line, `#` is part of the target assembly text. For example, the
  `#1` in the ARM instruction `mov r0, #1` is passed to the assembler unchanged.

The parser keeps a per-line `assembly_started` state and resets it at every physical newline. It uses normal DQ
whitespace, comment and directive processing while `assembly_started` is false, but must not let `SkipWhite()` treat a
later `#` as a directive after assembly text has started. The same line-start rule applies while skipping inactive
conditional branches, so an ARM immediate in inactive assembly cannot be mistaken for a malformed DQ directive.

This intentionally treats a line-leading immediate as a directive rather than assembly text:

```
    mov r0,
        #1
```

Assembly should initially keep each instruction on one physical source line. If line-leading immediates are
needed later, directive recognition can be refined to inspect the token following `#`.

Applying this processing to ordinary `[[asm]]` bodies intentionally changes their former raw-body behavior. Target
assembly must avoid DQ's `//` and `/* ... */` comment delimiters and line-leading `#` when those characters are meant
to reach the assembler.

For example:

```
function MulImmediate(value : int32, factor : int32) -> int32 [[inline, asm]]:
    // The comment is not part of the assembly template.
    #ifdef INSERT_DEBUG_NOP
        nop
    #endif
    /* Assembly-looking text in this comment is ignored:
       imul $result, $value, 99 */
    imul    $result, $value, $factor  // DQ line comment
    [[immediate(factor), clobber(flags)]]
endfunc
```

### Operand hints

An inline-assembly function may have an optional trailing `[[...]]` hint list immediately before `endfunc`. The hint
list is DQ metadata and is not part of the assembly text. Its entries are asm hint clauses: they use function-call
syntax, but they are evaluated at compile time and are not functions.

The supported clauses are:

- `immediate(...)` for arguments that must be compile-time immediate values
- `memread(...)` for memory locations read by the assembly
- `memwrite(...)` for memory locations written by the assembly
- `memreadwrite(...)` for memory locations both read and written by the assembly
- `clobber(...)` for additional target resources modified by the assembly

The first four clauses take function argument names. `clobber(...)` takes target resources such as `flags`, `memory`
or physical registers. Memory arguments must be typed pointers or references so that the compiler knows the accessed
element type. An argument must not occur in conflicting clauses.

Clause order and argument order do not affect semantics. Within each operand group, function arguments retain their
declaration order. The compiler generates LLVM operands in this canonical order:

1. The normal function result output, when present
2. `memwrite` operands and the write halves of `memreadwrite` operands
3. Normal register inputs, immediate inputs and `memread` inputs
4. The implicit read halves of `memreadwrite` operands
5. Clobbers

For example, several operands may share a hint without making the function signature longer:

```
[[immediate(scale, shift), memread(source1, source2), memwrite(destination), clobber(flags)]]
```


## Examples

```
function __RBIT(arg1 : uint) -> uint  [[inline, asm]]:
    rbit    $result, $arg1
endfunc
```

```
function __SSAT(value : int, bits : int) -> int  [[inline, asm]]:
    ssat    $result, $bits, $value
    [[immediate(bits), clobber(flags)]]
endfunc
```
Lowering:
```
call i32 asm
    "ssat $0, $2, $1",
    "=r,r,i,~{cc}"
    (i32 %value, i32 %bits)
```

```
function Load32(address : ^uint32) -> uint32  [[inline, asm]]:
    mov     $result, $address
    [[memread(address)]]
endfunc
```
Lowering:
```
%result = call i32 asm inteldialect
    "mov $0, $1",
    "=r,*m"
    (ptr elementtype(i32) %address)
```

ARM version:
```
function Load32(address : ^uint32) -> uint32  [[inline, asm]]:
    ldr     $result, $address
    [[memread(address)]]
endfunc
```
Lowering:
```
%result = call i32 asm
    "ldr $0, $1",
    "=r,*m"
    (ptr elementtype(i32) %address)
```


```
function Store32(address : ^uint32, value : uint32)  [[inline, asm]]:
    mov   $address, $value
    [[memwrite(address)]]
endfunc
```
Lowering:
```
call void asm inteldialect
    "mov $0, $1",
    "=*m,r"
    (ptr elementtype(i32) %address, i32 %value)
```

Memory read-write:
```
function AddTo(address : ^uint32, value : uint32)  [[inline, asm]]:
    add $address, $value
    [[memreadwrite(address), clobber(flags)]]
endfunc
```
Lowering:
```
call void asm inteldialect
    "add $0, $1",
    "=*m,r,*m,~{cc}"
    (
        ptr elementtype(i32) %address,
        i32 %value,
        ptr elementtype(i32) %address
    )
```

```
function IncMem(address : ^uint32)  [[inline, asm]]:
    inc    $address
    [[memreadwrite(address), clobber(flags)]]
endfunc
```
Lowering:
```
call void asm inteldialect
    "inc $0",
    "=*m,*m,~{cc}"
    (
        ptr elementtype(i32) %address,
        ptr elementtype(i32) %address
    )
```
