# Attributes

An **attribute** gives the compiler extra instructions about a declaration.
You have already seen examples such as `[[overload]]` for functions with the
same name and `[[virtual]]` for object methods. Other attributes control
optimization, memory layout, or access to external code.

Write attributes inside `[[...]]` next to the declaration they affect:

```dq
use print

func Double(value : int) -> int [[inline]]:
    return value * 2
endfunc

func *Main():
    PrintLn("{}", [Double(21)])
endfunc
```

This prints `42`. `[[inline]]` asks the compiler to consider putting the
function's code directly at its call sites. It does not change how you call
`Double` or the values it accepts.

## Syntax and Attribute Summary

An attribute can go before a declaration or in its supported trailing position.
Combine attributes in one list, separated by commas:

```dq
func DefaultHandler() [[weak, cexport]]:
    // fallback implementation
endfunc
```

Not every attribute applies to every declaration. Use the table to find the
attribute for your task; less common details follow the examples.

| Attribute | Purpose and applicable declarations |
| --- | --- |
| `external`, `external('name')` | Bind a function or global variable to a native symbol. |
| `external('name', 'module')` | Select a WebAssembly function import module. |
| `export('name')` | Export a function or global value with an explicit native name. |
| `cexport` | Use the declaration name as its unmangled native export name. |
| `used` | Retain a function or global value whose references are invisible to the optimizer. |
| `section('name')` | Place a function or global value in a named native section. |
| `align(n)` | Request alignment for supported global storage or compound fields. |
| `packed` | Pack a structure/compound layout. |
| `overload` | Mark a function, method, or constructor as an overload. |
| `inline`, `always_inline`, `noinline` | Control inlining of functions. |
| `virtual`, `override`, `abstract`, `final` | Control object method dispatch and inheritance. |
| `asm` | Supply a function body in target assembly. |
| `weak` | Give a module-level function definition replaceable weak linkage. |
| `volatile`, `noread`, `nowrite` | Control direct low-level global/field access. |
| `regrw`, `regro`, `regwo` | Volatile register read/write, read-only, and write-only shorthands. |
| `nowarn` | Suppress warnings on supported declarations/statements. |

## Function Inlining

Inlining can avoid the cost of a function call, but can also increase program
size. `[[inline]]` is a hint; `[[always_inline]]` makes a stronger request, and
`[[noinline]]` prevents inlining. Normal `[[inline]]` is considered at `-O1`
and above.

Inlining across module boundaries requires full link-time optimization
(`--lto=full`), which lets the optimizer see function bodies from other modules.

## Linkage and Placement

Use `[[external]]` for a function or global variable supplied by native code.
Use `[[cexport]]` to expose a DQ declaration under its own name, or
`[[export('native_name')]]` to choose another name:

```dq
var counter : uint32 = 0 [[export('dq_counter'), align(4), used]]

func ReadCounter() -> uint32 [[cexport]]:
    return counter
endfunc
```

Native code can refer to `dq_counter` and `ReadCounter` using the corresponding
C declarations. Match the types and calling convention described in
[C Interoperability](200-c-interoperability.md).

`[[used]]` retains a function or global value even when ordinary DQ code does
not refer to it. Use it for names reached through assembly, a linker script,
or a hardware table. It preserves the definition through compilation and
linking, including link-time optimization.

`[[section('name')]]` puts a function or global in a named native section. A
linker script or other build setup may be needed to place it at a particular
hardware address.

## Layout Attributes

`[[packed]]` removes ordinary layout padding from a structure or compound type.
`[[align(n)]]` requests alignment on supported global storage or fields:

```dq
struct SHeader [[packed]]:
    kind : uint8
    length : uint32
endstruct
```

Use packed layout only when the file format, hardware, or external declaration
requires it. Packed fields may be unaligned. Check `SizeOf` and `OffsetOf`
instead of assuming a particular layout, and choose alignment values supported
by the target. See [Structures and Unions](100-structures-and-unions.md#layout-and-external-interfaces).

## Volatile and Register Access

Hardware registers often live at fixed memory addresses. Register attributes
let a declaration say whether each register can be read, written, or both:

```dq
struct SDeviceRegisters:
    STATUS  : [[regro]] uint32
    CONTROL : [[regrw]] uint32
    COMMAND : [[regwo]] uint32
endstruct
```

They expand to the fundamental access attributes:

| Register attribute | Equivalent attributes | Allowed operations |
| --- | --- | --- |
| `[[regrw]]` | `[[volatile]]` | read and write |
| `[[regro]]` | `[[volatile, nowrite]]` | read only |
| `[[regwo]]` | `[[volatile, noread]]` | write only |

Reading `[[noread]]` storage or writing `[[nowrite]]` storage is a compile error.
A modify-assignment such as `|=` reads and writes its target and therefore needs
both operations to be allowed. `noread` and `nowrite` cannot be combined on the
same declaration. These restrictions apply to direct global, field, and
fixed-array-element access and are retained through module interfaces.

## Weak Attribute

`[[weak]]` provides a fallback function that native code can replace with a
strong definition of the same linker name. Combine it with `[[cexport]]` when
C or assembly code should use the function's plain DQ name:

```dq
func DefaultHandler() [[weak, cexport]]:
    // used unless another linked definition replaces DefaultHandler
endfunc
```

`weak` does not change the name by itself. It takes no arguments and applies
only to module-level function definitions, not external declarations or methods.

## Optimization Details

At `-O1`, only functions marked `inline` or `always_inline` are eligible for
inlining. Unmarked definitions receive LLVM's `noinline` attribute, including
in full LTO builds. An explicit `noinline` still prevents inlining of a function
marked `inline`. Higher optimization levels can also inline unmarked functions.

## Access Details and Limitations

Volatile accesses are observable compiler operations, but they are not atomic
and do not imply a CPU or compiler memory barrier. Taking an address or binding
a reference currently discards `noread` and `nowrite`; this is an explicit
low-level escape. Whole-structure initialization and copying also do not yet
apply the access restrictions of individual fields.

## Validity and Conflicts

Unknown, unsupported, or misplaced attributes produce warnings or errors.
Resolve those diagnostics rather than relying on an attribute the compiler
could not apply. An attribute cannot bypass a language rule: for example, an
`external` function cannot also have a DQ body.

Some combinations have additional restrictions. `weak` requires a module-level
definition; assembly functions have the combinations described in
[Assembly Functions](210-assembly-functions.md). Access restrictions apply to
reads and writes individually, so an operation such as `+=` requires both.

For object method attributes, see
[Inheritance and Virtual Dispatch](120-objects-and-properties.md#inheritance-and-virtual-dispatch).
