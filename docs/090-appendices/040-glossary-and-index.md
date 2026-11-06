# Glossary and Index

Use the site's search for individual declarations and methods. This index links
common terms to their detailed descriptions and also works in the PDF book.

| Term | Meaning and reference |
| --- | --- |
| Addressable value | Storage whose address can be taken with `%`; see [Pointers](../020-language/110-pointers.md). |
| `anyvalue` | A tagged value for supported scalars and text; see [`anyvalue`](../020-language/150-anyvalue.md). |
| Attribute | Declaration metadata written `[[...]]`; see [Attributes](../020-language/180-attributes.md). |
| `autofree` | An owner that releases its object or pointer; see [Memory Management and Ownership](../020-language/130-memory-management-and-ownership.md). |
| Borrowed view | A reference to storage whose lifetime is managed elsewhere; see [Arrays and Slices](../020-language/080-arrays-and-slices.md) and [Character and String Types](../020-language/070-strings-and-characters.md). |
| C ABI | Native calling and layout conventions used by external declarations; see [C Interoperability](../020-language/200-c-interoperability.md). |
| Canonical module identity | A module's identity within its module/package root; see [Modules and Packages](../020-language/170-modules-and-packages.md). |
| Copy-on-write | Shared string storage becomes independent when modified; see [Character and String Types](../020-language/070-strings-and-characters.md). |
| Directive | A compiler instruction such as `#if` or `#include`; see [Compiler Directives](../020-language/190-compiler-directives.md). |
| Dynamic array | Managed `[*]T` storage shared by assignments; see [Arrays and Slices](../020-language/080-arrays-and-slices.md). |
| Embedded object | An object stored directly in local storage, the global data segment, or a containing object, declared with `<-`; see [Objects and Properties](../020-language/120-objects-and-properties.md). |
| Embedded string | A bounded string descriptor or fixed `embstr(N)` storage; see [Character and String Types](../020-language/070-strings-and-characters.md). |
| Enumeration | A named set of ordinal values; see [Enumerations](../020-language/090-enumerations.md). |
| Exception | A failure propagated to a matching handler; see [Exceptions and Error Handling](../020-language/140-exceptions-and-error-handling.md). |
| Function reference | A callable value, optionally bound to an object; see [Functions](../020-language/060-functions.md). |
| Interface file | Compiler-owned `.dqm_if` public semantic data; see [Build Process and Module Artifacts](../030-compiler/060-build-process-and-module-artifacts.md). |
| LTO | Link-time optimization across compiled modules; see [Compiler Usage and Options](../030-compiler/040-compiler-usage-and-options.md). |
| Module | One `.dq` source file with public and implementation regions; see [Modules and Packages](../020-language/170-modules-and-packages.md). |
| Namespace | A qualified scope reached with `@name`; see [Modules and Packages](../020-language/170-modules-and-packages.md). |
| Ownership | Responsibility for releasing an allocation; see [Memory Management and Ownership](../020-language/130-memory-management-and-ownership.md). |
| Package | A source-module tree found through package roots; see [Library Overview](../040-packages/010-library-overview-and-sys.md). |
| Prelude | The implicit `rtl/sys` interface; see [`sys`](../040-packages/015-sys.md). |
| Project file | A `.dqproj` collection of compiler/build settings; see [Project Files](../030-compiler/050-project-files.md). |
| Property | Field-like syntax backed by a field or accessor; see [Objects and Properties](../020-language/120-objects-and-properties.md). |
| Reference parameter | An alias supplied with `ref`, `refin`, `refout`, or `refnil`; see [Functions](../020-language/060-functions.md). |
| Reexport | Imported declarations exposed by another module; see [Modules and Packages](../020-language/170-modules-and-packages.md). |
| Runtime | DQ support for startup, managed storage, and checks; see [Runtime and Program Execution](../030-compiler/015-runtime-and-program-execution.md). |
| Slice | A borrowed view of array or text storage; see [Arrays and Slices](../020-language/080-arrays-and-slices.md) and [Character and String Types](../020-language/070-strings-and-characters.md). |
| Structure / union | Direct aggregate storage; see [Structures and Unions](../020-language/100-structures-and-unions.md). |
| Sysroot | Target headers and native libraries for a cross build; see [Targets and Cross-Compilation](../030-compiler/070-targets-and-cross-compilation.md). |
| Target | Machine, ABI, runtime, and feature defaults; see [Targets and Cross-Compilation](../030-compiler/070-targets-and-cross-compilation.md). |
| Virtual dispatch | Method selection using an object's runtime type; see [Objects and Properties](../020-language/120-objects-and-properties.md). |
