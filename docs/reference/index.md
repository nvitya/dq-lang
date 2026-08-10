# DQ Language Reference

This reference defines the DQ language implemented by the compiler on the
repository's `main` branch. DQ is currently Pre-V1, so incompatible changes may
still be made. A rule belongs here only when it is implemented and covered by
the compiler or its autotests.

If you are learning DQ, begin with the [Language Basics](../language/basics.md)
guide. The guide shows common usage; this reference records exact behavior,
restrictions, and edge cases.

## Reference Map

| Area | Pages |
| --- | --- |
| Source language | [Lexical Structure](lexical-structure.md), [Declarations and Scope](declarations-and-scope.md) |
| Types | [Types and Conversions](types-and-conversions.md), [Enums](enums.md), [Arrays and Slices](arrays-and-slices.md), [Strings and Characters](strings-and-characters.md), [`anyvalue`](anyvalue.md), [Structures, Pointers, and Function References](structures-pointers-and-function-references.md) |
| Execution | [Expressions and Operators](expressions-and-operators.md), [Statements and Control Flow](statements-and-control-flow.md), [Functions](functions.md) |
| Larger units | [Objects and Properties](objects-and-properties.md), [Modules and Packages](modules-and-packages.md) |
| Platform access | [Attributes, Directives, and Interoperability](attributes-directives-and-interop.md), [Assembly Functions](assembly-functions.md) |

Runtime APIs such as string methods and formatting live under
[Runtime Library](../rtl/overview.md). Compiler command-line and build behavior
live under [Compiler and Tools](../compiler/using-dq-comp.md).

## Status Words

- **Reference** means implemented behavior and is normative for the documented
  development revision.
- **Guide** means a shorter explanation of normal use. It must not contradict
  the reference.
- **Contributor documentation** describes implementation rather than the
  language contract.
- **Historical** and **design** material is non-normative and may describe
  discarded or incomplete behavior.

See the [documentation policy](../contributing/documentation-policy.md) for the
authority and maintenance rules.
