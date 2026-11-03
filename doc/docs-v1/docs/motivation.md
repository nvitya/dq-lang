# Motivation and the Story of DQ

## A Short Note About Me

I was born in 1975 in Hungary. I have been writing computer programs since around 1988. I sold my first software in 1994, written in FoxPro. Since 1999, software development has also been my main profession. By 2026, I had used more than 10 different programming languages, counting only those where I had at least a half-year project.

Until 2015, I mostly used Pascal for application development. Since 2015, I have used mainly C++, mostly for embedded software development. Since 2018, I have also been involved in a large and important Python project for device production testing.

## Motivation

The large Python project became more and more difficult to manage. Unexpected runtime errors appeared frequently, and many of them could have been avoided with a compiled language.

Around 2023, I started searching for alternatives to Python. I investigated Mojo, Rust, Zig, and Swift. Unfortunately, none of them were complete or simple enough to replace Python for my use case. I found no better compiled Python alternative than FreePascal. I built a proof of concept in FreePascal to replace the old, badly structured Python code, and it worked ... for me. The main problem was that none of my colleagues knew Pascal. Pascal seems to be considered a dead language with an ever-shrinking community.

I tried to understand why Python became the most popular programming language, and why Pascal became so underrated. I also saw that new languages like Zig can get more developer attention, even when they provide significantly less than FreePascal (for example in GUI support).

That is how the idea was born: how would I imagine the ideal language? At first, I did not want to implement a compiler for it. I only wanted to create the language specification.

## The First Steps

I already had several years of experience creating text parsers, mainly for special configuration files. Some of them even had expression evaluators. I had also developed a lot in assembly language for different architectures, but I did not have experience translating expressions into machine code or doing register allocation. I did not know anything about LLVM.

I had a paid ChatGPT subscription, and I had seen that it was capable of discussing unusual topics too. My first AI prompt about the language design was this, on January 18, 2026:
```
I would like to design a new programming language.
I already know well pascal, python, C++, JavaScript (among other not so generic languages like VHDL, FoxPro, ActionScript, SQL).

What I don't like in C, C++:
- implicit type conversions: 3 / 2 * 10 != 3 * 10 / 2, or masked_value = (value & (1 < 2) ), no explicit boolean
- terrible operator precedence
- not readable syntax (* for pointers and multiplication, function pointer definition etc)

What I don't like in Python:
- not compiled, gives a lot of runtime errors
- object access require self.
- does not fully supports all object oriented features
- way of handling imports
- no explicit boolean

What I don't like in Pascal:
- Case insensitive (later this is rather a problem)
- Handling of var parameters sometimes
- inconsistent run-time library
- weak debugger
- too much heap usage

I was searching for a long time for some utility language, which is generic and stable. I don't like the syntax of Rust, the others are either not supporting object inheritance or have the same C roots with the [3 / 2 * 10 != 3 * 10 / 2] problem.

Is it a good idea to design a new language?
```
(The answer was not very encouraging.)
