# Objects and Properties

An object groups state and behavior behind a reference. It can define
constructors, destructors, methods, inheritance, and properties. Unlike a
structure, assigning an object variable shares the object rather than copying
its fields.

```dq
use print

object OCounter:
private
    stored : int = 0
public
    property value : int read stored
    func Increment():
        stored += 1
    endfunc
endobject

func *Main():
    var counter <- OCounter()
    var alias : OCounter = counter
    alias.Increment()
    PrintLn("{}", [counter.value])
endfunc
```

This prints `1`. The fixed object is destroyed when its containing scope ends;
the alias does not create another object or extend the lifetime.

## Object Types and References

An `object` defines a reference type. A variable of object type contains a
reference, not the object's fields. It may contain `nil`.

```dq
object OCounter:
    value : int = 0
endobject

var counter : OCounter = nil
```

Assigning an object variable copies the reference. It does not copy the object
or transfer ownership.

Use `autofree OType` when a reference storage location must own and release a
heap object automatically. Its moves and cleanup rules are defined in
[`autofree` Ownership Types](130-memory-management-and-ownership.md).

## Storage Forms

`new OType(...)` allocates an object on the heap and returns a reference.
`delete reference` calls its destructor and releases heap storage.

`<-` creates fixed object storage associated with the containing scope or
aggregate:

```dq
var local_counter <- OCounter()
```

The parentheses invoke the object's constructor, including when it has no
arguments. To reserve the storage and call `Create` manually later, mark that
intent explicitly with `..`:

```dq
object OOwner:
    child <- OChild ..

    func *Create():
        child.Create()
    endfunc
endobject
```

The resulting reference uses the same member-access model, but its storage is
not heap-owned and must not be passed to `delete`. Local fixed storage is cleaned
up when its scope exits, including during exception unwinding. Global and
embedded storage follow their containing lifetime.

Deleting `nil` is harmless where accepted by the runtime. Using any reference
after its object has been destroyed is invalid.

## Field Initialization

Fields initialize in declaration order before the constructor body. Explicit
field initializers are evaluated for each constructed object; otherwise fields
receive their default value. Base-object initialization precedes derived fields.

If construction raises, already initialized owned fields and base state are
cleaned up in reverse order. The not-fully-constructed object is not returned.

## Constructors and Destructors

`*Create` is a constructor and may be overloaded. `*Destroy` is the destructor.
Construction proceeds base-to-derived; destruction proceeds derived-to-base.

An inherited constructor or destructor can be invoked according to the special
func rules. The compiler prevents accidental double invocation of the same
base lifetime operation.

## Members and Visibility

Fields, methods, and properties can be grouped with `public`, `protected`, and
`private` labels. Public members are accessible to callers; protected members
are accessible in the declaring object and its descendants; private members
are restricted to the declaring object's implementation. Method
bodies can access members without a `self.` prefix.

Methods may be declared inside the object and implemented later with a qualified
name. Their declarations and definitions must have matching signatures.

## Inheritance and Virtual Dispatch

DQ supports single object inheritance. A base reference may refer to an instance
of a derived object.

`[[virtual]]` creates a virtual method slot; `[[override]]` implements the
corresponding inherited slot. `inherited` calls the inherited implementation of
the current method. `[[abstract]]` declares a method without an implementation,
and `[[final]]` prevents further overriding where supported.

Virtual calls dispatch on the runtime object type. Calls explicitly directed to
an inherited implementation are non-virtual for that selection.

## Runtime Type Tests and Casts

Polymorphic objects carry runtime type information sufficient for inheritance
checks and exception matching.

`value is OType` tests compatibility. `TryCast(OType, value)` and a declaration
using `tryfrom` return a typed reference or `nil` when incompatible.

```dq
if base is OChild:
    var child : OChild tryfrom base
endif
```

These operations preserve object identity and do not allocate or copy an object.

## Properties

A property provides field syntax backed by a field or accessor methods. It must
define at least a reader or writer.

```dq
property value : int read GetValue write SetValue
property direct : int read stored write stored
```

A getter takes no value argument and returns the property type. A setter receives
the new value after any property indices. Accessor parameter and return types
must match the declaration, and accessors must be methods of the object.

Read-only and write-only properties omit the unsupported accessor. Reading a
write-only property or assigning a read-only property is a compile error.

## Indexed and Default Properties

An indexed property declares its indices before the result type:

```dq
property item : [index : int]int read GetItem write SetItem
property cell : [row : int, col : int]int read GetCell write SetCell
```

The getter receives the indices. The setter receives the indices followed by the
new property value. Put `default` after the property type, before its accessors,
to handle indexing directly on the object reference:

```dq
property item : [index : int]int default read GetItem write SetItem
```

Default-property lookup uses the receiver's declared type. The last default
property declared in that type takes precedence; if there is none, lookup
continues in the base type. An upcast receiver therefore uses the base type's
default property. The selected accessor may still dispatch virtually.

## Dispatch and Addressability

Accessor methods participate in normal virtual dispatch. A property itself is
not addressable and cannot bind directly to `ref`, `refin`, or `refout`.
Modify-assignment reads through the getter, performs the operation, and writes
through the setter; both accessors are therefore required.

Values returned from a property keep their own normal semantics. For example,
an object reference returned by a property can be used for member access.


## Forward Declarations

Mutually referring object types can be declared before their complete bodies:

```dq
object OParent ..

object OChild:
    parent : OParent
endobject

object OParent:
    child : OChild
endobject
```

Complete the type before a use that requires its layout or full interface.
Declarations of constructors and methods can similarly use `..` and receive
qualified definitions later.

## Object Type Variables and Names

`type of OBase` stores an eligible concrete object type derived from `OBase`.
It can select an allocation type at runtime:

```dq
object OBase:
endobject

object OChild(OBase):
endobject

func MakeObject() -> OBase:
    var selected : type of OBase = OChild
    return new selected()
endfunc
```

The selected type must be concrete and satisfy the base constructor contract.
Unrelated types, abstract types, and incompatible constructor signatures are
rejected. The returned ordinary object reference still needs an owner or an
explicit `delete`.

`TypeName(Type)` names a type. `TypeName(object_value)` uses the runtime object
type for a non-nil reference; a nil reference reports its declared static type.
It is a diagnostic name, not a stable serialization or ABI identifier.

## Object Function References

A reference declared with `func(...) of object` stores a compatible method and
its receiver. A virtual method is resolved for that receiver at assignment.
Keep the receiver alive for every later call; the reference does not own it.
See [Functions](060-functions.md#function-references).

## Object Example Code



```dq
use print

object OBase ..

object OHelper:
    base : OBase

    func *Create(abase : OBase) ..
    func *Destroy() ..
endobject

object OBase:
protected
    intval : int = 1
public
    name   : embstr(64)
    helper <- OHelper ..

    property value : int  read intval

    func PrintExtra()  [[virtual]]:
        @.Print(' value={}', [value])
    endfunc

    func *Create(aname : rostr):
        name = aname
        helper.Create(self)
    endfunc

    func *Destroy():
        @.PrintLn('"{}" destroyed', [name])
    endfunc

    func Print()  [[virtual]]:
        @print.Print('{}:', [name])
        PrintExtra()
        @.PrintLn()
    endfunc
endobject

func OHelper.*Create(abase : OBase):
    base = abase
    @.PrintLn('Helper for "{}" created', [base.name])
endfunc

func OHelper.*Destroy():
    @.PrintLn('Helper for "{}" destroyed', [base.name])
endfunc


object OChild(OBase):
    flval : float

    func *Create(aname : rostr, aflval : float):
        inherited Create(aname)
        intval = 2
        flval = aflval
    endfunc

    func PrintExtra()  [[override]]:
        inherited
        @.Print(' flval={}', [flval])
    endfunc
endobject

func Manipulate(aobj : ref autofree OBase):
    aobj = new OChild('Child3-heap', 7 / 3)
    aobj.Print()
endfunc

func ErrorTest():
    PrintLn('Error test')
    try:
        var obj = new autofree OChild('ErrChild-heap', 8.3)
        raise Exception('Some error happened!')
    except Exception as e:
        PrintLn('ERROR: {}', [e.message])
        e.PrintBacktrace()
    endtry
endfunc

func *Main():
    PrintLn('DQ Objects Demo')

    var ob = new autofree OBase('Base-heap')
    ob.Print()

    if true:
        var oc1 <- OChild('Child1-stack', 3 / 2)
        oc1.Print()
    endif

    ErrorTest()

    PrintLn('reassigning autofree type')
    ob = new OChild('Child2-heap', 3.14)
    ob.Print()

    PrintLn('Autofree type as argument')
    Manipulate(ob)

    PrintLn('nil-ing autofree type')
    ob = nil

    PrintLn('Test finished.')
endfunc
```
Source: [https://github.com/nvitya/dq-lang/blob/main/examples/dq_intro_3.dq](https://github.com/nvitya/dq-lang/blob/main/examples/dq_intro_3.dq)
