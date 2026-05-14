# DQ Package Specification

Status: draft  
Scope: package directories, package search paths, package root modules, optional manifests, installation locations, package cache, and standard runtime package layout.

This document extends `dq_module_spec.md`. That spec owns canonical module ids, `use`, `usepath`, `#opt module_root_depth`, relative paths, namespaces, re-exporting, and linker-visible identity. This document defines how package names are found on disk.

---

## 1. Model

A DQ package is a named module tree stored in one package directory.

```text
package search root:   /usr/lib/dq/packages
package directory:     /usr/lib/dq/packages/json
package name:          json
module root directory: /usr/lib/dq/packages/json
root module:           /usr/lib/dq/packages/json/json.dq
canonical module id:   json
```

The package name is normally the final directory name of the package directory. Source names should stay close to Python package/module naming where practical, but the filesystem layout is stricter.

---

## 2. Search Roots

A package search root contains package directories only:

```text
/usr/lib/dq/packages/
  json/
    json.dq
  os/
    os.dq
  sqlite/
    sqlite.dq
    connection.dq
```

Files such as `/usr/lib/dq/packages/json.dq` are not import targets. A compiler may ignore them and warn in diagnostic or verbose mode.

Recommended search priority:

```text
1. explicit package mappings: --pkg name=path
2. command-line roots: --pkg-path path
3. user package roots: ~/.dq/packages
4. install-prefix package roots: <compiler-exe-dir>/../lib/dq/packages
5. system package roots: /usr/lib/dq/packages
6. archive-layout standard roots: <compiler-exe-dir>/../stdpkg
7. install-prefix standard roots: <compiler-exe-dir>/../lib/dq/stdpkg
8. system standard roots: /usr/lib/dq/stdpkg
```

Search order must be deterministic. If the same package is found more than once, the first match wins unless strict duplicate checking is enabled.

The `<compiler-exe-dir>` path is the directory containing the resolved
`dq-comp` executable. It should be based on the real executable location, not
only on the current working directory or the spelling of `argv[0]`.

---

## 3. Source Mapping

For package `P` with module root `<root>`:

```text
P       -> <root>/P.dq
P/a     -> <root>/a.dq
P/a/b   -> <root>/a/b.dq
```

Examples:

```text
json              -> json/json.dq
serial/port       -> serial/port.dq
sqlite/connection -> sqlite/connection.dq
```

The one-component absolute module path imports the package root module:

```dq
use json;              // @json
use serial/port;       // @port
use sqlite/connection; // @connection
```

Aliases, `only`, `--`, re-exporting, and namespace conflicts follow the module specification.

---

## 4. Layouts

Small packages should use a root module named after the package:

```text
crc/
  crc.dq

json/
  json.dq
```

A package can grow without changing the root import:

```text
serial/
  serial.dq
  port.dq
  config.dq
  platform/
    linux.dq
```

```dq
use serial;
use serial/port;
use serial/platform/linux;
```

The root module may be a facade:

```dq
// serial/serial.dq
use ./port reexport;
use ./config reexport;
```

The same pattern applies to larger packages:

```text
sqlite/
  sqlite.dq
  connection.dq
  stmt.dq
  result.dq
  internal/
    api.dq
```

---

## 5. Manifests

A manifest is optional. Without one:

```text
package directory     = directory found in a package search root
package name          = final directory name
module root directory = package directory
root module source    = <package-directory>/<package-name>.dq
```

Installed or complex packages may contain `dqpkg.toml`:

```toml
[package]
name = "sqlite"
source_root = "src"
version = "0.1.0"
```

```text
sqlite/
  dqpkg.toml
  src/
    sqlite.dq
    connection.dq
```

Then:

```text
sqlite            -> sqlite/src/sqlite.dq
sqlite/connection -> sqlite/src/connection.dq
```

Initial required fields are `name` and `source_root`. `version` is recommended but not required for the first implementation. If `name` is present, it must match the resolved package name unless an explicit package override allows otherwise.

---

## 6. Resolution

For `use P;`:

```text
1. Resolve package P to a package directory.
2. Determine module root from dqpkg.toml source_root, or use the package directory.
3. Load <module-root>/P.dq.
4. Assign canonical module id P.
5. Create default namespace @P.
```

For `use P/a/b;`, use the same package and module root, then load `<module-root>/a/b.dq`, assign canonical module id `P/a/b`, and create default namespace `@b`.

Inside a package, relative imports use the already-known package root and do not search package paths again:

```dq
use ./port;
use ^/platform/linux;
```

A relative path must not escape above the package/module root.

When a source file is loaded through package resolution, `#opt module_root_depth` is usually unnecessary. If present, it must compute the same module root as the package resolver. Directly compiled source files outside package resolution still use `#opt module_root_depth` as defined by the module specification.

---

## 7. Explicit Mappings and Options

An explicit package mapping binds a package name directly to a package directory and has higher priority than search roots:

```sh
dq build app.dq --pkg sqlite=/home/me/work/sqlite
```

The mapped directory may be manifest-free or manifest-based:

```text
/home/me/work/sqlite/sqlite.dq
/home/me/work/sqlite/dqpkg.toml
/home/me/work/sqlite/src/sqlite.dq
```

Recommended first implementation options:

```sh
--pkg NAME=PATH       explicitly map a package name to a package directory
--pkg-path PATH       add a package search root
--std-path PATH       override compiler-shipped standard package root
--rt-path PATH        override compiler runtime package root
```

The first implementation intentionally does not read package paths from an
environment variable.

---

## 8. Standard and Runtime Packages

The compiler distribution may provide public standard packages in a
compiler-shipped `stdpkg` root:

```text
os/os.dq
json/json.dq
math/math.dq
io/io.dq
fs/fs.dq
str/str.dq
time/time.dq
```

Compiler-internal runtime support should preferably be separate from public standard modules. Recommended internal package name: `dqrt`.

```text
dqrt/
  dqrt.dq
  string.dq
  array.dq
  exception.dq
  memory.dq
```

User code should normally not import `dqrt` directly. The compiler may import or link it implicitly when needed.

---

## 9. Installation and Cache

Recommended Unix-like roots:

```text
user packages:              ~/.dq/packages
install-prefix packages:    <compiler-exe-dir>/../lib/dq/packages
system packages:            /usr/lib/dq/packages
archive-layout stdpkg:      <compiler-exe-dir>/../stdpkg
install-prefix stdpkg:      <compiler-exe-dir>/../lib/dq/stdpkg
system standard root:       /usr/lib/dq/stdpkg
user cache:                 ~/.dq/cache
compiler cache:             /usr/lib/dq/cache
```

Package roots contain source package directories. Cache roots contain compiler-generated artifacts and are not source package roots.

The first implementation should support source packages. The compiler may cache compiled module interfaces and object files elsewhere; the cache is not authoritative and can be rebuilt.

Future binary packages may provide precompiled artifacts. Compatibility must include at least the target triple, compiler ABI/interface format version, build or optimization mode, and relevant compile options. Binary packages are not required for the first implementation.

---

## 10. Errors

Important package-level errors:

```text
PackageNameMismatch
  The manifest package name differs from the expected package name,
  unless explicitly allowed by an override.

ModuleNotFound
  use P; requested missing <module-root>/P.dq
  use P/a/b; requested missing <module-root>/a/b.dq

ModuleRootMismatch
  A loaded source file's #opt module_root_depth computes a different
  root than the package resolver.
```

Namespace conflicts remain module-level errors handled by `dq_module_spec.md`.

---

## 11. Summary

```text
P     -> <package-dir>/<P>.dq
P/a/b -> <package-dir>/a/b.dq

with source_root = "src":
P     -> <package-dir>/src/P.dq
P/a/b -> <package-dir>/src/a/b.dq
```

This gives DQ concise package imports while keeping discovery, installation, and future package management regular and deterministic.
