# Modules and Packages

A module is a `.dq` source file with public declarations and, optionally, a
private implementation. A package is a named source tree containing modules.
Modules let code be reused without textual copying and provide namespaces for
its public names.

For a small two-file program, put these files in the same directory.

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

Running `dq-run main.dq` compiles the imported module as needed and prints `5`.
The compiler also supplies the implicit `sys` runtime interface in normal
modules; the low-level `--no-use-sys` option opts out of that convenience.

## Source Module

Each `.dq` file is a module. Its canonical identity comes from its package or
module root and its relative path, not merely its basename. Two source paths
must not resolve to the same canonical module identity with different contents.

Declarations before `implementation` make up the module's public interface.
Declarations after it are private implementation details or definitions of
earlier public declarations.

## Basic Imports

`use` imports a module:

```dq
use print
use ./helpers
```

By default, eligible public symbols merge into unqualified lookup and the module
also receives a namespace. Namespace access uses `@name.Symbol`.

## Aliases and Merge Control

`as` chooses a namespace alias. `--` suppresses unqualified scope merging.

```dq
use ./math_extra as math --
var value : int = @math.Add(1, 2)
```

`only(...)` merges only named interface symbols. `exclude(...)` merges all
eligible symbols except those named. `--`, `only`, and `exclude` are mutually
exclusive merge selectors.

Importing the same canonical module more than once does not create another
runtime module instance. Reusing aliases or merge selections must remain
consistent and unambiguous.

## Reexports

`reexport` makes imported public symbols part of the importing module's public
interface. Selection and alias rules apply before reexporting.

```dq
use ./leaf only(Value, Run) reexport
```

A facade module can therefore expose a stable interface without duplicating
declarations. Reexport conflicts are diagnosed rather than silently shadowed.

## Relative and Root Paths

```dq
use ./child
use ../sibling
use ^/root_child
```

`./` starts at the importing module's directory, `../` moves to its parent, and
`^/` starts at the established module/package root. A path that escapes above
that root is invalid.

Bare imports are resolved through package search roots. The chosen package
directory provides the root from which canonical module names and relative
imports are derived.

## Package Search

The compiler searches built-in standard/runtime roots, installation roots, the
user package directory, and explicit `--pkg-path` roots. Later explicit roots
have higher precedence, allowing a project to override installed packages.

### Built-in Search Roots on Linux

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
provided with `--pkg-path` takes precedence over all built-in roots. If several
`--pkg-path` options are given, the last one takes precedence.

A package is source-first: its directory maps a package name to a module tree.
The root module conventionally has the same name as the package. Optional
project/build metadata does not change the language identity of a module.

## Namespace and Symbol Conflicts

An unqualified symbol import must not make lookup ambiguous. Local declarations,
multiple merged modules, aliases, selected imports, and reexports are checked
for conflicts. Qualification is the resolution mechanism; import order does not
silently choose one of two incompatible declarations.

`@.` explicitly selects the current module's global scope. Inside object methods,
this is also the way to bypass member-first lookup without merging a namespace.

## Interface Loading and Cycles

The compiler serializes public semantic information to standalone `.dqm_if`
files and emits code to paired `.o` files. This is an implementation detail of
incremental compilation; it does not change source-level visibility.

Circular module references are accepted only when their public interfaces can be
formed without requiring private implementation state. A cycle whose interface
cannot be resolved is a compile error with the dependency chain.

## Initialization

Module initialization follows dependency order and runs once for each canonical
module. `*ModuleInit` runs before the application's `*Main`, including generated
initialization for global fixed objects. There is no general automatic
module/global finalization phase; explicitly shut down long-lived resources.

## Method-Local Use

Object methods may opt into module-scope value lookup with `use .` or a module
namespace already available to the surrounding module. This restricted form
does not load a new source module from an arbitrary method body.


## Interface and Implementation Example

A public function can use private helpers without exposing them to importers:

```dq
const PUBLIC_VALUE : int = 1
func PublicValue() -> int ..

implementation

const PRIVATE_VALUE : int = 2

func PublicValue() -> int:
    return PUBLIC_VALUE + PRIVATE_VALUE
endfunc
```

A caller can use `PUBLIC_VALUE` and `PublicValue`, but cannot access
`PRIVATE_VALUE` through the imported namespace. A function body in the public
section is also part of the source interface; `implementation` is useful when
the API should expose only the signature.

## Package Root and Source Options

A bare import such as `use json` resolves a package's root module, conventionally
`json/json.dq`; `use nanonet/nano_http` selects a module within a package tree.
Relative and root-relative imports keep their canonical identity even when
reached through multiple aliases or facades.

For a main source whose root should include parent directories,
`#opt module_root_depth = N` selects how many levels above its directory form
the root. Use it before root-sensitive imports. See
[Compiler Directives](190-compiler-directives.md#source-root-option).

The source package and its build artifacts have different roles. `.dqbuild`,
`.o`, and `.dqm_if` are generated caches, not source imports or a stable ABI
between compiler versions. For cache locations and rebuilding, see
[Build Process and Module Artifacts](../030-compiler/060-build-process-and-module-artifacts.md).
