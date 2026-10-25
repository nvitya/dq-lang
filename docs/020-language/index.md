# DQ Programming Language

This chapter is the complete DQ language reference, organized from basic syntax
to platform access. Each topic introduces its concepts, then describes syntax,
type rules, lifetime constraints, and examples in the same chapter.

If DQ is new to you, begin with [Getting Started](../010-project/030-getting-started.md)
to install the compiler and run a program. This reference describes DQ 0.93.16.
DQ is still under development, so match the documentation to your compiler version.

Most code blocks illustrate declarations or statements in their surrounding
context; examples containing `func *Main()` are complete programs unless they
explicitly import companion files. Copy a complete program into a `.dq` file
and run it with `dq-run filename.dq`. Assembly examples identify their target.

**Contents**

- [Basic Syntax](010-basic-syntax.md)
- [Basic Types and Conversions](020-basic-types-and-conversions.md)
- [Variables, Constants, References, and Scope](030-variables-constants-references-and-scope.md)
- [Expressions and Operators](040-expressions-and-operators.md)
- [Statements and Control Flow](050-statements-and-control-flow.md)
- [Functions](060-functions.md)
- [Strings and Characters](070-strings-and-characters.md)
- [Arrays and Slices](080-arrays-and-slices.md)
- [Enumerations](090-enumerations.md)
- [Structures and Unions](100-structures-and-unions.md)
- [Pointers](110-pointers.md)
- [Objects and Properties](120-objects-and-properties.md)
- [Memory Management and Ownership](130-memory-management-and-ownership.md)
- [Exceptions and Error Handling](140-exceptions-and-error-handling.md)
- [anyvalue](150-anyvalue.md)
- [Text Formatting](160-text-formatting.md)
- [Modules and Packages](170-modules-and-packages.md)
- [Attributes](180-attributes.md)
- [Compiler Directives](190-compiler-directives.md)
- [C Interoperability](200-c-interoperability.md)
- [Assembly Functions](210-assembly-functions.md)
