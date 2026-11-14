# Modules and Packages

A **module** is a `.dq` source file. Split a program into modules to keep related
code together and reuse it from other files. A **package** groups modules under
a shared name, such as `json` or `file`.

Here is a complete two-file program. Put both files in the same directory.

`math_extra.dq`:

```dq
func Add(left : int, right : int) -> int ..

implementation

func Add(left : int, right : int) -> int:
    return left + right
endfunc
```

`main.dq`:

```dq
use print
use ./math_extra as math --

func *Main():
    PrintLn("{}", [@math.Add(2, 3)])
endfunc
```

Run `dq-run main.dq` to compile both files and print `5`. `use` imports the
module, `as math` gives it a short name, and `@math.Add` calls its public
function. The trailing `--` keeps that module's names qualified.

The import connects two separate scopes. `Add` belongs to `math_extra.dq`;
`Main` belongs to `main.dq`. The alias `math` is the name the importing file
uses to reach the other module, not a rename of its source file.

## Module Paths from Constants

A `use` path can come from a compile-time string expression enclosed in braces.
Any constants in the expression must be visible at that point:

```dq
use hwconfig
use {UART_MODULE} reexport
```

For example, the project-selected `hwconfig` package can declare:

```dq
const UART_MODULE : rostr = 'hwimpl/uart_a'
```

The expanded value must be a complete module path, without a `.dq` extension.
Normal `use` modifiers, including `as`, `only`, `exclude`, and `reexport`, apply.
Relative paths resolve from the importing module, not the constant's declaration.
Runtime strings cannot select modules.

`DQ.PKGNAME` is a read-only compile-time property containing the configured package
name of the source file containing the expression, without a trailing slash.
Inside an included fragment it uses that fragment's package, just like `^/` in
`use` and `#include`. A source outside configured packages produces an error.

Use it to construct a path that retains its meaning when consumed by another
package:

```dq
const HWUART_IMPL : rostr = "\{DQ.PKGNAME}/arma/rk/impl/hwuart_rk"
use {HWUART_IMPL}
```

String concatenation with constant operands (including `"a" + "b"` and named
string constants) is folded at compile time, even with dynamic strings disabled.


The configuration module and selected implementation are ordinary dependencies.
Changing the constant or resolving the configuration package from a different
source invalidates affected cached interfaces. Use different build tags for
concurrent configurations sharing a package build root.

## Public and Private Declarations

Declarations before `implementation` are public: other modules can use them.
Declarations after it are private, except for definitions of functions already
declared in the public section.

Use this split when callers need only the public signature:

```dq
const PUBLIC_VALUE : int = 1
func PublicValue() -> int ..

implementation

const PRIVATE_VALUE : int = 2

func PublicValue() -> int:
    return PUBLIC_VALUE + PRIVATE_VALUE
endfunc
```

An importing module can use `PUBLIC_VALUE` and `PublicValue`, but cannot access
`PRIVATE_VALUE`. A public function can also have its body directly in the
public section; `implementation` is optional.

## Basic Imports

Use a package name to import an installed module, or a relative path for a
nearby source file. Omit the `.dq` extension:

```dq
use print
use ./helpers
```

By default, public names are available directly and through the module's
namespace. For example, after `use print`, both `PrintLn(...)` and
`@print.PrintLn(...)` are available. A namespace keeps names from different
modules separate.

The compiler normally supplies the `sys` runtime interface automatically, so
basic runtime functions do not need a separate `use sys`.

## Aliases and Merge Control

`as` chooses a namespace name. Add `--` to require that name at each use:

```dq
use ./math_extra as math --
var total : int = @math.Add(2, 3)
```

This helps when two modules have functions with the same name. You can instead
choose which public names are available without qualification:

| Import form | Names available directly |
| --- | --- |
| `use ./helpers` | All eligible public names. |
| `use ./helpers --` | None; use `@helpers.Name`. |
| `use ./helpers only(Read, Write)` | Only `Read` and `Write`. |
| `use ./helpers exclude(Debug)` | All eligible public names except `Debug`. |

`--`, `only`, and `exclude` are alternatives; do not combine them on one import.
Qualification through the namespace remains available. For example,
`only(Read)` makes `Read` available by its short name, but does not make other
public declarations private: they can still be reached as `@helpers.Name`.
Use `implementation` in the defining module to hide a declaration from callers.

## Relative and Root Paths

Use these forms to find files within your project:

| Path | Starting point |
| --- | --- |
| `use ./child` | The importing file's directory. |
| `use ../sibling` | Its parent directory. |
| `use ^/root_child` | The containing package root (packages only). |

Relative imports may contain any number of `../` components, including paths
outside the original module or package root. A missing destination produces
`ModuleNotFound`.

For example, consider this project layout:

```text
project/
    main.dq
    helpers.dq
    tools/
        report.dq
```

From `main.dq`, `use ./helpers` imports `helpers.dq`, and `use ./tools/report`
imports `report.dq`. From `tools/report.dq`, `use ../helpers` reaches the same
helper module, including when `report.dq` is compiled or edited directly.
No source-root option is needed for parent traversal.

`^/` imports require membership in a configured package. The compiler and language
server resolve source and package paths through symlinks and search package roots
in reverse configuration order. Symlinked package directories are supported; a
shadowed package copy does not acquire the active package identity. Standalone
files outside configured packages cannot use `^/`.

Bare imports such as `use json` search for installed packages. The root module
conventionally has the package name, for example `json/json.dq`.
`use nanonet/nano_http` selects another module inside a package.

## Package Search

A package is a source directory containing a module tree. Build metadata is
optional; it does not replace the source modules.

The compiler searches standard and installation directories, your user package
directory, and directories supplied with `--pkg_path`. An explicit package
path can override an installed package. If several `--pkg_path` options are
given, the last takes precedence.

An explicit mapping such as `--pkg=board:boards/adcif` binds a logical package
name to a directory directly, without an extra `board/` directory level.
Mappings take priority over every search root. Command-line mappings override
project mappings; within each level the last mapping for a name wins. In a
project file, write `pkg = 'board:../boards/adcif'`. See
[package lookup](../030-compiler/050-project-files.md#package-lookup) for path
anchoring and validation rules.

These same package search paths configure [include discovery](190-compiler-directives.md#include)
for `#include "package/file.dqi"`; projects do not need a separate include-directory
list. The project-file `pkg_path` setting likewise applies to both module
imports and package-qualified includes. Explicit `pkg` mappings apply to both
as well. The old `--pkg-path` and `packagepath` spellings remain supported aliases.

## Namespace and Symbol Conflicts

If two imported modules expose the same name, qualify it or restrict which
names they make available. Import order does not silently choose one meaning.
Conflicting local declarations, aliases, selected names, and reexports are
also checked.

`@.` explicitly selects the current module's global scope. Inside an object
method, use it to call a module function rather than an object member.

## Initialization

A module can define `*ModuleInit()` for setup that must happen before `*Main`:

```dq
use print

var greeting : str

func *ModuleInit():
    greeting = "Ready"
endfunc

func *Main():
    PrintLn("{}", [greeting])
endfunc
```

This prints `Ready`. A used module initializes before the module that uses it,
and each module initializes once. Global embedded objects are also initialized
before the application starts.

There is no automatic module/global finalization phase. Explicitly release
long-lived resources when the application shuts down.

## Reexports

`reexport` makes an imported module's public names available through your own
module's public interface:

```dq
use ./leaf only(Value, Run) reexport
```

Use this to provide one convenient entry point for a group of modules.
Selection and alias rules apply before reexporting, and conflicting names are
reported as errors. Put a reexport in the public section, before
`implementation`.

For example, a library can keep its work in `reader.dq` and `writer.dq`, and
provide a single `storage.dq` entry point:

```dq
// storage.dq
use ./reader only(Read) reexport
use ./writer only(Write) reexport
```

A caller can then write `use ./storage` and use `Read` and `Write` without
importing the two implementation modules separately. The functions still
belong to their original modules; reexporting does not copy their code or run
their initialization again. This fragment assumes the two companion modules
provide those public functions.

## Method-Local Use

Inside an object method, outside variables, constants, and functions (including
those from the runtime `sys` module and imported modules) are not directly
accessible by their bare names. Placing a method-local `use` statement at the
start of a method body allows direct, unqualified access to outside symbols—such
as `use socket, sys`, `use .`, or `use *`.

Because this feature specifically controls name resolution inside object
methods, its full description, rules, and practical examples are documented in
[Objects and Properties](120-objects-and-properties.md#method-local-use).

## Interface Loading and Cycles

Modules may refer to each other only when the compiler can resolve their public
interfaces. A circular dependency that needs unavailable implementation details
is an error; the diagnostic shows the dependency chain.

The compiler saves public information in `.dqm_if` files and compiled code in
`.o` files. These are generated build artifacts, not files to import in source.

## Package Roots and Module Identity

The compiler identifies each module by its package or module root and relative
path. Importing the same module through another alias or reexport does not
create another runtime instance. Two different source files cannot claim the
same module identity with different contents. Repeated imports must also use
consistent, unambiguous aliases and selections.

`.dqbuild`, `.o`, and `.dqm_if` files are generated caches and may change between
compiler versions. For cache locations and rebuilding, see
[Build Process and Module Artifacts](../030-compiler/060-build-process-and-module-artifacts.md).
The low-level `--no-use-sys` option disables the normally implicit runtime import.

## Built-in Search Roots on Linux

The compiler adds these package search roots automatically. `<compiler-dir>` is
the directory containing the `dq-comp` executable:

```text
/usr/lib/dq/stdpkg
<compiler-dir>/../lib/dq/stdpkg
<compiler-dir>/../stdpkg
/usr/lib/dq/packages
<compiler-dir>/../lib/dq/packages
$HOME/.dq/packages
```

Package roots are checked from last to first. Therefore, a matching package in
`$HOME/.dq/packages` takes precedence over the installation roots, and a root
provided with `--pkg_path` takes precedence over all built-in roots. If several
`--pkg_path` options are given, the last one takes precedence.
