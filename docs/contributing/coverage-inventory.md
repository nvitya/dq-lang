# Documentation Coverage Inventory

This inventory records the disposition of the former `doc/` specifications.
The archived copies are historical and do not override the listed destinations.

| Archived source | Disposition and canonical current documentation |
| --- | --- |
| Original language specification and changelog | Historical baseline. Replaced by the complete [Language Reference](../reference/index.md). |
| Character/integer conversion specification | Migrated to [Types and Conversions](../reference/types-and-conversions.md) and [Strings and Characters](../reference/strings-and-characters.md). |
| Enum specification | Migrated to [Enumerations](../reference/enums.md). |
| Array specification | Migrated to [Arrays and Slices](../reference/arrays-and-slices.md); callable methods remain in runtime guides. |
| String specification and String Handling v2 | Reconciled in [Strings and Characters](../reference/strings-and-characters.md) and [String Methods](../rtl/strings.md). Conflicting earlier character models are historical. |
| `anyvalue` specification | Public behavior moved to [`anyvalue`](../reference/anyvalue.md) and the [`anyvalue` runtime guide](../rtl/anyvalue.md). Private layout remains compiler/runtime-owned. |
| Object allocation/lifetime specification | Migrated to [Objects and Properties](../reference/objects-and-properties.md). |
| Property specification | Migrated to [Objects and Properties](../reference/objects-and-properties.md). |
| Object type-information notes | User-visible tests/casts moved to [Objects and Properties](../reference/objects-and-properties.md); implementation detail belongs to compiler code. |
| Exception/backtrace specification | Semantics moved to [Statements and Control Flow](../reference/statements-and-control-flow.md); implementation summarized in [Exceptions and Backtraces](exceptions-and-backtraces.md). |
| Module system specification | Source behavior migrated to [Modules and Packages](../reference/modules-and-packages.md). |
| Package specification | Search and identity behavior migrated to [Modules and Packages](../reference/modules-and-packages.md) and [Using the Compiler](../compiler/using-dq-comp.md). |
| Module implementation notes | Current design summarized in [Module and Interface Internals](module-and-interface-internals.md); obsolete embedded-interface designs remain historical. |
| DQM interface-format specification | Current user contract is [DQ Module Artifacts](../compiler/dqm-format.md); implementation is summarized in [Module and Interface Internals](module-and-interface-internals.md). |
| Build specification | Current user behavior is [Using the Compiler](../compiler/using-dq-comp.md); superseded `.dqm` layouts are historical. |
| Cross-compiling notes | Maintained user workflow is [Cross-Compiling and Packaging](../compiler/cross-compiling.md); maintainer notes are [Release and Cross-Build Notes](release-and-cross-builds.md). |
| Method-local `use` notes | Migrated to [Declarations and Scope](../reference/declarations-and-scope.md) and [Modules and Packages](../reference/modules-and-packages.md). |
| DQ writing hints for AI agents | Replaced by [Writing Current DQ](writing-current-dq.md), which links to normative pages rather than duplicating specifications. |

## Original Specification Sections

| Original area | Canonical destination |
| --- | --- |
| Overview and design principles | [Home](../index.md), [Motivation](../motivation.md) |
| Lexical structure and literals | [Lexical Structure](../reference/lexical-structure.md) |
| Types, variables, and constants | [Types and Conversions](../reference/types-and-conversions.md), [Declarations and Scope](../reference/declarations-and-scope.md) |
| Expressions and operators | [Expressions and Operators](../reference/expressions-and-operators.md) |
| Statements and control flow | [Statements and Control Flow](../reference/statements-and-control-flow.md) |
| Functions | [Functions](../reference/functions.md) |
| Objects and properties | [Objects and Properties](../reference/objects-and-properties.md) |
| Pointers and memory | [Structures, Pointers, and Function References](../reference/structures-pointers-and-function-references.md), [Objects and Properties](../reference/objects-and-properties.md) |
| Modules and namespaces | [Modules and Packages](../reference/modules-and-packages.md) |
| Compiler directives and C access | [Attributes, Directives, and Interoperability](../reference/attributes-directives-and-interop.md) |
| Standard library | Runtime and Standard Modules sections of the site |
| Rejected features and open questions | Historical only unless reintroduced as a separate design proposal |
