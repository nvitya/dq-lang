# DQ-Lang Object Type Information & Casting

This document outlines the design for Runtime Type Information (RTTI) and dynamic type casting for polymorphic objects in DQ-lang.

## 1. TypeInfo for Polymorphic Objects

To safely support dynamic casting and exception matching, the compiler needs to generate lightweight Runtime Type Information (RTTI). 

### Implementation
The compiler will generate a tiny `TypeInfo` metadata struct for each class, containing at minimum:
* The class name.
* A pointer to its base class's `TypeInfo`.

A pointer to this `TypeInfo` struct will be embedded inside the object's `vtable`.

### What requires TypeInfo?
* **Polymorphic Objects Only:** Only classes that participate in inheritance (objects with a `vtable`) strictly require a `TypeInfo` pointer embedded in them. Their exact type can change at runtime, so the runtime needs a way to verify it.
* **Exceptions:** Adding `TypeInfo` drastically simplifies exception matching. `catch Exception e:` simply checks if the active exception's `TypeInfo` chain contains `Exception`. This eliminates the need for generating and parsing `type_chain` strings (e.g., `"ESegFault|Exception"`) in `DqExcRaise`.
* **Not Required for Primitives/Structs:** Value types (`int`, `float`, fixed arrays, structs) do not need embedded `TypeInfo` because their type is 100% known at compile time.
* **Not Required for `anyvalue`:** The `anyvalue` container correctly uses a simple `kind` enum (e.g., `DQTK_INT`) for tracking primitive types. It does not require a full `TypeInfo` struct unless full C#-style reflection is added in the future.

## 2. Object Type Testing (`is` operator)

To test if an object is of a specific type (or inherits from it), the `is` keyword is used.

### Syntax
```dq
if o is OSecond:
  // o is guaranteed to be compatible with OSecond
endif
```

### Advantages
* Universally understood and highly readable.
* Avoids heavy C++ style function calls.
* Under the hood, this simply walks the `TypeInfo` pointers starting from `o.vtable->typeinfo` to see if it matches the target type.

## 3. Dynamic Casting (`tryfrom`)

When a developer needs to cast an object to a derived type and use it, the `tryfrom` special statement is used. If the object is not compatible with the target type, the result is `null`.

### Syntax
```dq
var o2 : OSecond tryfrom o

if o2 <> null:
  o2.DoSomething()
endif
```

### Why `tryfrom`?
This syntax was specifically chosen for several reasons:
1. **Zero Repetition:** The target type (`OSecond`) is only written once.
2. **Highly Readable:** It reads like a natural English sentence ("var o2 is an OSecond, tried from o").
3. **Safety Warning:** The `try` prefix explicitly warns the programmer that the cast might fail and return `null`.
4. **Parser-Friendly:** Because it acts as an alternative assignment operator (replacing `=`), the semantic analyzer has immediate access to the target type (`OSecond`) in the AST node. The compiler does not need to perform complex top-down type inference.

### Why not implicit dynamic casting?
It might seem tempting to make the standard assignment operator (`=`) perform a silent dynamic cast (where `var o2 : OSecond = o` results in `null` if incompatible). 

**This is strictly avoided in DQ-lang.** 

Silently injecting a dynamic cast destroys compile-time type safety. If a developer makes a simple typo and assigns the wrong variable, an implicit cast would quietly assign `null` instead of throwing a compile-time "Type Mismatch" error. This would turn easily-preventable compile errors into mysterious Null Pointer Exceptions at runtime. 

By requiring an explicit keyword like `tryfrom`, the programmer is forced to declare their intent: *"I know this cast might fail at runtime, and I am prepared to handle the `null` result."*
