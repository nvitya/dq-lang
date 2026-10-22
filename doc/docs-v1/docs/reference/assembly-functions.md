# Assembly Functions

This page is the language contract for assembly-backed functions. The
[assembly guide](../language/asm-functions.md) contains the complete syntax,
operand tables, and target examples.

## Complete Assembly Functions

`[[asm]]` gives a function a complete target-assembly body. The assembly is
responsible for the declared calling ABI, including arguments, result,
callee-saved state, stack discipline, and returning to the caller. The compiler
does not emit a prologue or epilogue.

The body uses the colon/`endfunc` form. It cannot be external or abstract, and
DQ parameter names and `result` are not substituted as operands.

## Typed Inline Assembly Functions

`[[inline, asm]]` declares a typed template emitted at each direct call site.
`$parameter` and `$result` name compiler-allocated operands. `$$` emits a literal
dollar sign.

Inline assembly functions are module-level direct-call operations. They do not
have separately addressable function identities, cannot be methods or special
functions, and cannot be converted to function references.

## Operand Hints

One trailing attribute list may describe operands and effects:

- `immediate(parameter)` requires a compile-time integer-like value;
- `memread(parameter)`, `memwrite(parameter)`, and
  `memreadwrite(parameter)` describe typed memory effects;
- `clobber(resource)` declares changed registers, flags, or all memory.

Every memory operand must be a typed pointer or a reference mode that permits
the requested access. Every effect not represented by an output must be declared.
Incorrect constraints can make optimized code invalid even if unoptimized code
appears to work.

## Source Processing

DQ comments and compiler conditionals are processed inside assembly bodies.
Line-leading `#` is treated as a directive; after assembly text begins on a line,
`#` remains target assembly syntax. In inline assembly, the trailing operand-hint
list is metadata rather than an instruction.

## Targets

Typed inline assembly is implemented for the documented x86-64, ARM/Thumb,
AArch64, RV32, and RV64 target identifiers. Instruction syntax and available
register classes come from the selected target and CPU features. Complete
assembly functions may be accepted for other targets when the backend assembler
supports their body, but ABI correctness remains the author's responsibility.
