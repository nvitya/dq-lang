# DQM Interface Format (`.dqm_if`)

Status: draft  
Scope: binary storage format for the DQ compiled module interface payload.

This document specifies the `.dqm_if` payload format only. The same payload may be stored:

```text
module.dqm_if              standalone interface file
module.dqm:.dqm_if section embedded in complete object-file artifact
```

The payload is not a native object file by itself. It is compiler metadata used to restore the public interface of one DQ module.

---

## 1. Design Goals

The `.dqm_if` format is designed for:

```text
fast machine parsing
whole-interface loading
strict validation
simple writer/reader implementation
easy regeneration on failure
future format evolution by versioning
```

The format is not designed for:

```text
partial loading
lazy symbol loading
cross-version best-effort compatibility
human editing
long-term stable ABI interchange between unrelated compilers
```

A `.dqm_if` load either fully succeeds or fully fails.

If loading fails because the file is missing, stale, unsupported, corrupt, or incompatible with the current target/configuration, the compiler may regenerate the file once and retry loading. If the second load fails, compilation reports a hard error.

---

## 2. High-Level Layout

```text
+-----------------------------+
| compact global DQMIF header |
+-----------------------------+
| header record group         |
|   HEADER_BEGIN              |
|   HEADER_DATA               |
|   ...                       |
|   HEADER_END                |
+-----------------------------+
| record stream               |
|   record                    |
|   record                    |
|   ...                       |
|   END record                |
+-----------------------------+
```

The compact global header contains only the fields needed for fast rejection and for finding the
record stream. Descriptive metadata such as module path, compiler version text, target
triple, ABI name, and build configuration is stored in the header record group.

The record stream is a strict binary token stream. Records are interpreted according to parser state.

Example conceptual stream:

```text
HEADER_BEGIN
  HEADER_DATA module_path "dqgui/widgets/button"
  HEADER_DATA target_triple "x86_64-linux-gnu"
HEADER_END

MODULE_START
  NAME "dqgui/widgets/button"
  TARGET "x86_64-linux-gnu"
  IMPORT "dqgui/core"

  OBJECT_START
    NAME "TButton"
    BASE_TYPE "dqgui/widgets/widget.TWidget"
    SIZE_ALIGN 32, 8

    FIELD_START
      NAME "text"
      TYPE "str"
      OFFSET 0
    FIELD_END

    FIELD_START
      NAME "visible"
      TYPE "bool"
      OFFSET 16
    FIELD_END
  OBJECT_END

  FUNCTION_START
    NAME "NewButton"
    PARAM_START
      NAME "text"
      TYPE "str"
    PARAM_END
    RESULT_TYPE "TButton"
  FUNCTION_END
MODULE_END
END
```

---

## 3. Byte Order and Alignment

The file encoding is canonical and independent of the target CPU byte order.

Recommended v1 rules:

```text
integer encoding: little-endian
record-header alignment: 4 bytes
payload alignment: payload padded with zero bytes to next 4-byte boundary
record length: excludes padding
strings: UTF-8 bytes; length from record header; not zero-terminated
```

The target endianness is stored as metadata. It does not affect the `.dqm_if` file encoding.

---

## 4. Compact Global Header

The payload begins with a compact fixed-size global header. The fixed header exists so the
reader can quickly reject files that are definitely not readable before running the
stateful record parser.

Suggested v1 header shape:

```cpp
struct TDqmIfHeader
{
  char     magic[8];              // "DQMIF\0\0\0"

  uint16_t format_major;          // incompatible format version
  uint16_t format_minor;          // diagnostic / controlled evolution
  uint16_t header_size;           // sizeof(TDqmIfHeader) for this writer
  uint16_t header_flags;

  uint32_t endian_tag;            // 0x01020304, validates byte order
  uint32_t record_format;         // 1 = 16/16 record header + extended length escape

  uint64_t file_size;             // total payload size, including this header
  uint64_t payload_hash;          // hash/checksum of record stream or full payload
  uint64_t interface_hash;        // semantic public interface hash

  uint32_t compiler_if_version;   // compiler interface compatibility version
  uint32_t header_record_offset;   // offset of HEADER_BEGIN from file start
  uint32_t first_body_record_offset; // offset after HEADER_END padding
  uint32_t reserved0;              // must be zero in v1
};
```

Header strings and configuration details are stored as records, not directly in the
fixed header.

The fixed header should stay small. New header metadata should normally be added as
`HEADER_DATA` records instead of expanding `TDqmIfHeader`.

---

## 5. Header Records

After the fixed header, the stream starts with a header record group:

```text
HEADER_BEGIN
  HEADER_DATA <key, value>
  HEADER_DATA <key, value>
  ...
HEADER_END
```

The header group is still encoded with the normal record header described below.
This keeps the file flexible without making the global header large.

Suggested required v1 header data keys:

```text
module_path
language_version
target_triple
object_format
abi_key
pointer_size
target_endianness
source_hash
options_hash
defines_hash
dependency_hash
compiler_name
compiler_version
```

Suggested `HEADER_DATA` payload:

```text
u16 key_id
u16 value_kind
u8  value[rec_len - 4]
```

For string values, `value` is the UTF-8 string bytes directly. For integer and hash
values, `value` is the fixed-width little-endian scalar. The record header gives the
total payload length, so string values do not carry an additional length field.

---

## 6. Record Header

Every record starts with a 32-bit header:

```text
bits  0..15   record_id
bits 16..31   payload_len
```

C++ extraction:

```cpp
uint32_t h = ReadU32LE();
uint16_t rec_id = h & 0xFFFFu;
uint32_t rec_len = h >> 16;
```

If `payload_len != 0xFFFF`, then `payload_len` is the payload length in bytes.

If `payload_len == 0xFFFF`, then an extended 32-bit length follows immediately after the record header:

```text
u32 record_header
u32 extended_payload_len
u8  payload[extended_payload_len]
zero padding to 4-byte boundary
```

So the parser does:

```cpp
uint32_t h = ReadU32LE();
uint16_t rec_id = h & 0xFFFFu;
uint32_t rec_len = h >> 16;

if (rec_len == 0xFFFFu)
{
  rec_len = ReadU32LE();
}
```

Record length excludes alignment padding.

---

## 7. Record Padding

After the payload, zero padding is added until the next record header is 4-byte aligned.

```text
record start: 4-byte aligned
record header: 4 bytes
optional extended length: 4 bytes
payload: rec_len bytes
padding: 0..3 zero bytes
next record start: 4-byte aligned
```

The writer must emit zero padding. The reader may validate padding bytes and reject the file if non-zero bytes are found.

---

## 8. Record Stream Model

The stream is token-like and stateful.

Records are not a table database. They are closer to binary parser tokens. Sub-parsers consume the records they understand in the current context.

Example parser structure:

```cpp
ParseDqmIf()
{
  ParseHeader();
  Expect(REC_HEADER_BEGIN);
  ParseHeaderRecords();
  Expect(REC_HEADER_END);
  Expect(REC_MODULE_START);
  ParseModuleBody();
  Expect(REC_MODULE_END);
  Expect(REC_END);
}

ParseObjectType()
{
  Expect(REC_OBJECT_START);

  while (!Peek(REC_OBJECT_END))
  {
    switch (PeekId())
    {
      case REC_NAME:       ParseName(); break;
      case REC_BASE_TYPE:  ParseBaseType(); break;
      case REC_SIZE_ALIGN: ParseSizeAlign(); break;
      case REC_FIELD_START: ParseField(); break;
      case REC_ATTR:       ParseAttribute(); break;
      default:             ErrorUnexpectedRecord();
    }
  }

  Expect(REC_OBJECT_END);
}
```

An unknown record ID or a valid record in an invalid parser state is a load failure.

---

## 9. Symbolic References

The v1 format uses symbolic string references for language-level identifiers.

Do not replace type names, function names, or module paths with serialized numeric IDs in the file.

Preferred:

```text
TYPE "str"
BASE_TYPE "dqgui/widgets/widget.TWidget"
IMPORT "dqgui/core"
RESULT_TYPE "TButton"
```

Avoid:

```text
type_id = 17
base_type_id = 9
import_id = 3
```

The loader reconstructs normal compiler objects and scopes from symbolic records.

During loading, the compiler may create internal temporary objects and native pointers, but those are not part of the serialized file format.

---

## 10. Canonical Names

Module paths and imported module references must be canonicalized before writing `.dqm_if`.

Source-level spellings such as these should not be serialized as semantic references:

```text
./button
../core
w/button
```

They should be written as canonical module paths:

```text
dqgui/widgets/button
dqgui/core
```

For type and symbol references, use the shortest representation that is unambiguous in the restored import scope, or use a canonical qualified symbolic name where ambiguity could occur.

Examples:

```text
TYPE "str"
TYPE "TButton"
BASE_TYPE "dqgui/widgets/widget.TWidget"
```

---

## 11. String Payloads

String records store UTF-8 bytes.

A string record payload is exactly:

```text
u8 bytes[rec_len]
```

The string byte length is taken from the record header. The string is not
zero-terminated and does not include a separate length field. The record-level
padding handles alignment and is not part of the string.

---

## 12. Binary Scalar Payloads

String identifiers stay symbolic. Stable scalar metadata should remain binary.

Examples:

```text
hashes
flags
sizes
alignments
field offsets
calling convention enum values
source line/column numbers
```

Example `SIZE_ALIGN` record payload:

```text
u32 size_bytes
u32 align_bytes
```

Example `OFFSET` record payload:

```text
u32 offset_bytes
```

Example `INTERFACE_HASH` record payload:

```text
u64 hash
```

A later version may use 128-bit hashes if desired.

---

## 13. Suggested Record ID Ranges

The 16-bit record ID space should be grouped by purpose.

```text
0x0000  INVALID
0x0001  END

0x0100..0x01FF  header/file/module/build records
0x0200..0x02FF  imports/dependencies/reexports
0x0300..0x03FF  type records
0x0400..0x04FF  object/member records
0x0500..0x05FF  function/method/parameter records
0x0600..0x06FF  constants
0x0700..0x07FF  symbols/namespaces
0x0800..0x08FF  attributes
0x0900..0x09FF  source/debug records
0x0A00..0xEFFF  reserved
0xF000..0xFFFF  experimental/internal
```

Suggested initial records:

```text
0x0001  END

0x0100  HEADER_BEGIN
0x0101  HEADER_DATA
0x0102  HEADER_END
0x0110  MODULE_START
0x0111  MODULE_END
0x0112  NAME
0x0113  TARGET
0x0114  ABI
0x0115  BUILD_OPTIONS_HASH
0x0116  SOURCE_HASH
0x0117  INTERFACE_HASH

0x0200  IMPORT
0x0201  DEPENDENCY
0x0202  REEXPORT

0x0300  TYPE_PRIMITIVE
0x0301  TYPE_ALIAS_START
0x0302  TYPE_ALIAS_END
0x0310  OBJECT_START
0x0311  OBJECT_END
0x0312  BASE_TYPE
0x0313  SIZE_ALIGN
0x0320  ENUM_START
0x0321  ENUM_VALUE
0x0322  ENUM_END
0x0330  POINTER_TYPE
0x0331  ARRAY_TYPE
0x0332  FUNCTION_TYPE_START
0x0333  FUNCTION_TYPE_END

0x0400  FIELD_START
0x0401  FIELD_END
0x0402  OFFSET

0x0500  FUNCTION_START
0x0501  FUNCTION_END
0x0502  METHOD_START
0x0503  METHOD_END
0x0504  PARAM_START
0x0505  PARAM_END
0x0506  RESULT_TYPE
0x0507  TYPE
0x0508  CALLCONV

0x0600  CONSTANT_START
0x0601  CONSTANT_END
0x0602  VALUE

0x0700  SYMBOL
0x0701  NAMESPACE

0x0800  ATTR
0x0801  ATTR_VALUE

0x0900  SOURCE_FILE
0x0901  SOURCE_LOC
```

The exact numeric values are provisional and should be centralized in one compiler header.

---

## 14. Module Records

A module stream starts with `MODULE_START` and ends with `MODULE_END`.

Example:

```text
MODULE_START
  NAME "dqgui/widgets/button"
  TARGET "x86_64-linux-gnu"
  ABI "sysv-amd64"
  SOURCE_HASH <u64>
  INTERFACE_HASH <u64>
  IMPORT "dqgui/core" <hash>
  IMPORT "dqgui/widgets/widget" <hash>
  ... declarations ...
MODULE_END
END
```

The module name must be the canonical module path.

---

## 15. Import and Dependency Records

`IMPORT` records describe imported public interfaces required to restore this module interface.

Suggested payload:

```text
u64 interface_hash
u32 flags
u8  module_path[rec_len - 12]
```

Flags may describe semantic import role:

```text
normal import
reexport import
interface dependency
implementation-only dependency, if ever stored
```

Dependency hash validation is strict. If an imported module's current interface hash does not match the stored hash, the `.dqm_if` is stale and must be regenerated.

---

## 16. Type Records

Types are represented as tokenized declaration streams.

Example object type:

```text
OBJECT_START
  NAME "TButton"
  BASE_TYPE "dqgui/widgets/widget.TWidget"
  SIZE_ALIGN 32, 8
  ATTR public

  FIELD_START
    NAME "text"
    TYPE "str"
    OFFSET 0
  FIELD_END

  FIELD_START
    NAME "visible"
    TYPE "bool"
    OFFSET 16
  FIELD_END
OBJECT_END
```

Example alias:

```text
TYPE_ALIAS_START
  NAME "THandle"
  TYPE "pointer<void>"
TYPE_ALIAS_END
```

Example enum:

```text
ENUM_START
  NAME "TButtonState"
  TYPE "int32"
  ENUM_VALUE "Normal", 0
  ENUM_VALUE "Pressed", 1
  ENUM_VALUE "Disabled", 2
ENUM_END
```

The precise representation of compound type expressions may evolve, but the v1 principle is symbolic and tokenized.

---

## 17. Function and Method Records

Functions and methods may be complex and are represented as nested token streams.

Example function:

```text
FUNCTION_START
  NAME "NewButton"
  PARAM_START
    NAME "text"
    TYPE "str"
  PARAM_END
  RESULT_TYPE "TButton"
  CALLCONV "dq-default"
  ATTR public
FUNCTION_END
```

Example external function:

```text
FUNCTION_START
  NAME "printf"
  PARAM_START
    NAME "fmt"
    TYPE "^cchar"
  PARAM_END
  ATTR external_name "printf"
  ATTR varargs
  CALLCONV "c"
FUNCTION_END
```

A method is similar but appears in an object context or carries an owner type record.

Example method inside object:

```text
OBJECT_START
  NAME "TButton"

  METHOD_START
    NAME "Paint"
    PARAM_START
      NAME "self"
      TYPE "^TButton"
    PARAM_END
    RESULT_TYPE "void"
  METHOD_END
OBJECT_END
```

Normal function bodies are not stored in `.dqm_if`. Bodies are stored only for constructs that require importer-side availability, such as future inline/comptime/generic features.

---

## 18. Attributes

Attributes are represented as token records so the format can grow without changing every parent record.

Example:

```text
ATTR public
ATTR external_name "printf"
ATTR callconv "c"
ATTR inline
ATTR comptime_required
```

Suggested v1 representation:

```text
ATTR "public"

ATTR "external_name"
  ATTR_VALUE "printf"

ATTR "varargs"
```

`ATTR` payload is the UTF-8 attribute name. `ATTR_VALUE` payload is either a string
value or a fixed-width scalar value interpreted according to the current attribute
parser state. For string attribute values, the record payload is the UTF-8 bytes
directly and the length is `rec_len`.

For common attributes, specialized records may be added later if needed.

---

## 19. Source Location Records

Source locations are optional but recommended for diagnostics.

Possible records:

```text
SOURCE_FILE "src/dqgui/widgets/button.dq"
SOURCE_LOC file_index, line, column
```

Source paths should be stored in a build-reproducible form where possible, such as path relative to source root.

---

## 20. Validation Rules

A reader must reject the file if any of these occur:

```text
bad magic
unsupported format_major
unsupported record_format
header_size smaller than required v1 header
non-zero reserved fields where v1 requires zero
file_size mismatch
payload hash mismatch
missing or malformed HEADER_BEGIN / HEADER_END group
unsupported compiler interface version
unsupported language version
target/config mismatch
ABI/layout mismatch
unknown record ID
record length exceeds file bounds
extended length malformed
record header not 4-byte aligned
non-zero padding, if strict padding validation is enabled
unexpected record in current parser state
missing required record
invalid nesting
missing END record
trailing data after END, unless explicitly allowed
invalid UTF-8 in string records
invalid dependency hash
```

Rejection means the artifact is unusable for the current compilation.

---

## 21. Regeneration Policy

When an interface is needed:

```text
1. Try to load embedded .dqm_if from complete .dqm.
2. If missing or invalid, try standalone .dqm_if.
3. If missing/stale/invalid/unsupported, run dq-comp --ifgen once.
4. Try loading the freshly generated .dqm_if.
5. If this fails, report a hard compiler error.
```

The compiler must avoid infinite regeneration loops.

Pseudo-flow:

```cpp
LoadInterface(module)
{
  auto r = TryLoadInterfaceArtifact(module);
  if (r == Ok)
    return interface;

  if (IsRecoverableLoadFailure(r))
  {
    RunIfGenOnce(module);

    r = TryLoadInterfaceArtifact(module);
    if (r == Ok)
      return interface;
  }

  ErrorCannotLoadInterface(module, r);
}
```

Recoverable failures include:

```text
missing artifact
stale artifact
unsupported format
corrupt artifact
target mismatch
ABI mismatch
dependency hash mismatch
```

Non-recoverable failures include:

```text
source module missing
interface cycle
permission denied
I/O error not caused by normal missing file
compiler error during regeneration
```

---

## 22. Compiler Commands

The DQ compiler is already prepared with these interface-related options:

```bash
dq-comp --ifgen <file.dq>
dq-comp --ifgen <file.dq> -o <file.dqm_if>
dq-comp --ifdump <file.dqm_if>
```

`--ifgen` writes a standalone `.dqm_if` file and stops before normal IR/object-code
generation. Without `-o`, the output filename is derived from the input by replacing
`.dq` with `.dqm_if`.

`--ifdump` reads one `.dqm_if` input and prints a diagnostic dump. It is mutually
exclusive with `--ifgen` and does not take an output filename.

---

## 23. Dump Tool

A dump tool should be implemented early.

Example commands:

```bash
dq-comp --ifdump button.dqm_if
```

Later, the same dump path may also accept `.dqm` files by extracting the embedded
`.dqm_if` section first.

Example output:

```text
module: dqgui/widgets/button
target: x86_64-linux-gnu
abi:    sysv-amd64
interface_hash: 0x...
source_hash:    0x...

imports:
  dqgui/core            hash 0x...
  dqgui/widgets/widget  hash 0x...

exports:
  object TButton : dqgui/widgets/widget.TWidget
    field text    : str   offset 0
    field visible : bool  offset 16

  function NewButton(text : str) -> TButton
```

The dump tool should parse the same record stream as the compiler loader, but may provide better diagnostics for malformed files.

---

## 24. Version 1 Summary

The recommended v1 `.dqm_if` format is:

```text
compact fixed header for fast rejection
HEADER_BEGIN / HEADER_DATA / HEADER_END records for flexible header metadata
little-endian canonical encoding
strict binary token record stream
16-bit record ID
16-bit normal payload length
0xFFFF length escape followed by u32 extended length
4-byte aligned record headers
zero padding between records
string length taken from record header; string payload is UTF-8 bytes only
symbolic string identifiers for module/type/function references
binary scalar records for hashes, flags, sizes, offsets, and source positions
strict stateful parser
unknown or unexpected record = load failure
failed load may trigger one regeneration attempt
```

This keeps the file easy to write, easy to parse, easy to dump, and close to the DQ interface grammar while avoiding premature numeric symbol/type ID systems.
