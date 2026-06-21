# DQ Property Specification

## Overview

Properties provide controlled access to object data through fields and/or accessor
functions.

A property can be:

- Directly mapped to a field
- Read through a getter function
- Written through a setter function
- Indexed (array-like)
- Marked as the default property

Properties are only allowed inside objects. A property introduces no per-instance
storage of its own.

---

## Declaration Syntax

Property declarations have the following form:

```dq
property name : property-type [default] [read read-accessor] [write write-accessor]
```

The specifiers have a fixed order:

1. `default`, when present
2. `read`, when present
3. `write`, when present

At least one of `read` or `write` is required. A repeated or out-of-order
specifier is a compile-time error.

### Simple Property

```dq
property x : int read m_x write SetX
```

### Read-Only Property

```dq
property count : int read GetCount
```

### Write-Only Property

```dq
property password : str write SetPassword
```

### Field-Mapped Property

```dq
property name : str read m_name write m_name
```

---

## Declaration Order and Name Lookup

An accessor named by a property must already have been declared when the property
declaration is parsed.

An accessor may be a field or method declared earlier in the same object, or an
accessible member inherited from an ancestor object. Normal member visibility
rules apply. In particular, a private member of an ancestor cannot be used by a
derived object's property declaration.

The accessor itself does not need to have the same visibility as the property.
A public property may use private accessors declared by the same object. Access
through the property is checked using the property's visibility; the caller does
not gain direct access to its private accessor.

Example:

```dq
object OExample:
private
  var m_x : int

  function SetX(avalue : int):
    m_x = avalue
  endfunc

public
  property x : int read m_x write SetX
endobj
```

---

## Accessors

Accessor signatures are matched exactly. Normal implicit argument or result
conversions are not used to make an accessor compatible with a property.

When an accessor name denotes an overload set, normal overload resolution is
performed using the required property accessor signature. Failure to find
exactly one compatible accessor is a compile-time error.

### Getter

A getter returns the property value:

```dq
function GetCount() -> int
```

For a non-indexed property, a getter method must:

- Take no explicit parameters
- Return exactly the property type
- Not return `void`

A field used by `read` must have exactly the property type.

### Setter

A setter receives the assigned value:

```dq
function SetX(avalue : int):
```

For a non-indexed property, a setter method must:

- Take exactly one explicit parameter
- Take the assigned value as its last parameter
- Declare that parameter by value or `refin`, not `ref` or `refout`
- Use exactly the property type for that parameter
- Return `void`

A field used by `write` must have exactly the property type.

---

## Usage

```dq
obj.x = 123

var a : int = obj.x
```

Reading a property invokes its getter or reads its backing field. Assigning to a
property invokes its setter or writes its backing field.

Assigning to a read-only property is a compile-time error. Reading a write-only
property is a compile-time error.

---

## Indexed Properties

Indexed properties provide array-like access. Their index list is part of the
property declaration; it does not declare an ordinary DQ array type.

### Syntax

An unnamed index type may be declared as:

```dq
property items : [int]int read GetItem write SetItem
```

For documentation and diagnostics, indices may be named:

```dq
property items : [index : int]int read GetItem write SetItem
```

Both forms declare an indexed property with an `int` index and an `int` value.

Example:

```dq
property field : [name : str]OField read GetField write SetField
```

Usage:

```dq
var f : OField = rec.field["Name"]

rec.field["Name"] = newfield
```

Indexed properties must use methods as their accessors. A field cannot be used
as the `read` or `write` accessor of an indexed property.

Index types may be any valid parameter type supported by an ordinary function.
Index parameters may be passed by value or `refin`, but not by `ref` or
`refout`.

---

## Multi-Index Properties

Multiple indices are declared in one bracket pair:

```dq
property cells : [int, int]str read GetCell write SetCell
```

They may be named for readability:

```dq
property cells : [row : int, col : int]str read GetCell write SetCell
```

Usage also uses one bracket pair with comma-separated indices:

```dq
var s : str = grid.cells[2, 5]

grid.cells[2, 5] = "Hello"
```

---

## Indexed Property Accessors

For:

```dq
property items : [index : int]int read GetItem write SetItem
```

the accessors are:

```dq
function GetItem(index : int) -> int

function SetItem(index : int, avalue : int):
```

For:

```dq
property cells : [row : int, col : int]str read GetCell write SetCell
```

the accessors are:

```dq
function GetCell(row : int, col : int) -> str

function SetCell(row : int, col : int, avalue : str):
```

An indexed getter must take exactly the declared index parameters, in order, and
return exactly the property value type.

An indexed setter must take exactly the declared index parameters, in order,
followed by the assigned value as its last parameter. The assigned-value
parameter must have exactly the property value type. A setter returns `void`.

The parameter modes and types of accessor index parameters must exactly match
those in the property declaration.

The names of accessor parameters do not need to match the optional names in the
property declaration.

### Indexed Assignment

Assignment to an indexed property calls its setter directly:

```dq
obj.items[2] = 10
```

is equivalent to:

```dq
obj.SetItem(2, 10)
```

It does not first call the getter and does not mutate a temporary getter result.
If the indexed property has no setter, the assignment is a compile-time
`read-only property` error.

The receiver, index expressions, and assigned-value expression are each
evaluated exactly once.

---

## Default Properties

An indexed property may be marked as the default property of an object:

```dq
property field : [name : str]OField default read GetField write SetField
```

Only indexed properties may use `default`.

The name of a default property may be omitted at the access site:

```dq
var f : OField = rec["Name"]

rec["Name"] = newfield
```

These expressions are equivalent to:

```dq
var f : OField = rec.field["Name"]

rec.field["Name"] = newfield
```

An object has at most one effective default property. More than one property may
be declared `default`, but the last applicable declaration wins. Within one
object this is the last `default` property in declaration order. Across
inheritance it is the last declaration encountered from the base object toward
the receiver's declared object type; a derived default property therefore
replaces an inherited default property.

Property lookup, including default-property lookup, is based on the declared
compile-time type of the receiver. Therefore, a derived default property is used
only when the receiver expression has the derived type.

---

## Inheritance and Dispatch

Properties are inherited as object members and follow normal member visibility
rules.

A property declaration in a derived object with the same name hides the
inherited property. The derived declaration is a complete property declaration
and must specify its type and at least one accessor. Property lookup is static
and uses the declared compile-time type of the receiver.

Calling a method accessor follows normal method dispatch rules. If the selected
accessor method is virtual, invoking it through a property performs virtual
dispatch in the same way as an ordinary call to that method.

---

## Addressability

A property is not a storage location, even when it is backed directly by a
field. Therefore a property itself:

- Has no address
- Cannot be passed as a `ref` or `refout` argument
- Cannot be bound to a reference
- Cannot be used where an addressable lvalue is required

A property may return any otherwise valid property type, including an object
reference or pointer. The returned object or pointed-to value has its normal
addressability and lifetime semantics; this does not make the property itself
addressable.

Direct assignment to a writable property is the only property operation that
uses it as an assignment target.

---

## Notes

- Properties are a compile-time language feature.
- Properties introduce no per-instance storage.
- Simple properties may map directly to fields or accessor methods.
- Indexed properties use accessor methods only.
- Indexed properties support arbitrary valid parameter types and multiple
  indices.
- Accessor references must resolve to members declared earlier or inherited from
  an ancestor.
- Property lookup is static, while calls to virtual accessor methods retain
  normal virtual dispatch.
