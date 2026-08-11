# Functions

This page defines function declarations, parameters, overloads, and special
functions. See the [Functions guide](../language/functions.md) for common usage.

## Declaration and Definition

```dq
function Add(a : int, b : int) -> int:
    return a + b
endfunc
```

A signature consists of the function name, ordered parameter types and modes,
return type, calling convention, object binding, and relevant attributes. A
declaration without a body can be implemented later with the same signature.

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
| `name : refnull T` | nullable reference; accepts `null` |

The argument must be an addressable, lifetime-compatible value for a reference
parameter. Passing a reference never transfers ownership unless a specific API
documents a separate ownership convention.

View types such as `[]T`, `strview`, and `cstring` borrow their underlying
storage even when the descriptor itself is passed by value.

## Default and Named Arguments

A default expression supplies an omitted trailing argument. Named arguments use
the declared parameter name and participate in overload selection.

```dq
function Open(path : strview, retries : int = 0)
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

Functions can be assigned to a compatible `function(...)` reference type.
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
