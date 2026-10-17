# DQ Programming Language

The DQ is a general-purpose, human-friendly, beginner-friendly compiled programming language.

This repository contains the DQ compiler, runtime, standard packages, and documentation.

The DQ compiler and libraries are currently in __Pre-V1__ phase. The DQ compiler is already pretty reliable, because it uses LLVM for the code generation and libc for the low-level runtime functions. There are some applications running developed in DQ already. More and more libraries and applications will be developed, but the standard library set is still limited yet.

The DQ compiler standard package already contains user-friendly interfaces for many tasks, including:
 * files
 * JSON
 * async sockets
 * http serving
 * database
 * threading

It is very easy to use C libraries from DQ, so if there is no existing DQ standard function for a specific task, you can create a module with the missing C interface bindings.

Any help welcomed in DQ standard package development too.

# DQ Language Documentation

The DQ language and compiler documentation can be found here:

[https://nvitya.github.io/dq-lang/](https://nvitya.github.io/dq-lang/)

For quick start visit this page:

[Getting Started](https://nvitya.github.io/dq-lang/getting-started/)

For more information about the DQ language follow these links:

- [Language Guide](https://nvitya.github.io/dq-lang/language/basics/)
- [Language Reference](https://nvitya.github.io/dq-lang/reference/)

