# DQ Programming Language

The DQ is an universal, human-friendly, compiled programming language.

This repository contains the language specification and a compiler.

__Warning: The compiler and the DQ language is under development, not recommended for production use yet__

## DQ Language Specification

[Specification](doc/dq-lang-spec/dq-lang-spec.md)

## DQ Compiler

Configure the project from the repository root:

```sh
cmake .
make -j
```

Run the compiler autotests:

```sh
make test
```

Aliases are also available:

```sh
make autotest
make check
make comptest
```

Install the built tools:

```sh
sudo make install
```
