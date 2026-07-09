/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    version.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:   DQ Compiler Version Description
 */

#define DQ_COMPILER_VERSION  "0.31.5"

/* CHANGE LOG
------------------------------------------------------------------------------------
v0.31.5:
  - Compiler fix for inherited method search
  - nanonet/nano_http module added, simple http server is working
v0.31.4:
  - `@..` namespace qualifier for the module own declarations only
v0.31.3:
  - Method body symbol access errors to module root scope display the module alias
    instead of just "@."
v0.31.2:
  - Compiler crash fixed on using incomplete types
v0.31.1:
  - Value symbol searching refactoring
v0.31.0:
  - Method body `use` statement for value symbol merging
v0.30.2:
  - DQ code formatting changed using 4 spaces for indentation
v0.30.1:
  - Refactoring: call handling moved to OScope
v0.30.0:
  - Exception refactoring to zero-cost implemenation
v0.29.1:
  - Fixed virtual function delegation into function reference variables
  - Added nanonet package, so far it is working only on Linux
v0.29.0:
  - Support variables carrying the type of an object (required for object factories)
v0.28.9:
  - "Object" type for accepting any object as argument or storage
v0.28.8:
  - Handling forward declarations in the modules properly
v0.28.7:
  - json module implementation completed
v0.28.6:
  - Integer conversion logic: uint - int -> int, int - uint -> int
v0.28.5:
  - Exception propagation fix
v0.28.4:
  - Working `const STR_CONST : cstring = 'constant'`
v0.28.3:
  - Human-friendly default floating point formatting, not displaying trailing zeroes
v0.28.2:
  - Dynamic array of objects handling fixes
v0.28.1:
  - Debug info gen fixes
v0.28.0:
  - Dynamic string (str) concatenation with the '+' operator
v0.27.7:
  - Compiler fixes for object and dynamic array self-referencing
v0.27.6:
  - Exception handling fix for object methods
  - file module
v0.27.5:
  - Array literals support []byte and other smaller types too
v0.27.4:
  - Allowing returning dynamic arrays
v0.27.3:
  - Implicit use rtl/... fixes
v0.27.2:
  - RTL reorganization
v0.27.1:
  - User friendly error message for missing reexport at the module interfaces
v0.27.0:
  - DQ RTL reorganization
  - Repeated reexports re-use existing modules
v0.26.0:
  - Support for [[inline]] attributes and --lto optimization
v0.25.6:
  - Segfault handling on windows
v0.25.5:
  - Windows and linux backtrace handling refactored using separate rtl/backtrace_lin and rtl/backtrace_win modules
v0.25.4:
  - Windows backtrace printing / 1
v0.25.3:
  - Build finally working on windows
v0.25.2:
  - Windows compilation fixes / 2
v0.25.1:
  - Windows compilation fixes
v0.25.0:
  - Cross-compilation to Windows
v0.24.2:
  - DQ RTL cleanup: removed unnecessary `@.` prefixes
v0.24.1:
  - modify-assign allowed for properties
v0.24.0:
  - properties implemented
v0.23.0:
  - enums implemented
v0.22.4:
  - sys module moved to rtl/sys
v0.22.3:
  - Exception handling changed using the new TypeInfo
v0.22.2:
  - TypeName() function added
v0.22.1:
  - TypeInfo shared between the modules
v0.22.0:
  - Object TypeInfo added
  - Type Testing with `instance is OType`
  - dynamic_cast equivalent: `var o2 : OSecond tryfrom o;
  - TryCast(T, variable)
v0.21.2:
  - Exception formatting changes
v0.21.1:
  - OScope refactoring
v0.20.18:
  - Backtrace formatting changes
v0.20.17:
  - Using libbacktrace instead of addr2line
v0.20.16:
  - Round(), Floor(), Ceil() implemented in-line, they do not use libm calls anymore
  - removed implicit libm dependency
  - linklib generation fixed for .dqm_if
v0.20.15:
  - Pointers are initialized to null by default
  - Top-level signal handler refactoring
v0.20.14:
  - cstring size parsing fix
v0.20.13:
  - Exception refactorings
v0.20.12:
  - new compiler switch: --build-suffix to append a suffix to the default build-tags in the .dqbuild subdir
  - the .dqm artifact file names do not contain compiler switches or defines
v0.20.11:
  - dq-run uses "-g -O0" compiler mode by default
v0.20.10:
  - Debug info generation fix for compound types
v0.20.9:
  - Backtrace printing fix: skipping the first two internal positions
v0.20.8:
  - Exceptions use embedded cstring(127) instead str (dynamic string) for message
v0.20.7:
  - Exception handling fix for not using invalid return values
v0.20.6:
  - Print backtrace with function names and source code lineinfo
v0.20.5:
  - Code generation fix for returning objects
  - Exception handling fixes
v0.20.4:
  - Exceptions / 5: SIGSEGV catching
v0.20.3:
  - Exceptions / 4: locals cleanup, dynamic manager fixes
v0.20.2:
  - Exceptions / 3: backtrace capturing
v0.20.1:
  - Exceptions / 2
v0.20.0:
  - Exceptions / 1
v0.19.13:
  - Duplicated object initialization and destruction code merged
v0.19.12:
  - Managing dynamic strings and dynamic array object members
v0.19.11:
  - All internal functions are converted to PascalCase (Uppercase first letter),
    the only exception is `iif` where both `Iif` and `iif` is accepted
v0.19.10:
  - Format() function added to the strutils module
v0.19.9:
  - Allow the [[nowarn]] attribute after the type specifier:  var cs : [[nowarn]] cstring(4)
v0.19.8:
  - Warning, when the cstring storage size is not divisible with 4
v0.19.7:
  - New methods added to cstring and str: .Add(), .AddFmt()
v0.19.6:
  - No object forward declaration is required for struct/object to linking to own
  - Added strutils module with the StrToInt() function
v0.19.5:
  - Parser error recovery fix for invalid variables in the array literals
v0.19.4:
  - Fixing calling delete from object context
v0.19.3:
  - Allowing object forward declaration with [[forward]]
v0.19.2:
  - Supporting dynamic array of dynamic strings: [*]str
v0.19.1:
  - Removed all ';' semicolons from the DQ RTL
v0.19.0:
  - Supporting multi-dimensional dynamic arrays: [*][*]T, Dynamic Arrays with managed members.
v0.18.3:
  - Forcing using '*' for special object functions everywhere
v0.18.2:
  - Handling missing / empty constuctors/destructors
v0.18.1:
  - No semicolon handling fix for forward declarations
v0.18.0:
  - Semicolons are optional, required only for separating multiple statements in a single line
v0.17.7:
  - Compiler code refactoring / 7: moved type conversion functions into otype_xxx
v0.17.6:
  - Compiler code refactoring / 6: Object + Struct handling merged into otype_compound module
v0.17.5:
  - Compiler code refactoring / 5: Splitting ConvertExprToType and GetAssignTypeConversionCost
v0.17.4:
  - Compiler code refactoring / 4: Splitting ConvertExprToType and GetAssignTypeConversionCost using type virtual functions
v0.17.3:
  - Compiler code refactoring / 3: duplicated helper functions joined around otype_xxx modules
v0.17.2:
  - Compiler code refactoring / 2: dqc_parser splitted into dqc_parser, dqc_parser_stmt, dqc_parser_expr
v0.17.1:
  - Compiler code refactoring / 1: moving functions from parser to AST
v0.17.0:
  - Compiler cross-compilation
v0.16.10:
  - Local for variable allocation fix
v0.16.9:
  - Default optimization level changed to 1!
  - Function exports fixed for -O1
v0.16.8:
  - "print" module added with Print(), PrintLn()
v0.16.7:
  - Array literal overload matching fix
v0.16.6:
  - Struct methods, struct inheritance, implicit self, and managed member restrictions
v0.16.5:
  - RTL: textformat changed using DQ code only
v0.16.4:
  - Single-char ^cchar conversion for printf('\n')
v0.16.3:
  - Getting address for strview characters
v0.16.2:
  - Set() and AppendChar() methods to the strings
v0.16.1:
  - String handling fixes
  - Allow global external variable binding (like 'stdout')
v0.16.0:
  - Implemented compiler anyvalue handling
v0.15.2:
  - Proper expression support at array size declarations (like _pad : [@def.PTRSIZE - 1]byte;)
  - Pointer difference calculation fixes
  - "make test" makes full cleanup first
v0.15.1:
  - Pointer to object casting
v0.15.0:
  - Dynamic strings (str) and strview implementation phase 1
v0.14.4:
  - Compiler fix for returning small integers
v0.14.3:
  - ETypeKind split to future expansions
v0.14.2:
  - allow string literal to cstring conversion
v0.14.1:
  - Slightly optimized cstring handling RTL
v0.14.0:
  - New cstring implementation
v0.13.1:
  - Dynamic arrays implemented according the new specification
v0.13.0:
  - Static arrays of the new array specification implemented:
    - Slice types
    - .length property
    - $end, $last local symbols
    - [?] for inferred length
v0.12.4:
  - ^cchar is now assignable to cstring(n)
v0.12.3:
  - Another object reference handling fix
v0.12.2:
  - Object Reference handling fixes
  - Array slice calculation uses an RTL function now
v0.12.1:
  - .dqm_if function reference writing fix
v0.12.0:
  - First implementation of the dynamic arrays
v0.11.27:
  - Allow braces block mode for object and struct
v0.11.26:
  - cstring initialization fix
v0.11.25:
  - Object comparison fix, constructorless object handling fix
v0.11.24:
  - Fixed calling destructors of parent object when the inherited does not have destructor code
v0.11.23:
  - Object method pointers (Function references with 'of object')
v0.11.22:
  - Compiler fix for global ojbect initializations with 'null'
v0.11.21:
  - Implicit constructor created for the child objects without explicit constructor
v0.11.20:
  - User friendly error message forgetting "@." from object scope
v0.11.19:
  - Extended object checks
v0.11.18:
  - Code refactorings, object specfic type handling moved to OTypeObject
v0.11.17:
  - Object inheritance, inherited calls, virtual/override/final/abstract methods, polymorphic delete
  - DQM interface persistence for object base types and virtual method attributes
v0.11.16:
  - Code refactor: some of the object handling moved to otype_compound.cpp/h
v0.11.15:
  - Bugfix of using objects from modules
v0.11.14:
  - Object storage and lifetime first pass: object references, fixed storage, object constructors/destructors
v0.11.13:
  - Special function declarations: function *Main and function *ModuleInit
v0.11.12:
  - Numeric for loop support, with all the variants (to, downto, count, downcount and while)
v0.11.11:
  - Added new, delete
v0.11.10:
  - Code refactorings preferring object member functions instead of standalone static helper functions
v0.11.9:
  - Using sys module implicitly (--no-use-sys turns this behaviour off)
v0.11.8:
  - Missing colon fix after object and struct definitions
v0.11.7:
  - Module initializations with 'initialization:' ... 'endinitialization'
v0.11.6:
  - Refactored .dqm regeneration
v0.11.5:
  - Implicit entry point main -> "dq_main"
  - Implicit use of module rtl_linux
v0.11.4:
  - installed compiler package search path fix
v0.11.3:
  - dq-comp standard package search paths
  - using .dqbuild for build temporary files
v0.11.2:
  - 'use' 'nomerge' changed to '--', 'nomerge' is not accepted
v0.11.1:
  - Funcition argument mode syntax changed:
     from function myfunc(ref x : int)
     to   function myfunc(x : ref int)
v0.11.0:
  - Array type definition changed to prefix form: "var ia3 : [3]int;"
v0.10.26:
  - Locking changed not generating .lock files anymore
v0.10.25:
  - removing .dqm_if files after creating .dqm
  - lock files for multi-process generation of .dqm files
v0.10.24:
  - 'use' statements:
    - handling forward declarations properly
    - handling circular references trough the implementation block
v0.10.23:
  - Handling multiple, comma separated blocks in the use statements
v0.10.22:
  - Small code refactor using OModulePath for module path handling
v0.10.21:
  - use extensions: ^/pkg/mod, and ./mod_local
  - package search paths
v0.10.20:
  - 'use ... reexport' implemented
v0.10.19:
  - implmmented advanced 'use' cases: only, exclude
v0.10.18:
  - [WIP] Symbols now can be linked to modules
v0.10.17:
  - Mangled linker symbol names
  - [[export('name')]] and [[cexport]] implementation
v0.10.16:
  - Code refactoring
v0.10.15:
  - Automatic compiling / recompiling of the used modules
v0.10.14:
  - use fixes for global variables
v0.10.13:
  - Simple use statement implementation (no chained compiling yet)
v0.10.12:
  - Handle --ifdump with .dqm files (extract from object files)
v0.10.11:
  - Embed .dqm_if section into compiled .dqm objects
v0.10.10:
  - Full compilation emits .dqm object artifacts by default
v0.10.9:
  - --ifdump format adjustments
v0.10.8:
  - .dqm_if loading and --ifdump implementation
v0.10.7:
  - Structural type specs in binary module interfaces
v0.10.6:
  - added offsetof() internal function
v0.10.5:
  - Struct / object field alignment + [[packed]]
v0.10.4:
  - Small interface wrinting refactoring
v0.10.3:
  - Interface writing (--ifgen) in binary format
v0.10.2:
  - Interface writing preparation (text format so far)
v0.10.1:
  - Work in progress: Module interface generation with --ifgen
v0.9.2:
  - Constant array generation fix
v0.9.1:
  - Object methods share the capabilities like normal root functions
v0.9.0:
  - Simple object type introduced
v0.8.15:
  - #linklib directive
v0.8.14:
  - Function return type parsing fix
v0.8.13:
  - Function forward declarations
v0.8.12:
  - Some code for function overloading was moved to otype_func
v0.8.11:
  - Function overload resolution for direct calls and callbacks
v0.8.10:
  - Ignore argument mode for overload duplicate checks
v0.8.9:
  - Restrict same return type for the overloaded functions
v0.8.8:
  - Function overload v1: collection, no call resolution yet
v0.8.7:
  - ParseFunctionSignature simplification
v0.8.6:
  - Default parameters for FuncRef types
v0.8.5:
  - Code cleanup at function signature parsing
v0.8.4:
  - Function reference (callback) variables
v0.8.3:
  - ref local variables
  - ref/refin/refout/refnull function parameters
v0.8.2:
  - Function default arguments
v0.8.1:
  - Attribute simplification using flags
v0.8.0:
  - Generalized [[attribute]] parsing
v0.7.15:
  - iif() type conversion cleanup
v0.7.14:
  - Expression folding improvements, better tree re-writing
v0.7.13:
  - Removed unnecessary constant folding calls
v0.7.12:
  - Internal changes for Type conversion calls and Expression folding
v0.7.11:
  - Removed duplicate code from preprocessor #if... branches
v0.7.10:
  - const declaration is allowed in statement blocks too
v0.7.9:
  - eliminated more duplicate code with AST helpers
v0.7.8:
  - eliminated some duplicated code parts
  - functions moved from dqc_parser to dqc_ast
v0.7.7:
  - improved variable not initialized checks
v0.7.6:
  - partially re-activated variable not initialized checks
v0.7.5:
  - statement parser reworked phase 2: expression parsion termination rework for +=, -= etc
v0.7.4:
  - statement parser reworked phase 1: migrated to re-using the expression parsing
v0.7.3:
  - pointer type
  - type casting
v0.7.2:
  - Unified type conversion function
v0.7.1:
  - Added iif(cond, a, b) intrinsic with lazy evaluation
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
