# Documentation Coverage

The maintained book lives in `docs/`, organized into Project, Language, Compiler
and Tools, Standard Packages, and Appendices. The previous site's sources remain
under `doc/docs-v1/` as a migration snapshot. Design history remains under
`doc/archive/`; neither location overrides the current book.

## Previous Site to Current Book

Guide and reference material for the same feature is combined into one language
chapter. The later language chapters retain the material migrated before this
completion pass. Exceptions have a dedicated chapter, while attributes,
directives, and C interoperability are separate topics.

| Previous page under `doc/docs-v1/docs/` | Current destination |
| --- | --- |
| `index.md` | [DQ-lang Project Introduction](../010-project/010-introduction.md) |
| `motivation.md` | [Features and Design Goals](../010-project/020-features-and-goals.md) |
| `getting-started.md` | [Getting Started](../010-project/030-getting-started.md) |
| `reference/index.md` | [DQ Programming Language](../020-language/index.md) |
| `language/basics.md` | [Basic Syntax](../020-language/010-basic-syntax.md) |
| `reference/lexical-structure.md` | [Basic Syntax](../020-language/010-basic-syntax.md) |
| `language/types.md` | [Basic Types and Conversions](../020-language/020-basic-types-and-conversions.md) |
| `reference/types-and-conversions.md` | [Basic Types and Conversions](../020-language/020-basic-types-and-conversions.md) |
| `reference/declarations-and-scope.md` | [Variables, Constants, References, and Scope](../020-language/030-variables-constants-references-and-scope.md) |
| `language/expressions.md` | [Expressions and Operators](../020-language/040-expressions-and-operators.md) |
| `reference/expressions-and-operators.md` | [Expressions and Operators](../020-language/040-expressions-and-operators.md) |
| `language/statements.md` | [Statements and Control Flow](../020-language/050-statements-and-control-flow.md) |
| `reference/statements-and-control-flow.md` | [Statements and Control Flow](../020-language/050-statements-and-control-flow.md) |
| `language/functions.md` | [Functions](../020-language/060-functions.md) |
| `reference/functions.md` | [Functions](../020-language/060-functions.md) |
| `reference/strings-and-characters.md` | [Character and String Types](../020-language/070-strings-and-characters.md) |
| `rtl/strings.md` | [String Functions](../020-language/075-string-functions.md) |
| `rtl/textfmt.md` | [Text Formatting](../020-language/077-text-formatting.md) |
| `reference/arrays-and-slices.md` | [Arrays and Slices](../020-language/080-arrays-and-slices.md) |
| `reference/enums.md` | [Enumerations](../020-language/090-enumerations.md) |
| `reference/structures-pointers-and-function-references.md` | [Structures and Unions](../020-language/100-structures-and-unions.md) |
| `language/memory-and-pointers.md` | [Pointers](../020-language/110-pointers.md) |
| `language/objects.md` | [Objects and Properties](../020-language/120-objects-and-properties.md) |
| `reference/objects-and-properties.md` | [Objects and Properties](../020-language/120-objects-and-properties.md) |
| `reference/autofree.md` | [Memory Management and Autofree](../020-language/130-memory-management-and-ownership.md) |
| `reference/anyvalue.md` | [anyvalue](../020-language/150-anyvalue.md) |
| `rtl/anyvalue.md` | [anyvalue](../020-language/150-anyvalue.md) |
| `language/modules.md` | [Modules and Packages](../020-language/170-modules-and-packages.md) |
| `reference/modules-and-packages.md` | [Modules and Packages](../020-language/170-modules-and-packages.md) |
| `language/attributes-and-directives.md` | [Attributes](../020-language/180-attributes.md) |
| `reference/attributes-directives-and-interop.md` | [Attributes](../020-language/180-attributes.md) |
| `language/interop.md` | [C Interoperability](../020-language/200-c-interoperability.md) |
| `language/asm-functions.md` | [Assembly Functions](../020-language/210-assembly-functions.md) |
| `reference/assembly-functions.md` | [Assembly Functions](../020-language/210-assembly-functions.md) |
| `compiler/using-dq-comp.md` | [Compiler Usage and Options](../030-compiler/040-compiler-usage-and-options.md) |
| `compiler/project-file.md` | [Project Files (`.dqproj`)](../030-compiler/050-project-files.md) |
| `compiler/dqm-format.md` | [Build Process and Module Artifacts](../030-compiler/060-build-process-and-module-artifacts.md) |
| `compiler/linux-devenv.md` | [Linux Development Environment](../030-compiler/020-linux-development-environment.md) |
| `compiler/windows-devenv.md` | [Windows Development Environment](../030-compiler/030-windows-development-environment.md) |
| `compiler/vscode-setup.md` | [VSCode Setup and Debugging](../030-compiler/080-vscode-setup-and-debugging.md) |
| `compiler/windows-debugging.md` | [VSCode Setup and Debugging](../030-compiler/080-vscode-setup-and-debugging.md) |
| `rtl/overview.md` | [Library Overview](../040-packages/010-library-overview-and-sys.md) |
| `examples/index.md` | [Example Programs](../090-appendices/010-example-programs.md) |
| `compiler/getting-dq-comp.md` | [Building the Compiler from Source](../090-appendices/020-building-the-compiler-from-source.md) |
| `contributing/index.md` | [Contributor and Implementation Notes](../090-appendices/030-contributor-and-implementation-notes.md) |
| `contributing/documentation-policy.md` | [Contributor and Implementation Notes](../090-appendices/030-contributor-and-implementation-notes.md) |
| `contributing/writing-current-dq.md` | [Contributor and Implementation Notes](../090-appendices/030-contributor-and-implementation-notes.md) |
| `contributing/module-and-interface-internals.md` | [Module and Interface Internals](../090-appendices/031-module-and-interface-internals.md) |
| `contributing/exceptions-and-backtraces.md` | [Exceptions and Backtraces](../090-appendices/032-exceptions-and-backtraces.md) |
| `compiler/cross-compiling.md` | [Release and Cross-Build Notes](../090-appendices/033-release-and-cross-builds.md) |
| `contributing/release-and-cross-builds.md` | [Release and Cross-Build Notes](../090-appendices/033-release-and-cross-builds.md) |
| `contributing/coverage-inventory.md` | [Documentation Coverage](../090-appendices/050-documentation-coverage.md) |

The former combined structure/pointer/function-reference page is split among
[Structures and Unions](../020-language/100-structures-and-unions.md),
[Pointers](../020-language/110-pointers.md), and
[Functions](../020-language/060-functions.md). The old statement pages' exception
rules are in [Exceptions and Error Handling](../020-language/140-exceptions-and-error-handling.md).
The combined attributes page is split among
[Attributes](../020-language/180-attributes.md),
[Compiler Directives](../020-language/190-compiler-directives.md), and
[C Interoperability](../020-language/200-c-interoperability.md).

The old compiler usage page is split among the tool overview, option reference,
artifact chapter, and [application targets](../030-compiler/070-targets-and-cross-compilation.md).
Compiler cross-build procedures belong to the maintainer appendix. Release
installation is covered by the Linux and Windows environment chapters; source
builds have their own appendix.

The old runtime overview is expanded into
[Runtime and Program Execution](../030-compiler/015-runtime-and-program-execution.md),
[Library Overview](../040-packages/010-library-overview-and-sys.md), and
[`sys`](../040-packages/015-sys.md). There is also a dedicated
[`print`](../040-packages/020-print.md) chapter.

## Package Manuals

Package-local documentation remains the maintained API source. The book includes
these files directly, so a correction is published in both forms.

| Source | Book chapter |
| --- | --- |
| `stdpkg/file/docs/index.md` | [`file`](../040-packages/040-file.md) |
| `stdpkg/json/docs/index.md` | [`json`](../040-packages/050-json.md) |
| `stdpkg/threads/docs/index.md` | [`threads`](../040-packages/060-threads.md) |
| `stdpkg/time/docs/index.md` | [`time`](../040-packages/065-time.md) |
| `stdpkg/nanonet/docs/index.md` | [`nanonet`](../040-packages/070-nanonet.md) |
| `stdpkg/shell/docs/index.md` | [`shell`](../040-packages/075-shell.md) |

## Archived Design Coverage

This inventory records the disposition of the former `doc/` specifications.
The archived copies are historical and do not override the listed destinations.

| Archived source | Disposition and canonical current documentation |
| --- | --- |
| Original language specification and changelog | Historical baseline. Replaced by the complete [Language Reference](../020-language/index.md). |
| Character/integer conversion specification | Migrated to [Types and Conversions](../020-language/020-basic-types-and-conversions.md) and [Strings and Characters](../020-language/070-strings-and-characters.md). |
| Enum specification | Migrated to [Enumerations](../020-language/090-enumerations.md). |
| Array specification | Migrated to [Arrays and Slices](../020-language/080-arrays-and-slices.md), including the array methods. |
| String specification and String Handling v2 | Reconciled in [Strings and Characters](../020-language/070-strings-and-characters.md) and [String Methods](../020-language/075-string-functions.md). Conflicting earlier character models are historical. |
| `anyvalue` specification | Public behavior and runtime methods moved to [`anyvalue`](../020-language/150-anyvalue.md). Private layout remains compiler/runtime-owned. |
| Object allocation/lifetime specification | Migrated to [Objects and Properties](../020-language/120-objects-and-properties.md). |
| Property specification | Migrated to [Objects and Properties](../020-language/120-objects-and-properties.md). |
| Object type-information notes | User-visible tests/casts moved to [Objects and Properties](../020-language/120-objects-and-properties.md); implementation detail belongs to compiler code. |
| Exception/backtrace specification | Semantics moved to [Exceptions and Error Handling](../020-language/140-exceptions-and-error-handling.md); implementation summarized in [Exceptions and Backtraces](../090-appendices/032-exceptions-and-backtraces.md). |
| Module system specification | Source behavior migrated to [Modules and Packages](../020-language/170-modules-and-packages.md). |
| Package specification | Search and identity behavior migrated to [Modules and Packages](../020-language/170-modules-and-packages.md) and [Using the Compiler](../030-compiler/040-compiler-usage-and-options.md). |
| Module implementation notes | Current design summarized in [Module and Interface Internals](../090-appendices/031-module-and-interface-internals.md); obsolete embedded-interface designs remain historical. |
| DQM interface-format specification | Current user contract is [DQ Module Artifacts](../030-compiler/060-build-process-and-module-artifacts.md); implementation is summarized in [Module and Interface Internals](../090-appendices/031-module-and-interface-internals.md). |
| Build specification | Current user behavior is [Using the Compiler](../030-compiler/040-compiler-usage-and-options.md); superseded `.dqm` layouts are historical. |
| Cross-compiling notes | Maintained user workflow is [Cross-Compiling and Packaging](../090-appendices/033-release-and-cross-builds.md); maintainer notes are [Release and Cross-Build Notes](../090-appendices/033-release-and-cross-builds.md). |
| Method-local `use` notes | Migrated to [Declarations and Scope](../020-language/030-variables-constants-references-and-scope.md) and [Modules and Packages](../020-language/170-modules-and-packages.md). |
| DQ writing hints for AI agents | Replaced by [Writing Current DQ](../090-appendices/030-contributor-and-implementation-notes.md), which links to normative pages rather than duplicating specifications. |

## Original Specification Sections

| Original area | Canonical destination |
| --- | --- |
| Overview and design principles | [Home](../010-project/010-introduction.md), [Motivation](../010-project/020-features-and-goals.md) |
| Lexical structure and literals | [Lexical Structure](../020-language/010-basic-syntax.md) |
| Types, variables, and constants | [Types and Conversions](../020-language/020-basic-types-and-conversions.md), [Declarations and Scope](../020-language/030-variables-constants-references-and-scope.md) |
| Expressions and operators | [Expressions and Operators](../020-language/040-expressions-and-operators.md) |
| Statements and control flow | [Statements and Control Flow](../020-language/050-statements-and-control-flow.md) |
| Functions | [Functions](../020-language/060-functions.md) |
| Objects and properties | [Objects and Properties](../020-language/120-objects-and-properties.md) |
| Pointers and memory | [Structures, Pointers, and Function References](../020-language/100-structures-and-unions.md), [Objects and Properties](../020-language/120-objects-and-properties.md) |
| Modules and namespaces | [Modules and Packages](../020-language/170-modules-and-packages.md) |
| Compiler directives and C access | [Attributes, Directives, and Interoperability](../020-language/180-attributes.md) |
| Standard library | Runtime Library and Standard Packages sections of the site |
| Rejected features and open questions | Historical only unless reintroduced as a separate design proposal |
