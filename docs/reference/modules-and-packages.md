# Modules and Packages

This page defines source modules, imports, namespaces, packages, and public
interfaces. See the [Modules guide](../language/modules.md) for common forms.

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

An imported module is initialized before code that depends on it executes.
Module initialization follows dependency order and runs once for each canonical
module. Finalization, where supported, occurs in the corresponding reverse
order.

## Method-Local Use

Object methods may opt into module-scope value lookup with `use .` or a module
namespace already available to the surrounding module. This restricted form
does not load a new source module from an arbitrary method body.
