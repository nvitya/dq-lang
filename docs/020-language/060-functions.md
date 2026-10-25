# Functions

A function gives a name and a parameter list to reusable code. Parameters carry
input values; an optional result type describes the value returned to the caller.

```dq
use print

func Add(left : int, right : int) -> int:
    return left + right
endfunc

func *Main():
    PrintLn("{}", [Add(2, 3)])
endfunc
```

## Declaration and Definition

A definition uses `func Name(parameters)`, an optional `-> ResultType`, and a
body closed by `endfunc`. Omit `->` for a function with no result.
A declaration reserves the signature without supplying its body:

```dq
func Add(left : int, right : int) -> int ..
```

Implement it later with the matching name, ordered parameter types and modes,
result type, and relevant attributes. The public part of a module can declare
functions whose definitions follow `implementation`.

## Result Values

A function with a result type has its own built-in `result` variable. Either
return a value directly or assign `result` before a normal exit:

```dq
func Square(value : int) -> int:
    result = value * value
endfunc
```

Every reachable normal exit must leave the result assigned. A function without
a result cannot `return value`. A returned view, pointer, object reference, or
bound method must not refer to storage whose lifetime ends with the function.

## Parameter Modes

Plain parameters receive values. Copying an object reference copies its address,
and copying `str` retains its value. Passing by value does not mean deep-copying
an object or its data. Dynamic-array parameters require a reference mode; use
`[]T` for an ordinary borrowed array parameter.

| Form | Contract |
| --- | --- |
| `value : T` | A value parameter; rebinding it does not rebind caller storage. |
| `value : ref T` | A readable and writable alias to caller storage. |
| `value : refin T` | A read-only input alias. |
| `value : refout T` | An output alias whose input value is not part of the contract. |
| `value : refnull T` | A reference that may be omitted by passing `nil`. |

```dq
func SetValue(value : ref int):
    value = 5
endfunc

func MaybeSet(value : refnull int):
    if %value == nil:
        return
    endif
    value = 44
endfunc
```

A reference argument needs compatible addressable storage with sufficient
lifetime. A property is not itself addressable. For `refnull`, test `%value`
before reading or writing when the caller may have supplied `nil`.

`[]T`, `rostr`, `strslice`, and unsized `embstr` borrow storage even when their
descriptors are passed by value. A normal object/pointer parameter borrows an
`autofree` argument; `ref` parameters preserve its ownership qualification
exactly. Ownership restrictions are in
[Memory Management and Ownership](130-memory-management-and-ownership.md).

## Default Arguments

Trailing parameters may have compile-time constant default values. The caller
may omit those trailing arguments:

```dq
func Scale(value : int, factor : int = 2) -> int:
    return value * factor
endfunc

func Demonstrate() -> int:
    return Scale(4) + Scale(4, 3)
endfunc
```

This returns `20`: the calls use factors `2` and `3`. Defaults are converted to
their parameter type at the declaration. Reference parameters, array-slice
parameters, and unsized `embstr` parameters cannot have defaults. Calls pass arguments positionally; named
arguments are not supported.

## Overloading

Functions, methods, or constructors with the same name must be explicitly marked
`[[overload]]`. Parameter signatures must distinguish the alternatives:

```dq
func Sum(left : int, right : int) -> int [[overload]]:
    return left + right
endfunc

func Sum(left : int, right : int, third : int) -> int [[overload]]:
    return left + right + third
endfunc
```

All overloads of one name must have the same result type. Parameter names and
reference modes alone do not distinguish signatures. Resolution considers
argument count, available defaults, permitted conversions, and whether reference
arguments meet the parameter's access requirements. An ambiguous best match is
an error.

## Variable Argument Lists

DQ functions commonly accept a slice of boxed values:

```dq
func LogPair(values : []anyvalue):
    PrintLn("{}: {}", values)
endfunc
```

A call such as `LogPair(["answer", 42])` constructs a contextual temporary array.
Its slice is valid during the call. C-style `...` is available on external
variadic declarations and follows the native ABI rather than `anyvalue` boxing;
see [C Interoperability](200-c-interoperability.md).

## Function References

A function reference stores a callable with a specified signature:

```dq
type FTransform = func(value : int) -> int

func Double(value : int) -> int:
    return value * 2
endfunc

func Apply(value : int) -> int:
    var transform : FTransform = Double
    return transform(value)
endfunc
```

The parameter types and modes, result, calling convention, and binding form must
be compatible. A function reference can be `nil`; calling it requires a valid
callable. An `of object` reference also stores an object receiver:

```dq
type FHandler = func(message : strslice) of object
```

The receiver must remain alive while the reference is used. A virtual method's
implementation is resolved when its reference is assigned. Ordinary object
method references do not extend the object's lifetime.

Explicit pointer-to-function-reference casts support low-level interfaces, but
the programmer must match the real signature and ABI. They are not implicit and
are not supported for `of object` references.

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

## Special Functions

Names beginning with `*` have compiler/runtime roles:

| Name | Role |
| --- | --- |
| `*Main()` | Application entry after runtime/module initialization; no result. |
| `*ModuleInit()` | Module initialization; no result. |
| `*Create(...)` | Object constructor. |
| `*Destroy()` | Object destructor. |

```dq
var ready : bool = false

func *ModuleInit():
    ready = true
endfunc

func *Main():
    if not ready:
        exit_code = 1
    endif
endfunc
```

A module's initialization runs once before its dependents. Do not assume an
automatic module/global finalization function exists. On a bare target, the
application's startup code must call `dq_module_init()` after initializing data
and zeroed storage. See [Modules and Packages](170-modules-and-packages.md).

Constructors and destructors have object-specific placement and inheritance rules;
see [Objects and Properties](120-objects-and-properties.md). Complete and inline
assembly functions have additional restrictions in [Assembly Functions](210-assembly-functions.md).
