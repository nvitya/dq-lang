# DQ `anyval` and Runtime Type Information Specification

Status: draft 0.1

This document specifies the compact runtime type information records used by DQ RTL code and the first version of the `anyval` boxed value type.

`anyval` is intended for controlled heterogeneous value passing, especially formatting and database parameter binding:

```dq
print('{}, {}\n', [1, 2]);
DbExec('insert into t(a, b) values (?, ?)', [1, 'text']);
```

`anyval` is **not** a general dynamic object system. The initial supported value set is intentionally restricted.

---

## 1. Runtime type kind constants

The RTL defines broad runtime type categories as `uint8` constants:

```dq
const DQTK_VOID         : uint8 =  0;
const DQTK_INT          : uint8 =  1;
const DQTK_FLOAT        : uint8 =  2;
const DQTK_BOOL         : uint8 =  3;
const DQTK_POINTER      : uint8 =  4;
const DQTK_ENUM         : uint8 =  5;
const DQTK_CSTRING      : uint8 =  8;
const DQTK_STRVIEW      : uint8 =  9;
const DQTK_DYNSTR       : uint8 = 10;
const DQTK_ANYVAL       : uint8 = 15;
const DQTK_STRUCT       : uint8 = 16;
const DQTK_OBJECT       : uint8 = 17;
const DQTK_ARRAY        : uint8 = 20;
const DQTK_ARRAY_SLICE  : uint8 = 21;
const DQTK_DYN_ARRAY    : uint8 = 22;
const DQTK_FUNCTION     : uint8 = 28;
const DQTK_FUNCREF      : uint8 = 29;
const DQTK_ALIAS        : uint8 = 31;
```

The `kind` value describes the broad runtime category. Exact type identity is represented by the address of the corresponding `SDqTypeInfo` record.

---

## 2. `SDqTypeInfo`

```dq
struct SDqTypeInfo:
  storagesize  : uint32;   // array stride, including padding
  flags        : uint16;   // copy/destroy/init/move properties
  kind         : uint8;    // one of the DQTK_xxx constants
  subtype      : uint8;    // kind-specific type addition
  init_func    : pointer;  // reserved for managed type handling
  destroy_func : pointer;  // reserved for managed type handling
  copy_func    : pointer;  // reserved for managed type handling
  move_func    : pointer;  // reserved for managed type handling
endstruct
```

### 2.1 Field meanings

`storagesize` is the storage size and array stride of the type, including padding. It is limited to 32 bits because a single element type larger than 4 GiB is not a practical DQ target. Array total byte sizes may still use wider arithmetic.

`flags` describes the low-level lifetime behavior of values of this type. The exact bit allocation is defined by the RTL. Typical flag meanings are: requires initialization, requires destruction, requires non-trivial copy, requires non-trivial move, and managed value.

`kind` is one of the `DQTK_xxx` constants.

`subtype` is interpreted according to `kind`. It may describe integer width/sign, floating width, string representation details, or other small additions. `0` means the default subtype for that kind.

The function pointers are nullable for trivial types. For managed types, they point to RTL helper functions implementing initialization, destruction, copying, and moving.

### 2.2 Mutability

`SDqTypeInfo` records are generated as static runtime metadata. They must be treated as read-only after initialization, even if represented as `^SDqTypeInfo` in the current RTL source.

---

## 3. Array slice descriptor

```dq
struct SArraySliceDesc:
  dataptr : pointer;
  length  : uint;
endstruct
```

`SArraySliceDesc` is the runtime descriptor for `[]T` function parameters and other non-owning array views.

For `[]anyval`, `dataptr` points to the first `SDqAnyVal` element and `length` is the number of elements.

`SArraySliceDesc` does not own the element storage.

---

## 4. `SDqAnyVal`

```dq
struct SDqAnyVal:
  typeinfo : ^SDqTypeInfo;
  data     : [2]uint64;  // fits the SDqTextInfo
endstruct
```

`SDqAnyVal` is the runtime representation of the DQ builtin type `anyval`.

The `typeinfo` field identifies the boxed value type. The `data` field contains either the value bits directly or a compact value descriptor.

The first version of `anyval` is limited to simple scalar/text values. It must not be used as a universal boxed representation for arbitrary DQ values.

---

## 5. Supported `anyval` source values

The initial allowed implicit conversions to `anyval` are:

```text
null / void-value marker -> anyval
bool                     -> anyval
signed integer types     -> anyval
unsigned integer types   -> anyval
char                     -> anyval
floating-point types     -> anyval
str                      -> anyval
string literal           -> anyval as str
```

Recommended `typeinfo.kind` mappings:

```text
null marker        DQTK_VOID
bool               DQTK_BOOL
int/uint/char      DQTK_INT
float32/float64    DQTK_FLOAT
str                DQTK_DYNSTR
anyval             DQTK_ANYVAL
```

`char` is represented as an integer-like scalar. The exact distinction between signed integer, unsigned integer, and character is stored in `subtype` or in the exact `SDqTypeInfo` pointer.

String literals used in an `anyval` context are boxed as `str` values, not as raw `cstring` or `strview` values.

---

## 6. Values not supported by `anyval` v1

The following values are not accepted as `anyval` sources in the first version:

```text
pointer / ^T
ref T / refnull T
cstring
strview
arrays / [N]T
array slices / []T
dynamic arrays / [*]T
struct values
object values
function symbols
function references
enum values, unless explicitly converted to int or str
```

This restriction is intentional. It prevents `anyval` from becoming a hidden dynamic type system.

Future versions may allow selected secondary typed values by using type-info chaining or richer metadata, but this is outside the initial design.

---

## 7. `data` storage rules

For trivial scalar values, the value is stored directly in `data[0]`; `data[1]` is zero unless required by the subtype.

Recommended conventions:

```text
DQTK_VOID       data[0] = 0, data[1] = 0
DQTK_BOOL       data[0] = 0 or 1
DQTK_INT        data[0] = integer bits, sign/width from subtype/typeinfo
DQTK_FLOAT      data[0] = IEEE bits, width from subtype/typeinfo
DQTK_DYNSTR     data[0..1] = dynamic string/text descriptor compatible with SDqTextInfo
```

If the final dynamic string descriptor does not fit into `[2]uint64`, then `data[0]` shall contain a pointer to owned managed storage and `data[1]` shall be reserved.

---

## 8. Lifetime and ownership

`anyval` is a value type.

For scalar values, copying and destruction are trivial.

For `str` values, `anyval` must obey the normal `str` copy/destroy semantics. Therefore `anyval` itself is a managed type if it can contain `str`.

The `SDqTypeInfo` record for `anyval` must use the proper flags and helper functions so that arrays of `anyval` can be initialized, copied, moved, and destroyed safely.

Copying an `anyval` copies the contained value according to its contained `typeinfo`.

Destroying an `anyval` destroys the contained value according to its contained `typeinfo`.

---

## 9. `[]anyval` function parameters

The preferred argument-list type is the ordinary array view:

```dq
function print(fmt : str, values : []anyval);
function format(fmt : str, values : []anyval) -> str;
function DbExec(sql : str, params : []anyval);
```

There is no separate `anyvals` builtin type. Use standard DQ array forms:

```dq
[]anyval     // non-owning view
[*]anyval    // owning dynamic array
[N]anyval    // fixed-size static array
[?]anyval    // inferred-length static array
```

---

## 10. Array literal conversion to `[]anyval`

A normal array literal remains homogeneous by default.

When the expected target type is `[]anyval`, `[N]anyval`, `[?]anyval`, or `[*]anyval`, each element of the literal is individually boxed into `anyval`.

Example:

```dq
print('{}, {}\n', [1, 2]);
```

is lowered approximately to:

```dq
var __args : [?]anyval = [anyval(1), anyval(2)];
print('{}, {}\n', __args);
```

The temporary array created for a direct `[]anyval` function argument is valid for the duration of the call.

A `[]anyval` value is only a view and does not own the elements. Persistent storage must use `[N]anyval`, `[?]anyval`, or `[*]anyval`.

---

## 11. Type checking rules

Conversion from supported concrete value types to `anyval` is allowed in an `anyval` context.

Conversion from `anyval` back to a concrete type is explicit and checked.

Example:

```dq
var v : anyval = 123;

if v.IsInt():
  var i : int = v.AsInt();
endif
```

Implicit unboxing is not allowed:

```dq
var v : anyval = 123;
var i : int = v;      // error
```

---

## 12. Formatting use case

The canonical formatting call form is:

```dq
print('{}, {}\n', [1, 2]);
```

The format implementation uses the `[]anyval` descriptor to access the values and dispatches according to `value.typeinfo^.kind` and `value.typeinfo^.subtype`.

Format/value mismatches are runtime formatting errors, not ABI errors.

Example:

```dq
print('{} = {}\n', ['temperature', 23.5]);
```

---

## 13. Database use case

Database parameter binding can use `[]anyval` directly:

```dq
DbExec(
  'insert into sensor_log(name, value) values (?, ?)',
  ['vbat', 12.4]
);
```

`null`, integer, floating, boolean, and `str` values map naturally to common database field values.

Pointers, arrays, structs, and objects must be converted explicitly before binding.

---

## 14. Future extensions

Possible future extensions:

```text
- explicit enum boxing
- cstring/strview boxing with strict lifetime rules
- typed pointer boxing with chained typeinfo
- array/slice boxing with chained element typeinfo
- object/interface boxing
- richer subtype constants
- compile-time format string checking
```

These extensions must not weaken the first-version rule that `anyval` is a controlled boxed value type, not a general dynamic object model.
