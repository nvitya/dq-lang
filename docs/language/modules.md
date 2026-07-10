# Modules

Each `.dq` file is a module. Modules are imported with `use`.

## Basic Use

```dq
use libc/stdio
use ./helpers
```

By default, imported public interface symbols are merged into the current module
scope and the imported module also receives a namespace.

```dq
use ./math_extra

var a : int = Add(1, 2)
var b : int = @math_extra.Add(1, 2)
```

## Module Namespaces

An imported module can be given an alias with `as`.

```dq
use ./mod_advuse as advm

@advm.SomeFunction()
```

Namespace-qualified access uses `@name.Symbol`.

`@.` refers to the global scope of the current module.

```dq
@.printf("from this module scope\n")
```

## Suppressing Scope Merge

Use `--` to import a module namespace without merging its symbols into the
current unqualified scope.

```dq
use ./mod_advuse as advm --

@advm.printf("qualified only\n")
```

## Selecting Imported Symbols

`only` imports only selected symbols into the unqualified scope.

```dq
use ./mod_advuse as advm only(CONST1, Func1)
```

`exclude` imports all eligible symbols except the listed symbols.

```dq
use ./mod_advuse as advm exclude(hidden_func)
```

`--`, `only`, and `exclude` are mutually exclusive scope-merge modifiers.

## Reexport

`reexport` makes selected imported interface symbols part of the current
module's public interface.

```dq
use ./leaf reexport
use ./leaf as leaf only(Value, Func) reexport
```

This is useful for facade modules.

## Interface and Implementation Sections

The `implementation` keyword separates a module's public interface from its
private implementation.

```dq
const PUBLIC_VALUE : int = 1
function PublicFunc() -> int

implementation

const PRIVATE_VALUE : int = 2

function PublicFunc() -> int:
    return PUBLIC_VALUE + PRIVATE_VALUE
endfunc
```

Symbols before `implementation` are part of the module interface. Symbols after
it are implementation details unless explicitly exposed by declarations in the
interface.

## Local Module Paths

Relative imports use filesystem-like paths.

```dq
use ./child
use ./nested/deep
use ../sibling
use ^/root_child
```

`./` is relative to the current module directory. `../` moves up one directory.
`^/` refers to the package or module root used by the compiler.

Bare module names such as `use print` or `use libc/stdio` are resolved through
the compiler's package/module search paths.

## Circular Module References

Modules may refer to each other when their public interfaces can be loaded
without requiring private implementation details first. The compiler stores
compiled module interface data in `.dqm` files.

## Method-Local Use

Object methods have restricted name lookup. A method may use selected local
`use` forms to opt in to module-scope names.

```dq
function OThing.Method():
    use .
    use helper_alias
endfunc
```

This form is intended for object methods, not normal functions.
