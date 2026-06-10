# DQ `anyvalue` and Runtime Type Information Specification

Status: draft 0.3

This document specifies the compact runtime type information records used by DQ RTL code and the first version of the `anyvalue` boxed value type.

`anyvalue` is intended for controlled heterogeneous value passing, especially formatting, logging, and database parameter binding:

```dq
print('{}, {}\n', [1, 2]);
DbExec('insert into t(a, b) values (?, ?)', [1, 'text']);
```

`anyvalue` is **not** a general dynamic object system. The initial supported value set is intentionally restricted.

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
const DQTK_ANYVALUE     : uint8 = 15;
const DQTK_STRUCT       : uint8 = 16;
const DQTK_OBJECT       : uint8 = 17;
const DQTK_ARRAY        : uint8 = 20;
const DQTK_ARRAY_SLICE  : uint8 = 21;
const DQTK_DYN_ARRAY    : uint8 = 22;
const DQTK_FUNCTION     : uint8 = 28;
const DQTK_FUNCREF      : uint8 = 29;
const DQTK_ALIAS        : uint8 = 31;
```

The `kind` value describes the broad runtime category. Exact type identity may still be represented elsewhere by the address of an `SDqTypeInfo` record, but `anyvalue` v1 stores a compact inline type descriptor instead of a full `SDqTypeInfo` pointer.

---

## 2. Runtime subtype constants

Subtypes are interpreted according to `kind`.

The first required subtype is the signed-integer marker:

```dq
const DQSUBT_INT_SIGNED : byte = 1;
```

For `DQTK_INT`:

```text
subtype == DQSUBT_INT_SIGNED   signed integer
subtype != DQSUBT_INT_SIGNED   unsigned integer / integer-like scalar
```

For `DQTK_FLOAT`, the `datasize` field identifies the concrete storage width:

```text
datasize == 4   float32
datasize == 8   float64
```

The exact subtype encoding may be extended later for narrower integer widths, `char`, enum-origin information, or richer text metadata.

---

## 3. `SDqTypeInfo`

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

### 3.1 Field meanings

`storagesize` is the storage size and array stride of the type, including padding. It is limited to 32 bits because a single element type larger than 4 GiB is not a practical DQ target. Array total byte sizes may still use wider arithmetic.

`flags` describes the low-level lifetime behavior of values of this type. The exact bit allocation is defined by the RTL. Typical flag meanings are: requires initialization, requires destruction, requires non-trivial copy, requires non-trivial move, and managed value.

`kind` is one of the `DQTK_xxx` constants.

`subtype` is interpreted according to `kind`. It may describe integer width/sign, floating width, string representation details, or other small additions. `0` means the default subtype for that kind.

The function pointers are nullable for trivial types. For managed types, they point to RTL helper functions implementing initialization, destruction, copying, and moving.

### 3.2 Mutability

`SDqTypeInfo` records are generated as static runtime metadata. They must be treated as read-only after initialization, even if represented as `^SDqTypeInfo` in the current RTL source.

### 3.3 Relation to `anyvalue`

`SDqTypeInfo` is the general runtime type descriptor used by other RTL facilities.

`anyvalue` v1 does **not** store a `^SDqTypeInfo` in every value. Instead, it stores the compact fields `kind`, `subtype`, and `datasize`. This keeps `anyvalue` small and avoids turning it into a universal boxed-object mechanism.

---

## 4. Array slice descriptor

```dq
struct SArraySliceDesc:
  dataptr : pointer;
  length  : uint;
endstruct
```

`SArraySliceDesc` is the runtime descriptor for `[]T` function parameters and other non-owning array views.

For `[]anyvalue`, `dataptr` points to the first `SDqAnyValue` element and `length` is the number of elements.

`SArraySliceDesc` does not own the element storage.

---

## 5. `SDqAnyValue`

```dq
struct SDqAnyValue:
  data     : [16]byte;  // inline scalar bits or a text descriptor
  kind     : byte;      // one of the DQTK_xxx values
  subtype  : byte;      // kind-specific subtype
  datasize : byte;      // byte size of the stored scalar/descriptor when needed
  #if 4 == @def.PTRSIZE
    _pad   : [1]byte;
  #else
    _pad   : [5]byte;
  #endif
endstruct
```

`SDqAnyValue` is the runtime representation of the DQ builtin type `anyvalue`.

The `data` field contains either the value bits directly or a compact value descriptor. It is currently 16 bytes so that the common text descriptor can fit inline.

The `kind`, `subtype`, and `datasize` fields form the compact inline runtime type descriptor for `anyvalue` v1.

The padding keeps the complete record naturally aligned and pointer-size dependent without changing the public field semantics.

The first version of `anyvalue` is limited to simple scalar/text values. It must not be used as a universal boxed representation for arbitrary DQ values.

---

## 6. Supported `anyvalue` source values

The initial allowed implicit conversions to `anyvalue` are:

```text
database null marker   -> anyvalue
bool                   -> anyvalue
signed integer types   -> anyvalue
unsigned integer types -> anyvalue
char                   -> anyvalue
floating-point types   -> anyvalue
str                    -> anyvalue
cstring                -> anyvalue
strview                -> anyvalue
string literal         -> anyvalue as cstring
pointer/object         -> anyvalue (as pointer)
```

Recommended `kind` mappings:

```text
database null     DQTK_VOID with null marker
bool              DQTK_BOOL
int/uint/char     DQTK_INT
float32/float64   DQTK_FLOAT
str               DQTK_DYNSTR
cstring           DQTK_CSTRING
strview           DQTK_STRVIEW
pointer           DQTK_POINTER
anyvalue          DQTK_ANYVALUE
```

`char` is represented as an integer-like scalar. The exact distinction between signed integer, unsigned integer, and character is stored in `subtype` or by compiler-known source type information.

String literals used in an `anyvalue` context are boxed as `cstring` values. They reference static read-only storage and therefore satisfy the borrowed lifetime rules for `cstring`.

---

## 7. Values not supported by `anyvalue` v1

The following values are not accepted as normal user-level `anyvalue` sources in the first version:

```text
ref T / refnull T
arrays / [N]T
array slices / []T
dynamic arrays / [*]T
struct values
object values
function symbols
function references
enum values, unless explicitly converted to int or str
```

This restriction is intentional. It prevents `anyvalue` from becoming a hidden dynamic type system.

The RTL may contain low-level pointer handlers for special internal use. That does not mean ordinary pointer values are part of the public `anyvalue` v1 source set.

Future versions may allow selected secondary typed values by using type-info chaining or richer metadata, but this is outside the initial design.

---

## 8. `data` storage rules

For trivial scalar values, the value is stored directly in `data`. Unused bytes should be zeroed by setter functions when practical.

Recommended conventions:

```text
DQTK_VOID       data[0] = 1 for database NULL
DQTK_BOOL       data[0] = 0 or 1
DQTK_INT        data[0..datasize-1] = integer bits, sign from subtype
DQTK_FLOAT      data[0..datasize-1] = IEEE bits, width from datasize
DQTK_POINTER    data[0..@def.PTRSIZE-1]
DQTK_CSTRING    data[0..15] = borrowed text descriptor compatible with SDqTextInfo
DQTK_STRVIEW    data[0..15] = borrowed text descriptor compatible with SDqTextInfo
DQTK_DYNSTR     data[0..@def.PTRSIZE-1] = owned ODynStrMgr pointer
```

`DQTK_DYNSTR` uses the same `ODynStrMgr` manager object as the normal DQ `str` type. The `data` field stores only the manager pointer. A null manager pointer represents the empty string, matching normal `str` behavior.

`DQTK_CSTRING` and `DQTK_STRVIEW` are borrowed descriptor values. `DQTK_DYNSTR` is owned/refcounted managed storage.

---

## 9. Database null value

Database null is represented as a special `anyvalue` value. It is not the same thing as a null pointer.

The public API is:

```dq
function AnyValIsNull(v : ref SDqAnyValue) -> bool;
function AnyValSetNull(v : ref SDqAnyValue);
```

User-level method form:

```dq
var av : anyvalue;

av.SetNull();

if av.IsNull():
  // handle SQL/database NULL
endif
```

Recommended internal representation:

```text
kind    = DQTK_VOID
data[0] = 1
```

The old boolean form `SetNull(true)` is obsolete. User code should only use `SetNull()` and `IsNull()`.

---

## 10. Lifetime and ownership

`anyvalue` is a value type.

For scalar values, copying and destruction are trivial.

For `cstring` and `strview` values, `anyvalue` stores a borrowed text descriptor. Copying the `anyvalue` copies only the descriptor. Destroying the `anyvalue` does not destroy the referenced text storage.

Implicit boxing of `cstring` and `strview` is only allowed when the source text storage is guaranteed to remain valid for the lifetime of the resulting `anyvalue`. Direct `[]anyvalue` call arguments satisfy this rule for ordinary expression sources because the temporary array is only valid for the duration of the call. Persistent `anyvalue` storage may require an explicit conversion to owned `str`.

For `str` values, `anyvalue` must obey the normal `str` copy/destroy semantics. Therefore `anyvalue` itself is a managed type if it can contain owned `str`.

The `SDqTypeInfo` record for `anyvalue` must use the proper flags and helper functions so that arrays of `anyvalue` can be initialized, copied, moved, and destroyed safely.

Copying an `anyvalue` copies the contained value according to its contained `kind` and ownership mode.

Destroying an `anyvalue` destroys the contained value according to its contained `kind` and ownership mode.

The RTL lifetime helper ABI is:

```dq
function AnyValDestroy(v : ref SDqAnyValue);
function AnyValCopy(dst : ref SDqAnyValue, src : ref SDqAnyValue);
function AnyValMove(dst : ref SDqAnyValue, src : ref SDqAnyValue);
```

`AnyValDestroy()` releases the contained dynamic string when `kind == DQTK_DYNSTR` and leaves the value as `DQTK_VOID`.

`AnyValCopy()` releases the previous destination value, copies the source record, and increments the dynamic string refcount when the copied value is `DQTK_DYNSTR`.

`AnyValMove()` releases the previous destination value, transfers the source record without incrementing the dynamic string refcount, and leaves the source as `DQTK_VOID`.

---

## 11. Handler function naming model

The RTL exposes low-level free functions operating on `ref SDqAnyValue`.

The compiler may expose these as methods on the builtin `anyvalue` type:

```text
av.IsInt()         lowers to AnyValIsInt(av)
av.AsInt(defval)  lowers to AnyValAsInt(av, defval)
av.SetInt(value)  lowers to AnyValSetInt(av, value)
av = value        lowers to the matching AnyValSetXxx(av, value)
```

The low-level function names are part of the RTL ABI. The method names are the user-facing convenience form.

Setter functions overwrite the previous contained value. If the previous value is managed, the compiler or setter must first destroy/release the previous contained value according to the `anyvalue` lifetime rules.

Getter functions named `AsXxx` do not raise conversion errors. If the stored value cannot be converted to the requested result type, the supplied `defval` is returned.

Predicate functions named `IsXxx` test the stored category. They do not mean that every conversion to that category is necessarily lossless.

---

## 12. Required scalar handler functions

### 12.1 Null

```dq
function AnyValIsNull(v : ref SDqAnyValue) -> bool;
function AnyValSetNull(v : ref SDqAnyValue);
```

Method form:

```dq
av.IsNull();
av.SetNull();
```

`AnyValSetNull()` sets a database null value.

### 12.2 Number category

```dq
function AnyValIsNumber(v : ref SDqAnyValue) -> bool;
```

Method form:

```dq
av.IsNumber();
```

`AnyValIsNumber()` returns `true` for stored integer and floating-point values.

### 12.3 Integer

```dq
function AnyValIsInt(v : ref SDqAnyValue) -> bool;
function AnyValIsSInt(v : ref SDqAnyValue) -> bool;
function AnyValIsUint(v : ref SDqAnyValue) -> bool;
function AnyValAsInt(v : ref SDqAnyValue, defval : int) -> int;
function AnyValAsUint(v : ref SDqAnyValue, defval : uint) -> uint;
function AnyValSetInt(v : ref SDqAnyValue, value : int);
function AnyValSetUInt(v : ref SDqAnyValue, value : uint);
```

Method form:

```dq
av.IsInt();
av.IsSInt();
av.IsUint();

var i : int  = av.AsInt(-1);
var u : uint = av.AsUint(0);

av.SetInt(-123);
av.SetUInt(123);
```

`AnyValIsInt()` tests whether the stored value is integer-kind.

`AnyValIsSInt()` tests whether the stored value is integer-kind and has the signed subtype.

`AnyValIsUint()` tests whether the stored value is integer-kind and does not have the signed subtype.

`AnyValAsInt()` accepts stored integer values directly. It may also accept stored floating-point values using DQ `Round()`.

`AnyValAsUint()` accepts stored integer values directly. It may also accept stored floating-point values using DQ `Round()`.

If a numeric conversion cannot be represented by the result type, the default value is returned.

### 12.4 Boolean

```dq
function AnyValIsBool(v : ref SDqAnyValue) -> bool;
function AnyValAsBool(v : ref SDqAnyValue, defval : bool) -> bool;
function AnyValSetBool(v : ref SDqAnyValue, value : bool);
```

Method form:

```dq
av.IsBool();

var b : bool = av.AsBool(false);

av.SetBool(true);
```

`AnyValAsBool()` accepts stored boolean values. Other stored kinds return `defval`.

### 12.5 Floating point

```dq
function AnyValIsFloat(v : ref SDqAnyValue) -> bool;
function AnyValIsFloat32(v : ref SDqAnyValue) -> bool;
function AnyValIsFloat64(v : ref SDqAnyValue) -> bool;
function AnyValAsFloat(v : ref SDqAnyValue, defval : float) -> float;
function AnyValAsFloat32(v : ref SDqAnyValue, defval : float32) -> float32;
function AnyValAsFloat64(v : ref SDqAnyValue, defval : float64) -> float64;
function AnyValSetFloat(v : ref SDqAnyValue, value : float);
function AnyValSetFloat32(v : ref SDqAnyValue, value : float32);
function AnyValSetFloat64(v : ref SDqAnyValue, value : float64);
```

Method form:

```dq
av.IsFloat();
av.IsFloat32();
av.IsFloat64();

var f  : float   = av.AsFloat(-1.0);
var f4 : float32 = av.AsFloat32(-1.0);
var f8 : float64 = av.AsFloat64(-1.0);

av.SetFloat(3.14);
av.SetFloat32(3.14);
av.SetFloat64(3.14);
```

`AnyValIsFloat()` tests whether the stored value is floating-point-kind.

`AnyValIsFloat32()` and `AnyValIsFloat64()` inspect the stored floating-point width through `datasize`.

`AnyValAsFloat()`, `AnyValAsFloat32()`, and `AnyValAsFloat64()` accept stored floating-point values directly. They may also accept stored integer values through normal integer-to-float conversion.

If a numeric conversion cannot be represented by the result type, the default value is returned.

### 12.6 Pointer handlers

The RTL may provide pointer handlers for internal or implementation-specific use:

```dq
function AnyValIsPointer(v : ref SDqAnyValue) -> bool;
function AnyValAsPointer(v : ref SDqAnyValue, defval : pointer) -> pointer;
function AnyValSetPointer(v : ref SDqAnyValue, value : pointer);
```

These handlers do not make ordinary pointer values part of the public `anyvalue` v1 source set.

A language implementation may hide these functions from normal user code, or require an explicit unsafe/internal context.

---

## 13. Required text handler functions

The text handlers use `SDqTextInfo` as the canonical heapless text descriptor.

The exact `SDqTextInfo` structure is specified by the DQ string RTL. The `SDqAnyValue.data` field must be large enough to store the supported inline text descriptor or a pointer to managed text storage.

### 13.1 Text predicates

```dq
function AnyValIsStr(v : ref SDqAnyValue) -> bool;
function AnyValIsText(v : ref SDqAnyValue) -> bool;
```

`AnyValIsStr()` is the current RTL-compatible name.

`AnyValIsText()` is the preferred semantic alias. It returns `true` for `DQTK_CSTRING`, `DQTK_STRVIEW`, and `DQTK_DYNSTR`.

A compiler may expose only the method name `IsStr()` or may expose both `IsStr()` and `IsText()`.

Method form:

```dq
av.IsStr();
av.IsText();
```

### 13.2 Heapless text descriptor extraction

```dq
function AnyValAsText(
  v      : ref SDqAnyValue,
  defval : refin SDqTextInfo,
  rv     : refout SDqTextInfo
);
```

Method form:

```dq
var ti : SDqTextInfo;
av.AsText(DefaultTextInfo(""), ti);
```

`AnyValAsText()` returns a text descriptor through `rv`.

If `v` contains `cstring`, `strview`, or `str`, the result describes the contained text.

If `v` does not contain text, `rv` receives `defval`.

This function must not allocate. It returns or copies only a descriptor. The descriptor may refer to borrowed storage, so the caller must respect the source lifetime.

### 13.3 Text setter functions

```dq
function AnyValSetText(v : ref SDqAnyValue, ati : refin SDqTextInfo);
function AnyValSetCString(v : ref SDqAnyValue, ati : refin SDqTextInfo);
function AnyValSetStr(v : ref SDqAnyValue, value : str);
function AnyValSetStrText(v : ref SDqAnyValue, ati : ref SDqTextInfo);
```

`AnyValSetText()` stores a borrowed `DQTK_STRVIEW` descriptor.

`AnyValSetCString()` stores a borrowed `DQTK_CSTRING` descriptor.

`AnyValSetStr()` stores an owned/refcounted dynamic string reference. It increments the source manager refcount through normal dynamic string assignment rules.

`AnyValSetStrText()` copies text described by `SDqTextInfo` into owned dynamic string storage and stores it as `DQTK_DYNSTR`.

### 13.4 Heapless copy into fixed `cstring`

```dq
function AnyValToCString(
  v      : ref SDqAnyValue,
  defval : refin SDqTextInfo,
  rv     : cstring
);
```

Method form:

```dq
var cs : cstring(63);
av.ToCString(DefaultTextInfo(""), cs);
```

`AnyValToCString()` converts the contained value to text and copies it into caller-provided `cstring` storage.

If `v` contains text, the text is copied.

If `v` contains a numeric or boolean value, the implementation may format it using the default DQ text formatting rules.

If `v` cannot be converted to text, `defval` is copied.

This function must not allocate. Truncation and bounds behavior follow the normal `cstring` assignment/copy rules.

### 13.5 Owned dynamic string extraction

```dq
function AnyValAsStr(v : ref SDqAnyValue, defval : refin SDqTextInfo) -> str;
```

Method form:

```dq
var s : str = av.AsStr(DefaultTextInfo(""));
```

`AnyValAsStr()` returns an owned dynamic `str` value and may allocate.

If `v` contains text, the result is a dynamic string copy of that text.

If `v` contains a numeric or boolean value, the implementation may format it using the default DQ text formatting rules.

If `v` cannot be converted to text, `defval` is copied into the returned dynamic string according to normal `str` rules.

---

## 14. User-level basic usage

```dq
var av : anyvalue;

av.SetNull();       // database NULL

av.SetInt(123);
av = 123;           // same as SetInt(123)

av.SetFloat(3.14);
av = 3.14;          // same as SetFloat(3.14)

av.SetBool(true);
av = true;          // same as SetBool(true)
```

The `IsXxx()` functions test the stored value category:

```dq
if av.IsNull():
  // database NULL
elif av.IsInt():
  var i : int = av.AsInt(-1);
elif av.IsFloat():
  var f : float = av.AsFloat(-1.0);
elif av.IsBool():
  var b : bool = av.AsBool(false);
elif av.IsStr():
  var cs : cstring(63);
  av.ToCString(DefaultTextInfo(""), cs);
endif
```

The `AsXxx(defval)` functions never raise a conversion error. If the stored value cannot be converted to the requested result type, `defval` is returned.

Numeric `AsXxx()` functions may perform normal numeric conversions. For example, `AsInt()` accepts integer values directly and floating-point values through `Round()`:

```dq
av = 2.75;
var i : int = av.AsInt(-1);  // i == 3
```

If the conversion overflows the target type, the default value is returned.

Text values can be read in three forms:

```dq
var ti : SDqTextInfo;
av.AsText(DefaultTextInfo(""), ti);     // heapless descriptor result

var cs : cstring(63);
av.ToCString(DefaultTextInfo(""), cs);  // heapless copy into fixed buffer

var s : str = av.AsStr(DefaultTextInfo(""));  // owned dynamic string, may allocate
```

`cstring` and `strview` values stored in an `anyvalue` are borrowed. The referenced text storage must remain valid for the lifetime of the `anyvalue`.

A `str` value stored in an `anyvalue` is owned and follows normal managed string copy/destroy rules.

---

## 15. `[]anyvalue` function parameters

The preferred argument-list type is the ordinary array view:

```dq
function print(fmt : str, values : []anyvalue);
function format(fmt : str, values : []anyvalue) -> str;
function DbExec(sql : str, params : []anyvalue);
```

There is no separate `anyvalues` builtin type. Use standard DQ array forms:

```dq
[]anyvalue     // non-owning view
[*]anyvalue    // owning dynamic array
[N]anyvalue    // fixed-size static array
[?]anyvalue    // inferred-length static array
```

---

## 16. Array literal conversion to `[]anyvalue`

A normal array literal remains homogeneous by default.

When the expected target type is `[]anyvalue`, `[N]anyvalue`, `[?]anyvalue`, or `[*]anyvalue`, each element of the literal is individually boxed into `anyvalue`.

Example:

```dq
print('{}, {}\n', [1, 2]);
```

is lowered approximately to:

```dq
var __args : [?]anyvalue = [anyvalue(1), anyvalue(2)];
print('{}, {}\n', __args);
```

The temporary array created for a direct `[]anyvalue` function argument is valid for the duration of the call.

A `[]anyvalue` value is only a view and does not own the elements. Persistent storage must use `[N]anyvalue`, `[?]anyvalue`, or `[*]anyvalue`.

---

## 17. Type checking rules

Conversion from supported concrete value types to `anyvalue` is allowed in an `anyvalue` context.

Conversion from `anyvalue` back to a concrete type is explicit and checked through `AsXxx(defval)` or future `TryAsXxx()` helpers.

Example:

```dq
var v : anyvalue = 123;

if v.IsInt():
  var i : int = v.AsInt(-1);
endif
```

Implicit unboxing is not allowed:

```dq
var v : anyvalue = 123;
var i : int = v;      // error
```

---

## 18. Formatting use case

The canonical formatting call form is:

```dq
print('{}, {}\n', [1, 2]);
```

The format implementation uses the `[]anyvalue` descriptor to access the values and dispatches according to `value.kind`, `value.subtype`, and `value.datasize`.

Format/value mismatches are runtime formatting errors, not ABI errors.

Example:

```dq
print('{} = {}\n', ['temperature', 23.5]);
```

---

## 19. Database use case

Database parameter binding can use `[]anyvalue` directly:

```dq
DbExec(
  'insert into sensor_log(name, value) values (?, ?)',
  ['vbat', 12.4]
);
```

Database null values are passed explicitly:

```dq
var n : anyvalue;
n.SetNull();

DbExec(
  'insert into sensor_log(name, value) values (?, ?)',
  ['vbat', n]
);
```

Database binders should test `IsNull()` before testing scalar or text categories.

`null`, integer, floating, boolean, and text values map naturally to common database field values.

Pointers, arrays, structs, and objects must be converted explicitly before binding.

---

## 20. Future extensions

Possible future extensions:

```text
- explicit enum boxing
- typed pointer boxing with chained typeinfo
- array/slice boxing with chained element typeinfo
- object/interface boxing
- richer subtype constants
- TryAsXxx() handlers returning success/failure separately from the result value
- compile-time format string checking
```

These extensions must not weaken the first-version rule that `anyvalue` is a controlled boxed value type, not a general dynamic object model.
