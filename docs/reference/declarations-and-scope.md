# Declarations and Scope

This page defines common declaration and lookup rules. See
[Language Basics](../language/basics.md) for normal usage.

## Variables

A variable declaration starts with `var` and normally includes an explicit
type. Restricted declaration type inference is available with `?` when the
initializer has an independently determined typed-pointer, structure, or object
reference type.

```dq
var count : int = 0
var ready : bool
var registers : ? = ^SRegisters(address)
var values : [?]int = [1, 2, 3]
```

An omitted initializer default-initializes the value according to its type.
An inferred declaration requires an initializer. Integer, floating-point, null,
raw-pointer, and all other unlisted types are not eligible. `[?]T` remains the
separate syntax for inferring a fixed array's length.

Object fixed-storage declarations use `<-` and are described in
[Objects and Properties](objects-and-properties.md).

## References

A local alias is declared with `ref` and must bind to compatible storage.
Assignment through the alias writes the referenced value.

```dq
var value : int = 10
ref alias = value
alias = 20
```

A reference is not a nullable pointer value. Function parameter reference modes
are defined in [Functions](functions.md).

## Constants

Constants use `const`, have a declared type, and require a compile-time constant
initializer.

```dq
const BUFFER_SIZE : int = 4096
const REGISTERS : ? = ^SRegisters(0x40000000)
```

`?` follows the same restricted inference rules as variables. Inference does
not relax the requirement for a valid constant expression and is not supported
in a grouped `const(type): ... endconst` header.

Related constants of the same type can share a declaration block. The block
does not introduce a scope, and later members can reference earlier members.

```dq
const(uint32):
    FLASH_BASE      = 0x08000000
    FLASH_BANK1_END = FLASH_BASE + 0x7FFF
    SRAM_BASE       = 0x20000000
endconst
```

## Type Declarations

`type` introduces a type alias. `struct`, `enum`, and `object` introduce compound
types with their own declaration rules.

```dq
type TSize = uint
type FTransform = function(value : int) -> int
```

Aliases do not create a distinct runtime type. Enumerations do.

## Visibility and Declaration Order

Module declarations before `implementation` form the public interface. Later
declarations are private implementation details unless they implement a public
declaration. Object declarations use `public` and `private` member groups.

Names must be declared before uses that require their complete meaning, except
where the language explicitly permits a public declaration followed by an
implementation.

## Name Lookup

Unqualified lookup starts in the innermost active scope and proceeds outward.
Imported symbols may participate when a `use` merges them into the scope.
Namespace qualification uses `@name.Symbol`; `@.` selects the current module's
global scope.

Object methods intentionally prioritize object members and restrict implicit
access to outside value symbols. Use `@.Name`, an imported namespace, or an
allowed method-local `use` to select module-scope values explicitly. Type names
remain available for declarations and casts.

Detailed import, conflict, and reexport behavior is defined in
[Modules and Packages](modules-and-packages.md).

## Initialization

Primitive and aggregate values can be initialized with type-appropriate
expressions. `{}` performs zero/default initialization for supported aggregate
types. Assignment is a statement and does not itself produce a value.
