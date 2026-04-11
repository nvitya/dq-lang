/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    version.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:   DQ Compiler Version Description
 */

#define DQ_COMPILER_VERSION  "0.7.0"

/* CHANGE LOG
------------------------------------------------------------------------------------
v0.7.0:
  - scalar constant folding
v0.6.12:
  - Better compiler error recovery to find the next root statements
v0.6.11:
  - Fixed float32 handling
v0.6.10:
  - improved error messages, and error recovery
v0.6.9:
  - bool not parsing fix
v0.6.8:
  - fixed code generation error at floating point negation
  - fixed linking adding -lm option for the possible math functions
v0.6.7:
  - New dq-run utility: compile and run a DQ file in one step
v0.6.6:
  - verbosity switches
  - Default compiler verbosity changed to NONE (gcc like behaviour)
v0.6.5:
  - Printing compiler version with --version switch
v0.6.4:
  - Adding defines (-D<name>) from the command line
v0.6.3:
  - Improved statement error recovery
  - quoted string escape sequences parsing fix
v0.6.2:
  - Error system migration finished, all errors/warnings/hints produce short text identifiers
v0.6.1:
  - The new error handling applied for some parts (work in progress)
v0.6.0:
  - New error handling with central error symbols
v0.5.9:
  - Error handling rework (work in progress)
v0.5.8:
  - Namespaces with @ symbol:  @def for defines, @. for current module root, @dq for builtins
v0.5.7:
  - Preprocessor improvements
    - #define symbols with values
    - #if, #elif conditions
v0.5.6:
  - constant arrays
v0.5.5:
  - Working type aliases, type definitions with the syntax "type PByte = ^byte;"
  - loading global variables fix
v0.5.4:
  - Double pointer parsing fix
  - printf() varargs unsigned fix
  - command line arguments demo added to the dq_hello.dq
v0.5.3:
  - Duplicated postfix parsing removed from the parser
v0.5.2:
  - "." makes auto pointer de-reference when the pointed type is a compound type (struct or object):
    var ep : ^StructElem = &arr_elems[0];
    ep^.id = 1;  // explicit dereferencing
    ep.id  = 1;  // implicit dereferencing
v0.5.1:
  - "dq-comp file_with_main.dq" calls the linker and produces executable.
v0.5.0:
  - First standalone DQ example: dq_hello.dq (without C testbed)
v0.4.4:
  - Support for calling printf() from the libc
  - varargs support for external functions
v0.4.3:
  - Parser improvement with Addressable Expressions
v0.4.2:
  - Unitialized variable fix for function symbols
v0.4.1:
  - Refactored Left-value handling
v0.4.0:
  - struct support added, preliminary version
v0.3.3:
  - Expression parsing cleanup, more advanced postfix parsing
v0.3.2:
  - Improved pointer syntax: (p + 1)^ and p[1]^
v0.3.1:
  - Calling strnlen from libc instead of the internal implementation
v0.3.0:
  - Some simple C string support
v0.2.4:
  - Float to int conversion with round(), ceil(), floor()
  - const definition fix
v0.2.3:
  - Expression evaluation and code generation fixes for float and unsigned handling
  - nicer expression parsing functions
v0.2.2:
  - Array literals ([1,2,3])
  - Array type checking
  - Pointer improvements:
     - pointer arithmetic
     - getting address of array element
v0.2.1:
  - Expression parsing possible memory leak fixes (at errors)
  - Global variable initialisation set
v0.2.0:
  - Fixed size array handling added
  - Expression parsing loops fixed
v0.1.5:
  - Pointer handling added
v0.1.4:
  - Compiler directives can be written also without braces: #define ..., #ifdef ...
    (The directives without braces are terminated by the end of line)
v0.1.3:
  - Integer type fixes
  - debug info for global variables
v0.1.2:
  - Generating error messages
    - when accessing unitialized variables
    - when not setting the function result value
v0.1.1:
  - Binary integer operations (AND, OR, XOR, SHL, SHR)
v0.1.0:
  - working expression evaluations
  - working code generation
  - working debug info
  - command line arguments
v0.0.4:
  - compiler symbol, type, scope objects
v0.0.3:
  - feeder SetPosition functions
v0.0.2: version added
v0.0.1: initial version
------------------------------------------------------------------------------------
*/
