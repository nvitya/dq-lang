# Variables, Constants, References, and Scope

A variable names storage whose value can change. A constant names a value known
during compilation. A reference gives another name to existing storage.
A scope controls where a declaration can be found.

```dq
use print

const LIMIT : int = 10

func *Main():
    var count : int = 2
    ref alias = count
    alias += 3
    PrintLn("{} / {}", [count, LIMIT]) // 5 / 10
endfunc
```

## Variables and Initialization

Declare a variable with `var name : Type`, normally followed by `= initializer`.
An assignment replaces its current value:

```dq
var count : int = 0
count = 3
count += 1
```

Initialize scalar locals before reading them. A declaration such as
`var count : int` reserves storage but does not make its value readable; the
compiler tracks definite assignment across branches and reports uninitialized
uses. A function result must likewise be assigned on every normal exit path.

Global storage is zero/default-initialized before program startup. Types with
managed lifetimes, including dynamic arrays and owning references, have their
own empty/default initialization and cleanup rules. Use an explicit initializer
when the intended value matters. `{}` explicitly zero/default-initializes a
supported aggregate such as a structure or fixed array.

Object fixed storage uses `<-` rather than `=`; see
[Objects and Properties](120-objects-and-properties.md#storage-forms).

## Restricted Type Inference

Write `?` in place of a declared type when an initializer independently supplies
an eligible concrete typed pointer, structure, or object-reference type:

```dq
struct SPoint:
    x : int
    y : int
endstruct

var point : SPoint = {10, 20}
var copy : ? = point
var address : ? = %point
```

Inference requires `=` and preserves the initializer's static type. Numeric,
Boolean, text, enum, raw `pointer`, and other unlisted types are not inferred.
In particular, `var n : ? = 3` and `var p : ? = nil` are errors. The separate
array notation `[?]T` infers a fixed array's length, not its element type.

`new autofree T` provides a separate owning declaration shorthand; ordinary `?`
inference does not add ownership. See
[Memory Management and Ownership](130-memory-management-and-ownership.md).

## Local References

`ref name = existing_value` binds an alias to addressable compatible storage.
Writes through the alias update the original storage. The binding must remain
valid throughout the alias's use. A reference is not a nullable pointer value.

A reference cannot extend the lifetime of a local, a temporary, an array element,
or a property result. Function reference parameter modes are described in
[Functions](060-functions.md#parameter-modes).

## Constants

`const` requires a compile-time constant initializer:

```dq
const BUFFER_SIZE : int = 4096
const HALF_BUFFER : int = BUFFER_SIZE div 2
```

Related constants can share a declared type:

```dq
const(uint32):
    FLASH_BASE = 0x08000000
    FLASH_END  = FLASH_BASE + 0x7FFF
    SRAM_BASE  = 0x20000000
endconst
```

The group does not create another scope. Later constants can use earlier ones.
Restricted `?` inference follows the variable rules and does not remove the
constant-expression requirement; a grouped `const(Type)` header cannot use `?`.

`FirstInt`, `FirstFloat`, and `FirstBool` provide optional constant fallbacks;
see [Compiler Directives](190-compiler-directives.md#optional-constants).

## Lexical Scope and Declaration Order

Lookup starts in the innermost active scope and proceeds outward. A declaration
must be visible before a use that needs it. Forward declarations explicitly
reserve function or object names when their implementations come later.
Declarations in an inner block are not visible after that block finishes.

Shadowing a visible name can produce a warning. Choosing distinct names makes
it easier to tell which value an expression reads. A local reference and an
ordinary variable follow the same scope visibility rules.

## Module and Object Visibility

Module declarations before `implementation` are public. Declarations after it
are private unless they implement an earlier public declaration. Imported
symbols participate in unqualified lookup according to the `use` modifiers.

`@.Name` explicitly selects the current module's global scope.
`@module.Name` selects an imported module namespace. Object methods prioritize
members and require qualification for outside value symbols unless an allowed
method-local `use` makes them available. Type names remain available for type
annotations and casts.

Object members have `public`, `protected`, and `private` visibility; their rules
are in [Objects and Properties](120-objects-and-properties.md#members-and-visibility).
See [Modules and Packages](170-modules-and-packages.md) for import aliases,
merged symbols, and reexports.
