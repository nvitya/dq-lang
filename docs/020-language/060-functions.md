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

C-style variable argument lists use `...` after the fixed parameters. They let
the caller pass additional arguments of different types. DQ supports this form
for external functions such as C's `printf`:

```dq
func printf(format : ^char, ...) -> int32 [[external]] ..

func *Main():
    var answer : int32 = 42
    printf("%s: %d\n", "answer", answer)
endfunc
```

This prints `answer: 42`. The extra arguments follow the target's C calling
convention, including its argument promotions. DQ cannot check that their types
match the format string; see [C Interoperability](200-c-interoperability.md).

A major limitation of C-style variable arguments is forwarding: a function
cannot simply pass its unnamed arguments on to another variadic function.
In C, this requires a separate `va_list` interface, such as
[`vprintf` instead of `printf`](https://sourceware.org/glibc/manual/latest/html_node/Variable-Arguments-Output.html).

To make forwarding straightforward, DQ borrows Delphi's
[`array of const` pattern](https://docwiki.embarcadero.com/RADStudio/Florence/en/Open_Arrays#array_of_const)
as `[]anyvalue`: a slice whose elements can hold values of different types.
Each `anyvalue` stores both the value and its type information. The entire
argument list is an ordinary parameter that can be forwarded to other functions:

```dq
use print

func Log(format : rostr, values : []anyvalue):
    PrintLn(format, values)
endfunc

func *Main():
    Log("{}: {}", ["answer", 42])
endfunc
```

This also prints `answer: 42`. The caller groups the arguments in brackets;
`["answer", 42]` creates a temporary array of `anyvalue` elements. `Log` passes
the slice directly to `PrintLn`, without unpacking or rebuilding the arguments.
The temporary array remains valid throughout the call, including calls made
by `Log`; do not retain its slice after the call returns. See
[`anyvalue`](150-anyvalue.md) for supported values and lifetime rules.

## Function References

A function reference lets you store a function in a variable and call it through
that variable. Its type specifies the parameters and return value the function
must have:

```dq
use print

type FTransform = func(value : int) -> int

func Double(value : int) -> int:
    return value * 2
endfunc

func Square(value : int) -> int:
    return value * value
endfunc

func *Main():
    var transform : FTransform = Double
    PrintLn("{}", [transform(4)])

    transform = Square
    PrintLn("{}", [transform(4)])
endfunc
```

This prints `8` and then `16`. The same variable first refers to `Double`, then
to `Square`. Both functions take an `int` and return an `int`, so either can be
assigned to `transform`.

The parameter types and modes, result, calling convention, and binding form must
be compatible. A function reference can be `nil`; calling it requires a valid
callable.

Explicit pointer-to-function-reference casts support low-level interfaces, but
the programmer must match the real signature and ABI. They are not implicit.

## Object Method References

A method is a function that belongs to an object. An object method reference
remembers both the method and the object to call it on. Add `of object` to the
function reference type:

```dq
use print

type FAction = func() of object

object OCounter:
    value : int = 0

    func Increment():
        value += 1
    endfunc
endobject

func *Main():
    var counter <- OCounter()
    var action : FAction = counter.Increment
    action()
    action()
    PrintLn("{}", [counter.value])
endfunc
```

This prints `2`. Assigning `counter.Increment` stores the method reference
without calling it. Each later `action()` call runs `Increment` on `counter`,
so it changes that object's `value`.

Keep the object alive for every call through the reference; storing its method
does not extend the object's lifetime. A virtual method's implementation is
resolved for that object when the reference is assigned. Pointer casts to
`of object` references are not supported. See
[Objects and Properties](120-objects-and-properties.md#object-function-references).

## Local Subfunctions

A `subfn` declares a helper inside a function or object method:

```dq
func Count() -> int:
    var count : int = 0
    subfn Inc(amount : int):
        count += amount
    endsubfn
    Inc(2)
    Inc(3)
    result = count
endfunc
```

`Count()` returns `5`: both calls to `Inc` add their amount to the same `count`
variable in the enclosing function. Using a variable from the enclosing function
is called *capturing* that variable. `Inc` works with the original `count`, so
its changes are still visible in `Count` after each call.

Subfunctions follow declaration-before-use and lexical block visibility. They
can access enclosing parameters, locals, constants, and the enclosing method's
`self` and implicit members. Method namespace restrictions still apply: use
`@.` for module-root values unless a method `use` makes them available.
Parameters and locals can shadow enclosing names, with the usual warning.
Each value-returning subfunction has its own `result`; an enclosing `result`
is never implicitly accessible, including from a void subfunction.

The same rule applies to dynamic arrays and parameters of the enclosing
function: the subfunction uses those existing variables rather than separate
copies. It can also use constants from the enclosing function. Inside an object
method, `self` still refers to the same object. A subfunction can call itself or
another subfunction declared earlier in the same scope.

A subfunction cannot declare another subfunction, including inside control-flow
blocks. Subfunctions support direct calls only: their addresses cannot be taken,
and they cannot be stored, returned, or passed as function references, even when
they use no variables from the enclosing function. Variables used by a subfunction
keep their usual lifetime and cleanup rules. Exceptions and local cleanup follow
normal function rules.

## Special Functions

The `*` marks a function that has a special role, such as starting the application
or initializing a module. It tells the compiler to check that the name is a
recognized special function name.

This helps catch typos. For example, `func *ModulInit()` is rejected because
`ModulInit` is not a recognized special name. Without the `*`, that misspelling
could silently declare an ordinary function, which would never be called
automatically to initialize the module.

Special functions must always be declared with `*` before their name. Their
names are reserved: declaring them without `*` is a compiler error. Explicitly
calling a special function is rarely needed, but when you do, write its name
without `*`.

The special functions are:

| Name | Role |
| --- | --- |
| `*Main()` | Application entry after runtime/module initialization; no result. |
| `*ModuleInit()` | Module initialization; no result. |
| `*Create(...)` | Object constructor. |
| `*Destroy()` | Object destructor. |

A common use of `*ModuleInit()` is to allocate a global object on the heap so
that `*Main()` can use it without any preparation:

```dq
use print

object OSettings:
    message : str = "Ready"
endobject

var settings : OSettings = nil

func *ModuleInit():
    settings = new OSettings()
endfunc

func *Main():
    PrintLn("{}", [settings.message])
    delete settings
endfunc
```

This prints `Ready`. Module initialization always finishes before `*Main()`
runs, so `settings` already refers to the allocated object when `*Main()` starts.

The runtime function `dq_module_init()` calls the modules' `*ModuleInit()`
functions once, in `use` dependency order: a used module is initialized before
the module that uses it. There is no module finalization; this example releases
the object explicitly with `delete settings`. See
[Modules and Packages](170-modules-and-packages.md).

For how to define `*Create` and `*Destroy`, including how they work with
inheritance, see [Constructors and Destructors](120-objects-and-properties.md#constructors-and-destructors).
