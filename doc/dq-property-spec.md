# dq_property_spec.md

# DQ Property Specification

## Overview

Properties provide controlled access to object data through fields and/or accessor functions.

A property can be:

- Directly mapped to a field
- Read through a getter function
- Written through a setter function
- Indexed (array-like)
- Marked as the default property

Properties are only allowed inside objects.

---

## Syntax

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

## Accessors

### Getter

A getter returns the property value.

```dq
function GetCount() -> int
```

### Setter

A setter receives the assigned value.

```dq
function SetX(avalue : int)
```

The setter return value, if any, is ignored.

The last parameter of a property setter must be the value being assigned.

---

## Usage

```dq
obj.x = 123

var a : int = obj.x
```

The compiler translates property access to the corresponding getter/setter or direct field access.

---

## Indexed Properties

Indexed properties provide array-like access.

### Syntax

```dq
property items : [int]int read GetItem write SetItem
```

The form:

```dq
[IndexType]ValueType
```

declares an indexed property.

Example:

```dq
property field : [str]OField read GetField write SetField
```

Usage:

```dq
var f : OField = rec.field["Name"]

rec.field["Name"] = newfield
```

---

## Multi-Dimensional Indexed Properties

Multiple index types may be specified.

### Declaration

```dq
property cells : [int, int]str read GetCell write SetCell
```

### Usage

```dq
var s : str = grid.cells[2, 5]

grid.cells[2, 5] = "Hello"
```

DQ follows Delphi behavior and uses a single bracket pair with comma-separated indices.

---

## Indexed Property Accessors

For:

```dq
property items : [int]int read GetItem write SetItem
```

the accessors are:

```dq
function GetItem(index : int) -> int

function SetItem(index : int, avalue : int)
```

For:

```dq
property cells : [int, int]str read GetCell write SetCell
```

the accessors are:

```dq
function GetCell(row : int, col : int) -> str

function SetCell(row : int, col : int, avalue : str)
```

The assigned value is always passed as the last parameter.

---

## Default Properties

A property may be marked as the default property of an object.

### Declaration

```dq
property field : [str]OField default read GetField write SetField
```

Only one default property may exist in an object.

### Usage

```dq
var f : OField = rec["Name"]

rec["Name"] = newfield
```

The property name is omitted and the access is redirected to the default property.

Default properties may be indexed only.

---

## Notes

- Properties are a compile-time language feature.
- Properties do not occupy storage.
- Properties may map directly to fields or to accessor functions.
- Indexed properties support arbitrary index types.
- Multiple indices are supported.
- Only one default property is allowed per object.
- Property access syntax is identical whether backed by a field or by functions.
