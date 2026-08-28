# DQ Project Files (`.dqproj`)

Some projects may require a large amount of compiler options to compile. In order to make the complex project compilation easier the DQ compiler supports project files.

The DQ project files are basically a set of compiler options in a more manageable way.

The project files use the `.dqproj` extension:

Example usage:
```bash
dq-comp application.dqproj
dq-comp -O0 -g -o build/application application.dqproj
```

(the dq-comp options provided in the command line override the options in the .dqproj file)

## Project File Example

```text
// application.dqproj

packagepath = 'packages'
var BOARD_SUPPORT = PackagePath('board_support')

include '${BOARD_SUPPORT}/project/stm32f746.dqproj'

main      = 'src/application.dq'
output    = 'build/${PROJECT_NAME}.elf'
target    = 'arm_m7f-bare'
compiler_runtime = 'libgcc'
c_runtime = 'newlib-nano'
link      = true
optlevel  = 2
debuginfo = true
lto       = false

define BOARD_NUCLEO_F746
define CPU_CLOCK_HZ = 216000000

linkobject = '${BOARD_SUPPORT}/lib/startup.o'
linkoption = '--gc-sections'
```

`arm_m7f-bare` selects the Cortex-M7 single-precision FPU and makes `float`
32-bit. Use `arm_m7fd-bare` for Cortex-M7 devices with a double-precision FPU;
on that target `float` is 64-bit. The explicit `float32` and `float64` types do
not change with the target.

An included fragment might contain the shared target and linker configuration:

```text
// board_support/project/stm32f746.dqproj

linkerpath = '${THIS_DIR}/../lib'
linkscript = '${THIS_DIR}/../ld/stm32f746.ld'
```

## Lexical Rules

The format uses DQ-like identifiers, strings, comments, and statement
terminators. Property and keyword names are case-sensitive.

An identifier starts with an ASCII letter or `_` and may continue with ASCII
letters, decimal digits, or `_`:

```text
FEATURE
CPU_CLOCK_HZ
board2
```

Strings may use single or double quotes. They must end on the same physical
line on which they begin.

```text
'src/main.dq'
"arm_m7f-bare"
```

The recognized string escapes are:

| Escape | Result |
| --- | --- |
| `\n` | newline character |
| `\r` | carriage return |
| `\t` | tab |
| `\\` | backslash |
| `\"` | double quote |
| `\'` | single quote |

An unrecognized escape is preserved as a backslash followed by the escaped
character.

Both DQ comment forms are supported:

```text
// line comment

/* block
   comment */
```

An unterminated string or block comment is an error.

Statements normally end at a newline. A semicolon is optional at the end of a
statement and is required between multiple statements on the same line:

```text
main = 'main.dq'
output = 'app'

main = 'main.dq'; output = 'app'
```

The parts of one statement, including `=`, its value, and the arguments of
`PackagePath()`, may not continue onto another line.

## Statements

A project consists of property assignments and three special statements:
`define`, `var`, and `include`.

```text
property = value
define NAME
define NAME = value
var NAME = value
include 'file.dqproj'
```

Unknown properties, invalid value types, trailing tokens, and duplicate
single-value properties are errors.

### Properties

| Property | Value | Repetition | Meaning |
| --- | --- | --- | --- |
| `main` | path string | once, required | DQ source file compiled as the project entry module. |
| `output` | path string | once, optional | Final executable or object filename. Normal compiler output defaults apply when omitted. |
| `target` | string | once, optional | Compiler target name, using the same names accepted by `--target`. |
| `packagepath` | path string | repeatable | Add a DQ package search root. |
| `link` | boolean | once, optional | Force linking when `true`; compile only when `false`. |
| `exceptions` | boolean | once, optional | Enable or disable DQ exception handling. Defaults to enabled on hosted targets and disabled on bare targets. |
| `dynstrings` | boolean | once, optional | Enable or disable heap-backed dynamic strings. Defaults to enabled on hosted targets and disabled on bare targets. |
| `compiler_runtime` | string | once, optional | Bare-target compiler support runtime: `libgcc` (default) or `none`. |
| `c_runtime` | string | once, optional | Bare-target C runtime: `newlib-nano` (default) or `none`. |
| `linkobject` | path string | repeatable | Add an existing object or other positional linker input. |
| `linkerpath` | path string | repeatable | Pass `--library-path=<path>` to the linker. |
| `linkoption` | string | repeatable | Pass one argument directly to the linker. |
| `linkscript` | path string | once, optional | Pass `--script=<path>` to the linker. |
| `debuginfo` | boolean | once, optional | Enable or disable debug information. |
| `optlevel` | integer `0` through `3` | once, optional | Set the optimization level. |
| `lto` | boolean | once, optional | Enable full LTO when `true`; disable LTO when `false`. |

`main` is required only after the top-level file and all its includes have been
assembled. An included fragment therefore does not need its own `main`.
`output` is always optional.

The `cpu`, `abi`, and `floatabi` property names are reserved for future target
tuning. They are currently rejected as unsupported. They are not silently
ignored.

### Link Mode

Without a `link` property, the compiler retains its automatic link mode:

- A hosted target links a module that contains `*Main`.
- A bare target produces an object by default.

`link = true` is equivalent to explicitly selecting `--link`. It can force a
bare target to link an ELF image. `link = false` is equivalent to `-c` and
always requests compile-only output.

### Exception Handling

Exception handling is enabled by default for hosted targets and disabled by
default for bare targets. Projects can override the target default explicitly:

```text
exceptions = false
```

When enabled, the compiler defines `EXCEPTIONS` in the `@def` scope, so source
and RTL code can select exception-dependent code with `#ifdef EXCEPTIONS`.
Disabling exceptions removes native unwind generation and rejects `try`,
`except`, `finally`, and `raise`; runtime checks remain enabled and report
through the `RuntimeError` hook.

### Dynamic Strings

Dynamic strings are enabled by default for hosted targets and disabled by
default for bare targets. Projects can override the target default explicitly:

```text
dynstrings = true
```

When enabled, the compiler defines `DYNSTRINGS` in the `@def` scope. When
disabled, heap-owning `str` declarations, conversions, concatenation, and
methods are rejected with `DynStringsDisabled`. Non-owning and bounded text
facilities—text literals, `strview`, and bounded `cstring`—remain available.

### Bare-Metal Runtimes

For supported bare-metal targets, the compiler selects bundled libraries that
match the target CPU and floating-point ABI. The defaults are equivalent to:

```text
compiler_runtime = 'libgcc'
c_runtime = 'newlib-nano'
```

`compiler_runtime` supplies compiler-generated ABI helper routines and, when
exceptions are enabled, the C++ exception ABI support and target unwinder.
`c_runtime` supplies Newlib Nano and its no-system fallback stubs, and makes the
matching bundled math library available to `#linklib('m')`. Static archive
members are included only when referenced. Board or application implementations
override the fallback stubs and must provide functional system calls where the
application requires them.

Either property may be set to `none` when the application supplies that part
of the runtime explicitly. These properties are rejected for hosted targets,
whose runtime libraries continue to be selected by the system linker driver.

### Defines

`define` creates a symbol in the compiler's `@def` scope:

```text
define FEATURE
define LOGGING = true
define TRACE = false
define BUFFER_SIZE = 4096
define OFFSET = -16
```

A define without a value has boolean value `true`. Explicit values are limited
to `true`, `false`, or a signed 64-bit decimal integer. Strings, hexadecimal
integers, expressions, and variables are not accepted as define values.

Define names are identifiers and must be unique throughout the assembled
project, including all included files. A command-line `-DNAME` or
`-DNAME=value` replaces the project define with the same name.

Project defines are inherited by imported child modules.

## Paths

The following properties contain paths:

- `main`
- `output`
- `packagepath`
- `linkobject`
- `linkerpath`
- `linkscript`
- `include`

After variable expansion, a relative path is resolved against the directory of
the file containing that statement. The result is converted to an absolute,
lexically normalized path. This rule applies independently to every included
file.

For example, in `project/boards/board.dqproj`:

```text
linkscript = '../ld/board.ld'
```

the path is relative to `project/boards`, not to the process working directory
or the top-level project directory.

The project parser requires included files and `PackagePath()` results to
exist. It does not check the existence of `main`, `output`, `packagepath`,
`linkobject`, `linkerpath`, or `linkscript`; the compiler, filesystem, or linker
reports problems with those paths when they are used.

## Variables

A variable stores a path for later substitution. Its value must be either a
quoted path string or the result of `PackagePath()`:

```text
var SOURCE_DIR = 'src'
var SDK = PackagePath('sdk')

main = '${SOURCE_DIR}/main.dq'
include '${SDK}/project/common.dqproj'
```

A relative quoted value is resolved against the directory of the file that
declares the variable. Consequently, it keeps the same meaning when it is used
later from a different included file.

Variable references use only the explicit `${NAME}` form. Expansion is
performed inside all quoted project values, including path properties,
`target`, `linkoption`, include paths, and `PackagePath()` arguments.

Variable names are case-sensitive, must be declared before use, and are shared
through includes. A variable cannot be declared more than once. User variables
also cannot redefine either built-in variable:

| Variable | Meaning |
| --- | --- |
| `${PROJECT_NAME}` | Name of the top level `.dqproj` file, without extension |
| `${PROJECT_DIR}` | Directory containing the top-level `.dqproj` file. |
| `${THIS_DIR}` | Directory containing the file with the current statement. |

The project language does not expand environment variables, `~`, `$NAME`, or
`%NAME%`. A dollar sign has no special meaning unless it starts `${NAME}`.

## Package Lookup

`PackagePath()` locates a package using the same package-root resolver as a DQ
`use` statement:

```text
var SDK = PackagePath('sdk')
```

The argument must expand to one identifier. If `/opt/dq/packages/sdk` is the
selected package, the function returns the absolute normalized path
`/opt/dq/packages/sdk`, not `/opt/dq/packages`.

Package roots are searched from highest to lowest precedence:

1. Command-line `--pkg-path` roots, with the last command-line root winning.
2. Project `packagepath` roots already encountered, with the last project root
   winning.
3. Compiler default package roots, with the later default root winning.

Command-line package roots are collected before the project is evaluated, so
they are available to every `PackagePath()` call regardless of command-line
position. A project `packagepath` becomes available only after its statement is
evaluated. Includes follow the same textual ordering rule.

A missing package is a project-evaluation error.

## Includes

An include inserts another `.dqproj` file at the point of the statement:

```text
include 'common.dqproj'
include '${SDK}/project/board.dqproj'
```

The path is expanded and resolved relative to the including file. Included
files must also use the `.dqproj` extension.

Includes behave like textual insertion with file-local path anchoring:

- Variables declared before an include are visible inside it.
- Variables declared by an included file remain visible after it returns.
- `packagepath` statements affect later `PackagePath()` calls.
- Single-property, variable, and define duplicate checks span all files.
- Repeatable property ordering follows statement order across includes.

The same file may be included again after its earlier inclusion has completed.
Normal duplicate rules still apply to the repeated statements. Including a file
that is already active is a cycle and is rejected with the canonical include
chain.

## Linker Argument Ordering

`linkerpath`, `linkscript`, and `linkoption` produce linker arguments in their
assembled project statement order. Included statements retain their position in
that order. Command-line `--linker-arg=<arg>` entries follow all project linker
arguments.

The compiler constructs linker inputs in this order:

1. The object generated for `main`.
2. Objects generated for imported DQ modules.
3. Project `linkobject` inputs, in statement order.
4. `-o` and the final output filename.
5. Libraries requested by DQ modules.

Each project or command-line linker argument is passed through the compiler
driver as one linker argument before those positional inputs.

Project parsing does not verify that linker objects, library paths, or linker
scripts exist. Such errors are reported by the linker.

## Command-Line Precedence

The project is parsed before normal command-line option evaluation and before
target-dependent compiler initialization. Project values act as defaults;
explicit command-line selections are then applied on top.

| Setting | Precedence |
| --- | --- |
| Target | `--target` overrides `target`; otherwise the project target overrides the host default. |
| Output | `-o` overrides `output`; otherwise normal output defaults apply when the property is absent. |
| Optimization | `-O0` through `-O3` override `optlevel`. |
| Debug information | `-g` enables debug information even when `debuginfo = false`. There is currently no command-line option to force it off. |
| LTO | `--lto` or `--lto=full|off` overrides `lto`. |
| Exceptions | `--exceptions` or `--no-exceptions` overrides `exceptions`; the last command-line flag wins. |
| Dynamic strings | `--dynstrings` or `--no-dynstrings` overrides `dynstrings`; the last command-line flag wins. |
| Link mode | The first explicit `-c` or `--link` replaces the project mode. Conflicting subsequent command-line link modes are errors. |
| Defines | The first command-line definition of a name removes the project definition of that name. |
| Package roots | Command-line roots have higher lookup precedence than project and default roots. |
| Linker arguments | Project arguments precede command-line `--linker-arg` entries. |

The project target is selected early enough to initialize target-dependent
built-in types correctly. Imported child modules inherit the effective target,
optimization level, debug and LTO settings, defines, build settings, and package
roots through compiler arguments. Child compilations receive the resolved `.dq`
source file and do not parse the project again.

## Errors and Diagnostics

Project evaluation stops at the first error. Diagnostics identify the file,
line, column, diagnostic identifier, and message:

```text
/work/app/application.dqproj(12,8) ERROR(ProjectValue): optlevel must be between 0 and 3
```

Project errors include, among others:

- unreadable project or include files;
- wrong project extensions;
- malformed strings, comments, or statements;
- unknown, duplicate, or unsupported properties;
- invalid property types or values;
- duplicate or unknown variables;
- duplicate project defines;
- missing packages;
- include cycles;
- an assembled project without `main`.

## Compact Grammar

The following grammar is descriptive. `newline` or `;` terminates a statement,
and the final statement may end at end-of-file.

```text
project       := { terminator | statement terminator } [ statement ] EOF

statement     := property '=' property-value
               | 'define' identifier [ '=' define-value ]
               | 'var' identifier '=' variable-value
               | 'include' string

property      := 'main' | 'output' | 'target' | 'packagepath'
               | 'link' | 'linkobject' | 'linkerpath' | 'linkoption'
               | 'linkscript' | 'debuginfo' | 'optlevel' | 'lto'
               | 'exceptions' | 'dynstrings'

property-value := string | boolean | signed-decimal-integer
variable-value := string | 'PackagePath' '(' string ')'
define-value   := boolean | signed-decimal-integer
boolean        := 'true' | 'false'
terminator     := newline | ';'
```

Whitespace and comments may occur between tokens as long as they do not move
the remainder of the current statement onto another line.
