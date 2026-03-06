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

#define DQ_COMPILER_VERSION  "0.2.0"

/* CHANGE LOG
------------------------------------------------------------------------------------
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