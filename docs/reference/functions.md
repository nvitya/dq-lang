# Functions

This page defines function declarations, parameters, overloads, and special
functions. See the [Functions guide](../language/functions.md) for common usage.

## Declaration and Definition

```dq
func Add(a : int, b : int) -> int:
    return a + b
endfunc
```

A signature consists of the function name, ordered parameter types and modes,
return type, calling convention, object binding, and relevant attributes. A
declaration without a body can be implemented later with the same signature.

## Local Subfunctions

A `subfn` declares a helper inside a function or object method:

```dq
func Count() -> int:
    var count : int = 0
    subfn Inc():
        ++count
    endsubfn
    Inc()
    result = count
endfunc
```

Subfunctions follow declaration-before-use and lexical block visibility. They
can access enclosing parameters, locals, constants, and the enclosing method's
`self` and implicit members. Method namespace restrictions still apply: use
`@.` for module-root values unless a method `use` makes them available.
Parameters and locals can shadow enclosing names, with the usual warning.
Each value-returning subfunction has its own `result`; an enclosing `result`
is never implicitly accessible, including from a void subfunction.

Captures of mutable variables refer to their original storage, including dynamic
array descriptors and value parameters. Constants are resolved directly, and
`self` uses the existing receiver pointer. A subfunction can call itself and
previously declared sibling helpers. Captures needed by sibling calls are
forwarded automatically, even when the caller has a local with the same name.

A subfunction cannot declare another subfunction, including inside control-flow
blocks. Subfunctions support direct calls only: their addresses cannot be taken,
and they cannot be stored, returned, or passed as function references, even when
they capture nothing. Captured storage and its cleanup remain owned by the
enclosing invocation. Exceptions and local cleanup follow normal function rules.

Each helper is emitted as an internal LLVM function with one hidden argument
per required runtime capture, before its source parameters. There is no closure
allocation or context structure.

## Result Values

A function without `->` has no result value. A value-returning function can
return an expression directly or assign the built-in `result` variable.
Every reachable normal exit must leave a valid result.

## Parameter Modes

Plain parameters are passed by value. Reference modes alias caller storage:

| Mode | Contract |
| --- | --- |
| `name : T` | function receives a value |
| `name : ref T` | readable and writable alias |
| `name : refin T` | readable alias, not writable through the parameter |
| `name : refout T` | writable output alias; input value is not part of the contract |
| `name : refnull T` | nullable reference; accepts `nil` |

The argument must be an addressable, lifetime-compatible value for a reference
parameter. Passing a reference never transfers ownership unless a specific API
documents a separate ownership convention.

View types such as `[]T`, `rostr`, `strslice`, and `embstr` borrow their underlying
storage even when the descriptor itself is passed by value.

An `autofree` value parameter is borrowed by a normal value parameter. `ref`
parameters require matching ownership qualification; `autofree` return values
and `refin`/`refout autofree` parameters are not allowed. See
[`autofree` Ownership Types](autofree.md).

## Default and Named Arguments

A default expression supplies an omitted trailing argument. Named arguments use
the declared parameter name and participate in overload selection.

```dq
func Open(path : rostr, retries : int = 0)
Open(path = "data.txt", retries = 2)
```

Defaults must be valid where declared and do not create additional runtime
overloads.

## Overloading

Functions explicitly marked for overloading may share a name when their
signatures can be distinguished. Resolution considers argument count, names,
parameter modes, and defined conversion costs. An ambiguous best match is a
compile error.

Return type alone does not distinguish overloads.

## Variadic Functions

C-compatible variadic external functions use `...` and follow the target C ABI,
including its default argument promotions. DQ-native heterogeneous interfaces
normally use an `[]anyvalue` parameter and contextual array literals.

The caller and declaration author are responsible for external variadic type
agreement; the callee has no DQ signature information for the unnamed values.

## Function References

Functions can be assigned to a compatible `func(...)` reference type.
Object methods require an `of object` reference. See
[Structures, Pointers, and Function References](structures-pointers-and-function-references.md).

## Attributes and External Functions

Attributes control behavior such as `external`, `overload`, `inline`, `weak`,
and object virtual dispatch. Attributes cannot be combined when their execution
models conflict. See [Attributes, Directives, and Interoperability](attributes-directives-and-interop.md).

## Special Functions

Special names beginning with `*` define compiler/runtime entry points such as
constructors, destructors, module initialization, and the program main function.
Their permitted signature and placement depend on the specific special name;
they are not ordinary callable overload names.
