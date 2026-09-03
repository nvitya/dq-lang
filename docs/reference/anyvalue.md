# `anyvalue`

`anyvalue` is DQ's restricted boxed-value type. It supports heterogeneous API
arguments such as formatting and database parameters; it is not a general
dynamic object system. See the [`anyvalue` runtime guide](../rtl/anyvalue.md)
for its convenience methods.

## Supported Values

An `anyvalue` can represent:

- database null;
- signed and unsigned integers;
- Boolean values;
- `float32`, `float64`, and `float`;
- typed and generic pointers;
- `str`, `strview`, and `cstring` text values.

Objects, structures, function references, and arbitrary arrays are not boxed as
general secondary values. An array literal may nevertheless convert element by
element to `[]anyvalue` or another accepted array context.

```dq
PrintLn("{}: {}", ["answer", 42])
```

## Runtime Type Information

The value records a broad kind, a subtype describing width or representation,
and data sufficient for the stored value. The public contract is exposed through
predicates and accessors; programs must not depend on the private bit layout.

## Lifetime

Boxing a managed `str` retains the string storage correctly. Borrowed text and
pointer values do not extend the lifetime of the storage they refer to. The
caller must ensure such storage remains valid for every use of the boxed value.

Copying or destroying an `anyvalue` performs the corresponding handler action
for its stored kind.

## Null

Database null is distinct from numeric zero, Boolean false, an empty string, and
a nil pointer. Use `SetNull()` and `IsNull()`.

## Checked Access

Predicates such as `IsInt`, `IsFloat`, `IsPointer`, and `IsText` inspect the
stored category. `As...` methods return a permitted conversion or the supplied
default value. They do not define unrestricted casts between unrelated kinds.

For APIs that need to distinguish failure from a legitimate default value, test
the kind before calling the accessor.

## Array Parameters

Formatting-style functions normally accept `[]anyvalue`. A contextual array
literal creates the temporary boxed elements for the duration of the call.
Retaining a view of that temporary array beyond the call is invalid.
