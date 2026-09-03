# Objects and Properties

This page defines object reference, lifetime, inheritance, casting, and property
behavior. See the [Objects guide](../language/objects.md) for shorter examples.

## Object Types and References

An `object` defines a reference type. A variable of object type contains a
reference, not the object's fields. It may contain `nil`.

```dq
object OCounter:
    value : int = 0
endobj

var counter : OCounter = nil
```

Assigning an object variable copies the reference. It does not copy the object
or transfer ownership.

## Storage Forms

`new OType(...)` allocates an object on the heap and returns a reference.
`delete reference` calls its destructor and releases heap storage.

`<-` creates fixed object storage associated with the containing scope or
aggregate:

```dq
var local_counter <- OCounter()
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
function rules. The compiler prevents accidental double invocation of the same
base lifetime operation.

## Members and Visibility

Fields, methods, and properties are declared in `public` or `private` groups.
Private members are accessible only where the declaring object permits. Method
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
new property value. A `default` indexed property handles indexing directly on
the object reference. At most one compatible default property may apply to an
index expression.

## Dispatch and Addressability

Accessor methods participate in normal virtual dispatch. A property itself is
not addressable and cannot bind directly to `ref`, `refin`, or `refout`.
Modify-assignment reads through the getter, performs the operation, and writes
through the setter; both accessors are therefore required.

Values returned from a property keep their own normal semantics. For example,
an object reference returned by a property can be used for member access.
