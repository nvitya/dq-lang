# DQ Package Specification

Status: draft  
Scope: package directories, package search paths, package root modules, source lookup, optional manifests, installation locations, package cache, and standard runtime package layout.

This document extends `dq_module_spec.md`. The module specification defines canonical module ids, `use`, `usepath`, `#opt module_root_depth`, relative paths, namespaces, re-exporting, and linker-visible identity. This package specification defines how package names are found on disk.

---

## 1. Basic Model

A DQ package is a named module tree stored in one package directory.

A package has:

```text
package name
package directory
module root directory
package root module
package-local modules
canonical module ids
```

The package name is normally the final directory name of the package directory.

Example:

```text
package search root:   /usr/local/share/dq-packages
package directory:     /usr/local/share/dq-packages/json
package name:          json
module root directory: /usr/local/share/dq-packages/json
package root module:   /usr/local/share/dq-packages/json/json.dq
canonical module id:   json
```

User code:

```dq
use json;
use os;
use serial;
```

The source-level style should remain close to Python package/module naming where practical, but the filesystem layout is stricter.

---

## 2. Package Search Roots

A package search root is a directory that contains package directories.

Example:

```text
/usr/local/share/dq-packages/
  json/
    json.dq
  os/
    os.dq
  serial/
    serial.dq
  sqlite/
    sqlite.dq
    connection.dq
    stmt.dq
```

A package search root must not contain importable `.dq` files directly.

Valid:

```text
/usr/local/share/dq-packages/json/json.dq
/usr/local/share/dq-packages/os/os.dq
```

Invalid as import targets:

```text
/usr/local/share/dq-packages/json.dq
/usr/local/share/dq-packages/os.dq
```

A compiler may ignore direct `.dq` files in a package search root. In diagnostic or verbose mode, it may warn:

```text
WARN(PackageSearchRootFileIgnored): ignoring '/usr/local/share/dq-packages/json.dq';
  package search roots contain package directories only
```

---

## 3. Package Root Module

An absolute module path with exactly one component imports the root module of a package.

Example:

```dq
use json;
```

Resolution:

```text
package name:          json
package-local path:    <root>
canonical module id:   json
source file:           <module-root>/json.dq
namespace:             @json
```

For this package directory:

```text
/usr/local/share/dq-packages/json/
```

this source file is used:

```text
/usr/local/share/dq-packages/json/json.dq
```

This rule is intentionally different from the normal non-root module mapping. It makes single-module utility packages pleasant to use while preserving a uniform package-directory layout.

---

## 4. Source File Mapping

For a package named `P` with module root directory `<root>`, source files map as follows:

```text
canonical module id   source file
-------------------   -----------------------
P                     <root>/P.dq
P/a                   <root>/a.dq
P/a/b                 <root>/a/b.dq
P/a/b/c               <root>/a/b/c.dq
```

Examples:

```text
json                  json/json.dq
serial                serial/serial.dq
serial/port           serial/port.dq
serial/platform/linux serial/platform/linux.dq
sqlite                sqlite/sqlite.dq
sqlite/connection     sqlite/connection.dq
```

Imports:

```dq
use json;
use serial;
use serial/port;
use serial/platform/linux;
use sqlite;
use sqlite/connection;
```

Namespaces created by default:

```text
use json;                  -> @json
use serial;                -> @serial
use serial/port;           -> @port
use serial/platform/linux; -> @linux
use sqlite/connection;     -> @connection
```

Aliases still work according to the module specification:

```dq
use serial/port as sp;
```

creates:

```text
@sp
```

---

## 5. Single-Module Utility Packages

A small package should use this layout:

```text
crc/
  crc.dq

json/
  json.dq

serial/
  serial.dq
```

User code:

```dq
use crc;
use json;
use serial;
```

This is the preferred form for small RTL modules and small third-party utility packages.

A package can later grow without changing the root import:

```text
serial/
  serial.dq
  port.dq
  config.dq
  platform/
    linux.dq
    windows.dq
```

Then all of these are valid:

```dq
use serial;
use serial/port;
use serial/config;
use serial/platform/linux;
```

The package root module can become a facade:

```dq
// serial/serial.dq

use ./port reexport;
use ./config reexport;
```

---

## 6. Multi-Module Packages

A larger package may have a root facade and child modules:

```text
sqlite/
  sqlite.dq
  connection.dq
  stmt.dq
  result.dq
  internal/
    api.dq
```

Root facade example:

```dq
// sqlite/sqlite.dq

use ./connection reexport;
use ./stmt reexport;
use ./result reexport;
```

User code can import the facade:

```dq
use sqlite;
```

or a specific child module:

```dq
use sqlite/connection;
use sqlite/stmt;
```

---

## 7. Manifest-Free Packages

A package manifest is not required for simple packages.

If no manifest exists:

```text
package directory     = directory found in a package search root
package name          = final directory name
module root directory = package directory
root module source    = <package-directory>/<package-name>.dq
```

Example:

```text
/usr/local/share/dq-packages/json/json.dq
```

means:

```text
package name          = json
module root directory = /usr/local/share/dq-packages/json
root module source    = /usr/local/share/dq-packages/json/json.dq
canonical module id   = json
```

---

## 8. Optional Package Manifest

A package may contain a manifest for installed or complex packages.

Recommended manifest name:

```text
dqpkg.toml
```

Minimal example:

```toml
[package]
name = "sqlite"
source_root = "src"
version = "0.1.0"
```

Layout:

```text
sqlite/
  dqpkg.toml
  src/
    sqlite.dq
    connection.dq
    stmt.dq
```

Resolution:

```text
use sqlite;            -> sqlite/src/sqlite.dq
use sqlite/connection; -> sqlite/src/connection.dq
```

Initial required fields:

```text
name         package name
source_root  relative source/module root directory
```

The `version` field is recommended but not required for the first implementation.

If `name` is present, it must match the resolved package name unless the package was selected through an explicit package override.

---

## 9. Package Search Path

The compiler has a list of package search roots.

Recommended sources, in priority order:

```text
1. explicit package mappings:        --pkg name=path
2. current application package
3. project-local package roots:      ./dq-packages or ./.dq/packages
4. command-line package roots:       --pkg-path path
5. environment package roots:        DQ_PACKAGE_PATH
6. user package roots
7. system package roots
8. compiler-shipped standard roots
```

The package search order must be deterministic.

If the same package name is found in multiple roots, the first match wins unless strict duplicate checking is enabled.

In verbose mode, the compiler should print resolved package locations:

```text
package json resolved to /usr/local/share/dq-packages/json
package sqlite resolved to /home/me/project/dq-packages/sqlite
```

---

## 10. Explicit Package Mapping

An explicit package mapping binds a package name directly to a package directory.

Example:

```sh
dq build app.dq --pkg sqlite=/home/me/work/sqlite
```

This is useful for package development.

The mapped directory may be either:

```text
/home/me/work/sqlite/sqlite.dq
```

or, with a manifest:

```text
/home/me/work/sqlite/dqpkg.toml
/home/me/work/sqlite/src/sqlite.dq
```

An explicit package mapping has higher priority than package search roots.

---

## 11. Package Root Import Resolution

For:

```dq
use P;
```

resolution is:

```text
1. Resolve package name P to a package directory.
2. Determine module root directory:
   - if dqpkg.toml exists, use its source_root
   - otherwise use the package directory itself
3. Load root module file:
   <module-root>/P.dq
4. Assign canonical module id:
   P
5. Create default namespace:
   @P
```

Example:

```dq
use os;
```

maps to:

```text
/usr/local/share/dq-packages/os/os.dq
```

and creates:

```text
@os
```

---

## 12. Child Module Import Resolution

For:

```dq
use P/a/b;
```

resolution is:

```text
1. Resolve package name P to a package directory.
2. Determine module root directory.
3. Load module file:
   <module-root>/a/b.dq
4. Assign canonical module id:
   P/a/b
5. Create default namespace from the last path component:
   @b
```

Example:

```dq
use sqlite/connection;
```

maps to:

```text
/usr/local/share/dq-packages/sqlite/connection.dq
```

and creates:

```text
@connection
```

---

## 13. Relative and Package-Root-Relative Imports

Inside a package, relative imports use the already-known package root. They do not search package paths again.

Example package:

```text
serial/
  serial.dq
  port.dq
  platform/
    linux.dq
```

Inside `serial/serial.dq`:

```dq
use ./port;
use ^/platform/linux;
```

Both resolve inside package `serial`:

```text
serial/port
serial/platform/linux
```

A relative path must not escape above the package/module root.

---

## 14. Interaction with `#opt module_root_depth`

When a source file is loaded through package resolution, the package root is already known.

Therefore `#opt module_root_depth` is usually unnecessary inside installed packages.

If present, it must compute the same module root directory as the package resolver. A mismatch is an error.

Example:

```text
ERROR(ModuleRootMismatch): source file declares module root '/x/y',
  but it was imported as part of package root '/a/b'
```

For directly compiled source files outside package resolution, `#opt module_root_depth` remains the source-level way to infer the package root from the file location.

---

## 15. Standard Library and Runtime Packages

The compiler distribution may provide standard packages in a compiler-shipped package root.

Recommended public standard modules:

```text
os/
  os.dq
json/
  json.dq
math/
  math.dq
io/
  io.dq
fs/
  fs.dq
str/
  str.dq
time/
  time.dq
```

User code:

```dq
use os;
use json;
use math;
use io;
```

Compiler-internal runtime support should preferably be separate from public standard modules.

Recommended internal/runtime package name:

```text
dqrt
```

Example:

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

## 16. Installation Locations

Recommended Unix-like package roots:

```text
project-local:
  ./dq-packages
  ./.dq/packages

user:
  ~/.local/share/dq/packages

system:
  /usr/local/share/dq-packages
  /usr/share/dq-packages

compiler-shipped:
  <compiler-prefix>/share/dq/packages
```

Recommended cache roots:

```text
user cache:
  ~/.cache/dq

system/compiler cache:
  <compiler-prefix>/lib/dq/cache
```

Package search roots contain package directories. Cache roots contain compiler-generated artifacts and are not source package roots.

---

## 17. Installation Modes

### 17.1 Source Package

The first implementation should support source packages.

Example:

```text
json/
  json.dq
```

or:

```text
sqlite/
  dqpkg.toml
  src/
    sqlite.dq
    connection.dq
```

The compiler compiles the required modules for the current target.

### 17.2 Source Package with Cache

The package still contains source files. The compiler may cache compiled module interfaces and object files elsewhere.

Example cache layout:

```text
~/.cache/dq/
  x86_64-linux-gnu/
    <compiler-abi-hash>/
      sqlite/
        connection.dqm
        connection.o
```

The cache is not authoritative. It can be deleted and rebuilt.

### 17.3 Binary Package

A future binary package may provide precompiled artifacts:

```text
sqlite/
  dqpkg.toml
  src/
    sqlite.dq
  lib/
    x86_64-linux-gnu/
      <compiler-abi-hash>/
        release/
          sqlite.dqm
          libsqlite_dq.a
```

Binary package compatibility must include at least:

```text
target triple
compiler ABI/interface format version
build mode or optimization mode
relevant compile options
```

Binary packages are not required for the first implementation.

---

## 18. Collision and Error Rules

A package search root must not contain two package directories with the same effective package name.

If a manifest package name differs from the expected package name, it is an error unless explicitly allowed by an override.

Example:

```text
ERROR(PackageNameMismatch): package directory 'json' contains manifest name 'json2'
```

If a package root module is requested but the root module source is missing:

```dq
use json;
```

and no file exists at:

```text
<module-root>/json.dq
```

then the compiler reports:

```text
ERROR(ModuleNotFound): package root module 'json' not found
```

If a child module is requested but the source is missing:

```dq
use sqlite/connection;
```

then the compiler reports:

```text
ERROR(ModuleNotFound): module 'sqlite/connection' not found
```

If two imported modules would create the same local namespace, that remains a module-level namespace conflict handled by the module specification.

---

## 19. Command-Line Options

Recommended first implementation options:

```sh
--pkg NAME=PATH       explicitly map a package name to a package directory
--pkg-path PATH       add a package search root
--std-path PATH       override compiler-shipped standard package root
--rt-path PATH        override compiler runtime package root
```

Environment variable:

```sh
DQ_PACKAGE_PATH=/path/one:/path/two
```

The path separator follows the host platform convention.

---

## 20. Summary

A package search root contains package directories only:

```text
/usr/local/share/dq-packages/
  json/
    json.dq
  os/
    os.dq
  serial/
    serial.dq
```

No importable `.dq` files are allowed directly in the package search root.

Root module imports are concise:

```dq
use json;
use os;
use serial;
```

Root module mapping:

```text
P -> <package-dir>/<P>.dq
```

Child module mapping:

```text
P/a/b -> <package-dir>/a/b.dq
```

With a manifest and `source_root = "src"`:

```text
P     -> <package-dir>/src/P.dq
P/a/b -> <package-dir>/src/a/b.dq
```

This gives DQ Python-like import names while keeping package discovery, installation, and future package management regular and deterministic.
