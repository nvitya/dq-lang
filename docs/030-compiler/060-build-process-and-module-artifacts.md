# Build Process and Module Artifacts

## Artifact Locations

DQ packages and the RTL are source-first. The compiler writes generated module
artifacts under `.dqbuild`.

```text
.dqbuild/<build-tag>/
  local/
    app.o
    app.dqm_if
  pkg/
    print/
      print.o
      print.dqm_if
    rtl/
      rtl_linux.o
      rtl_linux.dqm_if
```

Each module has a normal native `.o` object and a standalone `.dqm_if` interface.
Both are generated artifacts, not source package contents.

Local imports outside the build root are stored under
`.dqbuild/<build-tag>/external/`, using their absolute source path below that
directory (and a drive identifier on Windows). This keeps parent-relative imports
inside the build cache and distinguishes sources with identical filenames.

By default the build root is the directory containing the main `.dq` file. The
default build tag is `<target-arch>-<target-rtl>`, for example
`x86_64-linux`.

The build tag is only a namespace. A tag named `debug` does not imply `-g`, and
a tag named `release` does not imply `-O3`.

By default, local and package artifacts use the same build root. With
`--package-build-root <path>`, package and RTL artifacts are instead written to
`<path>/.dqbuild/<build-tag>/pkg`, while local artifacts remain under the normal
build root. This allows several projects with the same build context to share
compiled package modules.

DQ compilation uses paired `.o` and `.dqm_if` artifacts.

| Artifact | Meaning |
| --- | --- |
| `.o` | native compiled module object file |
| `.dqm_if` | serialized public module interface and freshness metadata |

Both files normally live under `.dqbuild`. The object is a regular linker input
and contains no DQ-specific interface section. The compiler always reads semantic
module data from the standalone `.dqm_if`.

## Why Interfaces Exist

When a module imports another module, the compiler needs that imported module's
public declarations: exported functions, objects, types, constants, properties,
and imported or reexported interface symbols. Loading a compact interface is
faster and simpler than reparsing implementation code for every import.

## Interface Payload

The `.dqm_if` payload is a compiler-owned binary format designed for fast loading,
strict validation, regeneration, and versioning. It records source dependencies
with their paths, sizes, and modification times. Interfaces emitted by a full
compile also identify the matching object by its size and modification time.

Reexports are flattened into the facade interface. The payload contains:

- the facade module's own public declarations;
- selected reexported declarations, grouped by their canonical origin module;
- facade export references that map visible names to the canonical declarations;
- non-exported declarations required to describe exported types and signatures.

The last category is the referenced-type closure. It includes aliases, bases,
fields, methods, properties, function parameters and results, and nested pointer,
array, function-reference, and object-reference types. Closure-only declarations
remain inaccessible through the facade.

Named type references in embedded declarations identify both their origin module
and original name. Consequently a type reached through a facade and the same type
reached through a later direct import share canonical ownership, linkage identity,
and one in-memory representation.

Freshness metadata is flattened as well. A facade records the source metadata and
canonical module-to-source resolution for its complete public-use dependency
graph. Loading the facade therefore reads only its own `.dqm_if`; child interface
files are not opened merely to load reexported declarations. Child source files
are still checked with filesystem metadata, and a facade is regenerated if a
source changes or package-path resolution selects a different source.

The binary format is versioned as a compiler cache. Incompatible interfaces are
rejected and regenerated rather than read through an older reexport layout.

It is not a source format, is not intended for hand editing, and is not a stable
ABI contract between unrelated compiler versions.

## Inspecting Interfaces

Use `--ifdump` to inspect a standalone `.dqm_if`.

```bash
dq-comp --ifdump .dqbuild/x86_64-linux/local/app.dqm_if
```

For flattened facades, the dump lists facade export mappings and origin groups.
Declarations used only for the referenced-type closure are marked `closure`.

Use `--ifgen` when only the public interface is needed.

```bash
dq-comp --ifgen module.dq
```

Normal builds do not require checking in `.o` or `.dqm_if` files.

## Cleaning

Clean `.dqbuild/<tag>` when the compiler version, target, ABI-relevant options,
package sources, or important build defines change.

```bash
rm -rf .dqbuild/x86_64-linux
```

Rebuild after changing compiler versions or target settings if an old cache is
in doubt. Keep `.dqbuild` out of source control; packages are distributed as
source, and generated interfaces are specific to a compiler/build context.

The compiler also tracks textual include and `#srcdep` inputs. For source-level
visibility and reexport rules, see [Modules and Packages](../020-language/170-modules-and-packages.md).
Contributor details are in [Module and Interface Internals](../090-appendices/031-module-and-interface-internals.md).
