# Attributes

Attributes attach metadata to declarations, changing layout, linkage, access,
or code generation. They are written inside `[[...]]`, before a declaration or
at its supported trailing attribute position. A comma-separated list combines
attributes that are valid together.

```dq
use print

func Double(value : int) -> int [[inline]]:
    return value * 2
endfunc

func *Main():
    PrintLn("{}", [Double(21)])
endfunc
```

The function still follows the normal DQ signature and type rules. The attribute
controls optimization, not what values can be passed.

## Syntax and Attribute Summary

Attributes use `[[...]]` and attach to the declaration they precede or follow.
Multiple attributes may share one list.

```dq
[[weak]] func Hook()
func Run() [[inline]]:
endfunc
```

Unknown, unsupported, or inapplicable attributes are diagnosed as warnings or
errors depending on whether compilation can continue safely. They never acquire
an undocumented effect. Important implemented attributes include:

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

An attribute cannot override an incompatible language contract. For example, an
external function has no DQ body.

`inline` is an LLVM inlining hint, not a guarantee. It is considered at `-O1`
and higher when compiling a module. Cross-module inlining additionally requires
full LTO (`--lto=full`), which makes the callee body available during linking.
At `-O1`, only definitions marked `inline` or `always_inline` are eligible for
inlining; unmarked definitions receive LLVM's `noinline` attribute, including
in full LTO builds. `noinline` still prevents inlining of an `inline` function.
Higher optimization levels can also inline unmarked functions.

`used` applies to functions and global storage. It preserves the emitted
definition through compilation and linking, including LTO. Use it when a symbol
is reached only through assembly, a linker script, a hardware table, or another
mechanism that is not represented by ordinary DQ references.

## Volatile and Register Access

The register attributes provide concise declarations for memory-mapped I/O:

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

Volatile accesses are observable compiler operations, but they are not atomic
and do not imply a CPU or compiler memory barrier. Taking an address or binding
a reference currently discards `noread` and `nowrite`; this is an explicit
low-level escape. Whole-structure initialization and copying also do not yet
apply the access restrictions of individual fields.

## Weak Attribute

`[[weak]]` gives a module-level function definition weak linker binding, allowing
a strong definition with the same linker name to replace it. It does not change
the function's linker name. Combine it with `[[cexport]]` when C or assembly code
must override the function by its unmangled DQ name.

```dq
func DefaultHandler() [[weak, cexport]]:
    // fallback implementation
endfunc
```

The attribute does not take arguments and is not valid on external declarations
or object and struct methods.


## Linkage and Placement

`external` imports an existing definition. `cexport` exports under the DQ name;
`export('native_name')` chooses an explicit native name. Exporting does not
replace the need to match an external caller's signature and ABI.
`section('name')` controls native placement; a linker script or native build
may be needed to place that section at an intended hardware address.

```dq
var counter : uint32 = 0 [[export('dq_counter'), align(4), used]]

func ReadCounter() -> uint32 [[cexport]]:
    return counter
endfunc
```

An explicit alignment must be valid for the target. `packed` can introduce
unaligned fields; use layout queries rather than assuming offsets.

## Validity and Conflicts

Attributes have declaration-specific applicability. A function attribute on a
variable, or a storage attribute on an unsupported type, does not create that
behavior. The compiler diagnoses ignored or invalid attributes; resolve warnings
rather than relying on them.

`weak` requires a module-level definition and cannot decorate an external
function or a method. Assembly and inline-assembly functions impose additional
combinations in [Assembly Functions](210-assembly-functions.md). `external`
functions have no DQ body. `noread` plus `nowrite` is an error, and a modify
assignment requires both access modes.

Memory-access attributes are not a synchronization API. They do not replace
atomics, locks, or the platform's memory barriers. See
[Objects and Properties](120-objects-and-properties.md) for dispatch attributes
and [C Interoperability](200-c-interoperability.md) for native declarations.
