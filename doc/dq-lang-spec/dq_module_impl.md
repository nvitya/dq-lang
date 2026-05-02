# DQ Module Implementation Notes

Status: draft implementation design  
Scope: compiler/build implementation strategy for DQ modules, compiled module artifacts, interface generation, dependency handling, object-file metadata, and linking.

This document is intentionally separate from `dq_module_spec.md`.

`dq_module_spec.md` describes module behavior from the user perspective. This file describes one practical compiler implementation strategy for that behavior.

---

## 1. Goals

The DQ module implementation should support:

- fast recompilation using compiled module interfaces
- low memory usage on small Linux systems
- single-module compiler internals in the current `dq-comp`
- recursive module dependency handling
- target/ABI-specific interface data
- normal native object-file linking
- one compiled artifact per module where possible
- later extension to archives/packages

The design should not require the compiler to keep a whole application in memory.

A build of a large GUI application should be possible on systems with limited RAM, for example a 512 MB Linux machine.

---

## 2. Main Artifact Names

Recommended naming:

```text
.dq       DQ source module
.dqi      DQ source include file
.dqm      compiled DQ module artifact
.dqm_if   DQ module interface payload, either as a standalone file or as an object-file section
```

The `.dqm` extension is used for complete compiled DQ modules.

A `.dqm` file is:

```text
complete
  produced by full compilation
  contains object code and the embedded module interface
  suitable for final linking
```

A `.dqm_if` file is:

```text
interface-only
  produced by --ifgen
  contains enough information for importing the module
  not a native object file
  not suitable for final linking
```

The `.dqm_if` payload bytes are the same whether stored as a standalone file or embedded as the `.dqm_if` custom section inside a `.dqm` object file.

---

## 3. Compiled Module Concept

A compiled DQ module is not only a public interface file and not only machine code.

Conceptually:

```text
module.dqm
  native object-file container
  DQ module-interface section: .dqm_if
  optional native code/data sections: .text, .data, .rodata, ...
```

For a full compile:

```text
button.dq -> button.dqm
```

where `button.dqm` is internally a native object file:

```text
ELF / COFF / Mach-O object file
  .text
  .data
  .rodata
  .dqm_if
```

For interface-only generation:

```text
button.dq -> button.dqm_if
```

where `button.dqm_if` is a standalone serialized module-interface payload, not an object file.

For a full compile, the same payload is embedded into the completed object file:

```text
button.dq -> button.dqm
```

After a successful full compile, the standalone `button.dqm_if` may be deleted because its contents have been merged into `button.dqm`.

---

## 4. Why `.dqm` Can Be an Object File

Most native linkers care about file content, not primarily the filename extension.

Therefore a file named:

```text
button.dqm
```

can still be linkable if its content is a valid target object file.

Examples:

```bash
gcc main.dqm button.dqm window.dqm -o app
```

or:

```bash
ld main.dqm button.dqm window.dqm -o app
```

This should work with usual GNU/LLVM toolchains, assuming the `.dqm` files are real object files for the target.

However, build systems may need explicit rules because many tools assume `.o` by convention.

---

## 5. Object Format Is Target-Specific

The `.dqm` file format follows the compilation target.

Typical examples:

```text
x86_64-linux-gnu       -> ELF relocatable object
arm-linux-gnueabihf    -> ELF relocatable object
riscv64-linux-gnu      -> ELF relocatable object
x86_64-w64-mingw32     -> COFF / PE-COFF relocatable object
x86_64-apple-darwin    -> Mach-O relocatable object
```

Using GNU tools on Windows does not imply Linux-style ELF objects. For example, MinGW-w64 normally produces COFF/PE-COFF `.o` files.

The target, not the host system, determines the object format.

---

## 6. The `.dqm_if` Payload

The DQ compiler interface data is serialized as a `.dqm_if` payload.

During interface generation, the payload is written as a standalone file:

```text
button.dqm_if
```

During full compilation, the same payload bytes are embedded into the compiled module object file using a custom section:

```text
.dqm_if
```

This name is short and works well for both ELF and COFF. It is also clear enough:

```text
.dqm_if = DQ module interface
```

The payload contains serialized DQ module-interface metadata.

The linker normally ignores unknown custom sections, while the DQ compiler can either read the standalone `.dqm_if` file or extract and parse the embedded section when it needs the module interface.

---

## 7. Suggested `.dqm_if` Payload Header

The `.dqm_if` payload should begin with a DQ-specific header.

Suggested fields:

```text
magic:                    "DQMIF\0"
format_version
compiler_interface_version
language_version
canonical_module_path
target_triple
object_format             elf | coff | macho | ...
abi_key
pointer_size
endianness
data_layout
calling_convention_defaults
preprocessor_define_hash
compiler_option_hash
source_hash
interface_hash
dependency_count
section_table_offset
```

The payload after the header should contain sectioned compiler metadata.

Suggested internal payload sections:

```text
STRINGS
MODULE_INFO
OPTIONS
IMPORTS
REEXPORTS
SYMBOLS
TYPES
FUNCTIONS
CONSTANTS
OBJECT_LAYOUTS
ATTRIBUTES
DEPENDENCY_HASHES
SOURCE_MAP
```

A section table is recommended so the format can evolve without rewriting the whole loader.

---

## 8. Interface Data Is Target/ABI-Specific

DQ compiled interface data is not target-independent.

Public interface data may depend on:

- pointer size
- integer aliases
- alignment rules
- object layout
- calling convention
- target CPU
- target OS
- endianness
- external ABI rules
- enabled feature flags
- preprocessor defines
- platform-specific imported modules

Therefore `.dqm_if` and `.dqm` files must be treated as target/configuration-specific compiler artifacts.

A `.dqm_if` or `.dqm` must not be loaded just because the module path matches. The compiler must validate that the stored configuration matches the current build.

Minimum validation:

```text
module path matches
compiler interface format matches
target triple matches
ABI/config key matches
relevant defines/options match
dependency interface hashes match
source/interface is not stale
```

---

## 9. Canonical Module Paths

The compiler should store canonical module paths in `.dqm_if`, not source-level spellings.

Examples:

```text
source spelling       canonical module path
-------------------------------------------
./button              dqgui/widgets/button
../core               dqgui/core
w/button              dqgui/widgets/button
dqgui/widgets/button  dqgui/widgets/button
```

`usepath` aliases and relative paths are source-level conveniences. They should be resolved before writing the compiled interface.

The `.dqm_if` payload should store:

```text
import dqgui/widgets/button
```

not:

```text
import ./button
import w/button
```

---

## 10. What the Compiled Interface Contains

The `.dqm_if` payload should contain the semantic public interface of one module.

It should include:

```text
canonical module path
public symbol table
public type table
public function signatures
public constants
public object layouts when public/ABI-relevant
public enum definitions
public aliases
public attributes
public overload sets
public external linkage metadata
interface imports
reexports
dependency hashes
source locations for diagnostics
```

It should not normally include:

```text
normal function bodies
local variables
private implementation declarations
statement ASTs
temporary expression trees
local namespace aliases
usepath aliases
formatting
comments
```

Exceptions exist for constructs whose body must be available to importers:

```text
inline functions
comptime functions required by importers
generic/template functions
public constants computed from compile-time code
future macro-like constructs
```

---

## 11. Public Functions With Bodies in the Interface Section

DQ allows implementations in the interface section.

Example:

```dq
function Add(a : int, b : int) -> int
  return a + b;
endfunc
```

For module-interface purposes:

```text
function signature -> public interface
function body      -> implementation
```

The `.dqm_if` payload normally stores only:

```text
function Add(a : int, b : int) -> int
```

The body belongs to the implementation and should not affect the interface hash unless the function is explicitly inline/comptime/generic or otherwise requires body availability to importers.

Changing a normal function body should require recompiling that module, but should not require recompiling users of that module.

---

## 12. Interface Hash

Each module interface should have a stable semantic hash.

The interface hash should include ABI-relevant public information:

```text
public names
public types
public function signatures
calling conventions
public constants
public object layouts
public attributes
reexported symbols
external linkage declarations
relevant target/config settings
```

The interface hash should not include normal private implementation details:

```text
normal function bodies
local variables
private helper functions
private statement code
non-public implementation imports
```

If the interface hash is unchanged, dependent modules do not need recompilation.

If the interface hash changes, dependent modules may need recompilation.

---

## 13. Compiler Modes

The current `dq-comp` is designed around one compiler state and one module at a time.

Recommended modes:

```bash
dq-comp --ifgen <module>
dq-comp --compile <module>
dq-comp --dump-module <module.dqm_if>
dq-comp --dump-module <module.dqm>
```

### `--ifgen`

Generates a standalone `.dqm_if`.

Behavior:

```text
read source module
scan interface
resolve interface imports
skip/discard normal function bodies
emit .dqm_if
```

### `--compile`

Generates a complete `.dqm`.

Behavior:

```text
read source module
load needed .dqm_if data from imported modules
parse/check implementation
emit native object code
embed .dqm_if
atomically publish .dqm
delete standalone .dqm_if after successful publish if it is now redundant
```

### `--dump-module`

Debugging tool similar in spirit to FreePascal `ppudump`.

Behavior:

```text
open .dqm_if or .dqm
read standalone .dqm_if or extract embedded .dqm_if
print module path, imports, exports, hashes, public symbols, type layouts
```

---

## 14. Interface Scanning Pass

DQ should have a real interface scanning pass.

This is important for low memory use.

The interface scanner should:

```text
parse declarations needed for the public interface
handle use/usepath/reexport/only/nomerge semantics
resolve imported interfaces on demand
skip normal function bodies
stop or skip after implementation depending on mode
write .dqm_if metadata
```

The scanner should not keep full implementation ASTs in memory.

For a normal public function body in the interface section:

```dq
function Paint(w : TWindow)
  if w.visible:
    DrawFrame(w);
  endif
endfunc
```

The interface scanner should keep:

```text
function Paint(w : TWindow)
```

and structurally skip until the matching:

```dq
endfunc
```

Because DQ uses explicit closers such as `endif`, `endwhile`, and `endfunc`, structural skipping should be practical.

---

## 15. `use` as an Interface Barrier

In DQ, a `use` declaration can affect lookup for subsequent declarations.

Example:

```dq
use ./core;

type TWindow = object(TGuiObject)
  ...
endobj
```

The compiler cannot resolve `TGuiObject` unless the public interface of `dqgui/core` is already available.

Therefore, during interface scanning, `use` is a semantic barrier:

```text
ParseUse()
  parse the use syntax
  canonicalize the module path
  ensure the imported module interface exists
  load its .dqm_if
  create local namespace
  merge public symbols unless nomerge
  continue scanning
```

---

## 16. Subprocess-Based Interface Generation

Because the current `dq-comp` can handle only one compiler state, it should not try to recursively parse another source file inside the same process.

Instead, when a module needs an imported interface and no valid embedded or standalone `.dqm_if` exists, it can run another `dq-comp --ifgen` subprocess.

Conceptual function:

```text
LoadInterface(module):
  if valid complete .dqm exists:
    extract .dqm_if
    return interface

  if valid standalone .dqm_if exists:
    read .dqm_if
    return interface

  run dq-comp --ifgen module

  if valid standalone .dqm_if now exists:
    read .dqm_if
    return interface

  report error
```

Example first build:

```text
dq-comp --compile dqgui_app
  sees use dqgui/window
    runs dq-comp --ifgen dqgui/window
      sees use ./core
        runs dq-comp --ifgen dqgui/core
        loads dqgui/core.dqm_if
      writes dqgui/window.dqm_if
    loads dqgui/window.dqm_if

  sees use dqgui/button
    runs dq-comp --ifgen dqgui/button
      sees use ./core
        loads existing dqgui/core.dqm_if
      sees use ./widget
        runs dq-comp --ifgen dqgui/widget
      writes dqgui/button.dqm_if
    loads dqgui/button.dqm_if
```

This is a process-based module manager.

Later, a dedicated `dq-build` driver may replace this with an internal dependency scheduler, but subprocess recursion is a practical bootstrap design.

---

## 17. Avoid Repeated Blind Retries

The compiler should not repeatedly try the same file and abort with messages like:

```text
interface unknown for dqgui/core
interface unknown for dqgui/widgets
```

Instead it should explicitly satisfy the dependency:

```text
need dqgui/core interface
  check cache
  generate with --ifgen if missing/stale
  load .dqm_if
  continue
```

This keeps the flow deterministic.

---

## 18. File Locking and Atomic Writes

The `.dqm_if` and `.dqm` generation protocols must be safe for recursive subprocesses and later parallel builds.

Do not let one process read a partially written `.dqm_if` or `.dqm`.

Recommended protocol:

```text
1. Check whether a valid artifact exists.
2. If not, acquire a lock for that module/configuration.
3. Write output to a temporary file.
4. Validate/finish the temporary file.
5. Atomically rename temporary file to final artifact.
6. Release the lock.
```

Example files for interface generation:

```text
button.dqm_if.lock/
button.dqm_if.tmp.<pid>
button.dqm_if
```

Example files for full compilation:

```text
button.dqm.lock/
button.dqm.tmp.<pid>
button.dqm
```

Use atomic directory creation, for example `mkdir`, for the lock.

Use atomic `rename()` to publish the finished `.dqm_if` or `.dqm`.

The reader should only accept final artifacts, never temporary files.

After a full compile successfully publishes `button.dqm`, it may remove `button.dqm_if` because the same payload is now embedded in the complete module. The removal must happen after publishing the complete `.dqm`; a failed full compile should not leave the build without any usable interface artifact.

---

## 19. Interface Cycle Detection

Subprocess recursion needs explicit cycle detection.

Example cycle:

```text
A --ifgen
  needs B
    B --ifgen
      needs A
```

If this is handled only with locks, the system may deadlock.

Each `--ifgen` subprocess should receive the current interface stack:

```bash
dq-comp --ifgen dqgui/window --ifstack dqgui_app,dqgui/window
```

When spawning another interface generation:

```bash
dq-comp --ifgen dqgui/core --ifstack dqgui_app,dqgui/window,dqgui/core
```

If a module is already present in the stack, report an interface cycle:

```text
ERROR(ModuleInterfaceCycle):
  dqgui_app -> dqgui/window -> dqgui/core -> dqgui/window
```

Initial implementation recommendation:

```text
interface-to-interface cycles are errors
implementation-to-interface cycles may be allowed later
```

---

## 20. Passing Build Context to Subprocesses

Every `--ifgen` subprocess must receive the same build context as the parent.

Required context includes:

```text
source roots
output/cache root
target triple
object format
ABI key
compiler options
language version
preprocessor defines
import search paths
feature flags
build mode
interface stack
```

Otherwise child processes may resolve modules differently from the parent.

Example:

```bash
dq-comp \
  --ifgen dqgui/window \
  --srcroot src \
  --outroot build \
  --target x86_64-linux-gnu \
  --define DEBUG=1 \
  --ifstack dqgui_app,dqgui/window
```

---

## 21. Build Flow for the `dqgui` Example

Given:

```text
dqgui_app uses:
  dqgui/window
  dqgui/button

dqgui/window uses:
  dqgui/core

dqgui/button uses:
  dqgui/core
  dqgui/widget
```

First compilation, no artifacts exist.

Interface generation order may become:

```text
dqgui/core       -> dqgui/core.dqm_if
dqgui/window     -> dqgui/window.dqm_if
dqgui/widget     -> dqgui/widget.dqm_if
dqgui/button     -> dqgui/button.dqm_if
dqgui_app        -> dqgui_app.dqm_if
```

Then implementation compilation:

```text
dqgui/core.dq    -> dqgui/core.dqm
dqgui/widget.dq  -> dqgui/widget.dqm
dqgui/window.dq  -> dqgui/window.dqm
dqgui/button.dq  -> dqgui/button.dqm
dqgui_app.dq     -> dqgui_app.dqm
```

Each complete `.dqm` embeds the same `.dqm_if` payload that was generated or recomputed for that module. After successful publication, the standalone `.dqm_if` can be removed if no longer needed as a separate cache entry.

Then final link:

```text
dqgui/core.dqm
dqgui/widget.dqm
dqgui/window.dqm
dqgui/button.dqm
dqgui_app.dqm
  -> executable
```

The exact object generation order may be less strict after all interfaces are known, but dependency order is useful for diagnostics and predictable builds.

---

## 22. Reading `.dqm_if`

The main compiler operation on imported modules is reading the `.dqm_if` payload.

For a standalone `.dqm_if` file, the compiler can read the file directly and parse the payload.

For a complete `.dqm`, the compiler extracts the `.dqm_if` section from the object file.

Recommended implementation:

```text
Use LLVM object-file library for complete .dqm files.
Do not manually parse ELF/COFF at first.
```

LLVM can read common object formats:

```text
ELF
COFF / PE-COFF
Mach-O
```

Conceptual algorithm:

```text
open object file
detect format
iterate sections
find section named .dqm_if
read raw section bytes
parse DQM interface payload
```

Conceptual C++ shape:

```cpp
auto buffer = MemoryBuffer::getFile(filename);
auto objOrErr = llvm::object::ObjectFile::createObjectFile(buffer);

for (const auto &sec : obj->sections())
{
  auto name = sec.getName();
  if (name && *name == ".dqm_if")
  {
    auto contents = sec.getContents();
    // parse contents as DQ module interface payload
  }
}
```

Manual parsing is possible but less attractive:

```text
ELF reader:
  parse ELF header
  parse section headers
  parse section string table
  find .dqm_if

COFF reader:
  parse COFF header
  parse section table
  handle section names
  find .dqm_if
```

Since DQ already uses LLVM, LLVM's object-file reader is the preferred path.

---

## 23. Emitting `.dqm_if` With LLVM

If DQ uses LLVM IR generation, the compiler can emit the DQ interface payload as a global byte array placed into a named section.

Conceptual LLVM IR:

```llvm
@__dq_dqm_if = private constant [N x i8] c"...", section ".dqm_if"
```

The global should be marked as used so LLVM does not remove it:

```llvm
@llvm.compiler.used = appending global [1 x ptr] [ptr @__dq_dqm_if], section "llvm.metadata"
```

In the LLVM C++ API:

```text
create a ConstantDataArray containing the payload
create a GlobalVariable for it
set section to .dqm_if
set linkage/visibility appropriately
append it to llvm.compiler.used or llvm.used
```

---

## 24. Final Executables Are Not the Primary Source of Interfaces

The compiler should read `.dqm_if` from standalone `.dqm_if` files or from compiled module `.dqm` files, not from the final executable.

Reason: linkers may:

```text
discard unused custom sections
merge sections
rename sections
strip metadata
change section layout
```

The standalone interface file and the compiled module file are the reliable units of compiler metadata.

The final executable does not need to preserve `.dqm_if` unless a special debugging/reflection mode wants it.

---

## 25. Complete vs Standalone Interface Artifacts

A standalone `.dqm_if` is an interface artifact:

```text
OK for importing symbols
not sufficient for final link
not a native object file
```

A `.dqm` is a complete compiled module artifact:

```text
OK for importing symbols
OK for final link
native object file with embedded .dqm_if
```

Before final link, the build tool must ensure that all required modules have complete `.dqm` files.

If only a standalone `.dqm_if` exists during final link preparation, run full compilation for that module.

Because artifact type now distinguishes the states, the `.dqm_if` header does not need a `module_state` field. The same payload can be stored in both places without changing its contents.

---

## 26. Interface-Only Source Modules

Some modules may naturally have no implementation code.

Examples:

```text
external C library declarations
platform binding declarations
SDK/header-like modules
pure type declaration modules
```

For those modules, full compilation may still produce a complete `.dqm` object file that contains `.dqm_if` and no `.text` section.

The `.dqm_if` header should distinguish whether object code is needed:

```text
needs_object_code = false
```

Alternatively, this can be inferred from the absence of implementation code and represented by a complete object file that contains no `.text`.

---

## 27. Reexports

A facade module that reexports child modules should store reexported symbols as public symbols of the facade module.

Example source:

```dq
use ./button reexport;
use ./label reexport;
```

The `.dqm_if` for the facade should contain entries like:

```text
TButton -> reexport dqgui/widgets/button.TButton
TLabel  -> reexport dqgui/widgets/label.TLabel
```

Local child namespaces used inside the facade module should not be exported as namespaces of the facade.

---

## 28. Namespace Aliases and `usepath`

Local namespace aliases from `use ... as ...` are local to a source file.

`usepath` aliases are also local source-level shortcuts.

Therefore `.dqm_if` should generally not serialize them as semantic data.

It may optionally store them only for diagnostics/debug information.

The semantic data should use canonical module paths and canonical symbol identities.

---

## 29. Stable Symbol Identity

Inside `.dqm_if`, public symbols should have stable identities independent of local aliases.

Recommended conceptual identity:

```text
canonical_module_path + symbol_name + overload_signature
```

Examples:

```text
dqgui/widgets/button:TButton
dqgui/widgets/button:NewButton(str)->TButton
```

For overloaded functions:

```text
print(str)
print(int)
print(float64)
```

Internal numeric IDs may be used inside the file, but they should be local to the `.dqm_if` payload.

---

## 30. Object Layouts

Public object layout should be stored when it is ABI-relevant.

This includes cases where public objects can be:

```text
passed by value
embedded in other public objects
allocated statically
used across module boundaries
used by external ABI calls
```

A public object type should record:

```text
base type
fields
field types
field offsets
size
alignment
layout flags
```

If private implementation details affect layout, they are not actually private from the ABI perspective and must be represented in the public interface somehow.

---

## 31. Dependency Records

Each `.dqm_if` should record the interfaces it depends on.

Example:

```text
module dqgui/widgets/button
depends interface:
  dqgui/core           interface_hash = abc123
  dqgui/widgets/widget interface_hash = def456
```

This supports stale-cache detection and incremental recompilation.

If an imported module's interface hash changes, dependent modules may need regeneration.

---

## 32. Incremental Rebuild Policy

Recommended rebuild rules:

```text
If source file changed but interface hash unchanged:
  regenerate complete .dqm for this module if needed
  do not rebuild dependent modules

If interface hash changed:
  rebuild dependent module interfaces/implementations as needed

If target/config changed:
  reject old .dqm_if/.dqm
  regenerate for new target/config

If dependency interface hash mismatch:
  reject old .dqm_if/.dqm
  regenerate
```

This is the main compile-time benefit of compiled module interfaces.

---

## 33. Cache Layout

Because `.dqm_if` and `.dqm` files are target/config specific, the build output should include a configuration key.

Example:

```text
build/dqcache/x86_64-linux-gnu-debug/
  dqgui/core.dqm_if
  dqgui/core.dqm
  dqgui/widget.dqm_if
  dqgui/widget.dqm
  dqgui/button.dqm_if
  dqgui/button.dqm
  dqgui/window.dqm_if
  dqgui/window.dqm
  dqgui_app.dqm_if
  dqgui_app.dqm
```

Or with a more explicit structure:

```text
build/dqcache/<config-key>/modules/
  dqgui/core.dqm_if
  dqgui/core.dqm
  dqgui/widget.dqm_if
  dqgui/widget.dqm
  dqgui/button.dqm_if
  dqgui/button.dqm
```

Standalone `.dqm_if` files may be absent for modules that already have valid complete `.dqm` files. They are intermediate/cache artifacts, while `.dqm` files are the required artifacts for final linking.

The `<config-key>` should include or derive from:

```text
target triple
object format
ABI key
build mode
relevant compiler flags
relevant defines
language version
```

---

## 34. Future Build Driver

The subprocess model can bootstrap module compilation, but a dedicated build driver may be added later.

Possible commands:

```bash
dq build dqgui_app
```

or:

```bash
dq-comp --build dqgui_app
```

A build driver can:

```text
resolve the module graph
schedule --ifgen jobs
schedule full compilation jobs
avoid redundant subprocesses
parallelize independent modules
track locks and dependencies centrally
link final executable
```

The compiler backend may still remain single-module internally.

---

## 35. Comparison With FreePascal

FreePascal uses a similar conceptual split:

```text
.pas/.pp source unit
.ppu     compiled unit interface/metadata
.o       native object code
```

DQ's proposed model is similar in spirit, but uses `.dqm_if` for standalone interface metadata during interface generation and embeds the same payload into the complete `.dqm` object artifact:

```text
DQ source:           module.dq
DQ compiled module:  module.dqm
DQ interface data:   module.dqm_if or .dqm_if section inside module.dqm
```

Unlike FreePascal's internal unit manager, the initial DQ implementation may use subprocesses because `dq-comp` currently handles only one compiler state.

---

## 36. Recommended Initial Implementation Steps

Recommended order:

```text
1. Define .dqm_if binary payload format.
2. Implement interface scanner that skips normal bodies.
3. Emit standalone .dqm_if files from --ifgen.
4. Implement .dqm_if reading from standalone files.
5. Implement .dqm_if extraction from .dqm files using LLVM ObjectFile.
6. Implement LoadInterface(module): load existing .dqm, load existing .dqm_if, or spawn --ifgen.
7. Add atomic .dqm_if/.dqm writing and module locks.
8. Add interface stack and cycle detection.
9. Implement full .dqm generation with object code + embedded .dqm_if.
10. Delete redundant standalone .dqm_if after successful full .dqm publication.
11. Ensure final link uses only complete .dqm files.
12. Add dq module dump tool.
13. Add smarter build driver later.
```

---

## 37. Summary

The preferred implementation model is:

```text
.dq       source module
.dqi      source include file
.dqm      compiled DQ module artifact
.dqm_if   compiled interface payload, stored as a standalone file or embedded object section
```

A `.dqm` is a native object file that the linker can consume. It also contains the DQ module interface in the `.dqm_if` section.

The interface-only pass creates a standalone `.dqm_if` that is sufficient for imports. The full compile later creates a complete `.dqm` containing object code and the same `.dqm_if` payload embedded as a section. After successful publication of the complete `.dqm`, the standalone `.dqm_if` may be deleted.

Since the current compiler supports only one compiler state, recursive interface generation can be implemented by spawning `dq-comp --ifgen` subprocesses. This must be protected with locks, atomic writes, shared build context, and explicit interface-cycle detection.

This design keeps module behavior clean for users while allowing a practical, low-memory compiler implementation.
