# DQ Package System Specification

## 1. Purpose

The DQ package system provides a simple way to develop, register, install, and discover DQ libraries independently from the compiler repository.

The initial design intentionally avoids a central online package registry and dependency solver. It builds on the package search mechanism already implemented by `dq-comp`.

The package manager and compiler remain separate:

- `dq-comp` resolves packages through package search paths.
- `dq-pkg` manages package registrations/installations inside those search paths.

This keeps the compiler independent of package-management policy.

---

## 2. Terminology

### Module

A DQ source unit imported with `use`.

Examples:

```dq
use serial
use dqgui/widgets
```

Corresponding files may be:

```text
serial/serial.dq
dqgui/widgets.dq
```

### Package

A top-level package namespace represented by a directory in a package search root.

For:

```dq
use dqgui/widgets
```

`dqgui` is the package and `widgets` is a module inside that package.

A package may contain multiple modules.

### Repository

A source repository, typically Git, containing one or more DQ packages.

A repository and a package are not necessarily the same thing.

---

## 3. Package Search Paths

Bare imports are resolved through package search roots.

The compiler automatically adds standard roots such as:

```text
/usr/lib/dq/stdpkg
<compiler-dir>/../lib/dq/stdpkg
<compiler-dir>/../stdpkg
/usr/lib/dq/packages
<compiler-dir>/../lib/dq/packages
~/.dq/packages
```

Additional roots may be appended with:

```text
--pkg-path <path>
```

Later package roots have higher priority. Therefore, later roots override earlier matching packages.

Example:

```text
/usr/lib/dq/packages/dqgui
~/.dq/packages/dqgui
```

The user-local `dqgui` package is selected because `~/.dq/packages` appears later in the search order.

---

## 4. Package Installation Scopes

DQ distinguishes three package scopes.

### Standard packages

```text
/usr/lib/dq/stdpkg
```

These are part of the DQ platform/compiler distribution.

Normal package-management commands should not install third-party packages here.

### System-global packages

```text
/usr/lib/dq/packages
```

These are third-party packages installed for all users.

System-global installation normally requires administrator/root privileges.

### User packages

```text
~/.dq/packages
```

These are packages installed or linked for the current user.

User scope is the default for `dq-pkg` operations.

The default priority is therefore conceptually:

```text
standard packages
system-global packages
user packages
```

Because later search roots override earlier roots, user packages can override system-global packages. This is useful for development versions.

---

## 5. Initial Package Layout

A package should keep importable DQ modules directly in the package directory.

Example:

```text
dqgui/
    dqpkg.json
    mkdocs.yml

    dqgui.dq
    widgets.dq
    layouts.dq
    graphics.dq

    docs/
        index.md
        widgets.md
        layouts.md

    examples/
        simple_window.dq
        calculator.dq

    autotest/
        widgets_test.dq
        layout_test.dq

    README.md
    LICENSE
```

This allows:

```dq
use dqgui
use dqgui/widgets
use dqgui/layouts
```

A package should preferably contain a module with the same name as the package, for example:

```text
dqgui/dqgui.dq
```

This can serve as the package's main or umbrella module.

A separate `src/` directory is not recommended for package modules because it would conflict with the current package-resolution model.

---

## 6. Standard Package Subdirectories and Documentation

The following package-local directories are standardized:

```text
docs/
examples/
autotest/
```

An optional reserved directory may include:

```text
assets/
```

### `docs/`

Contains the package documentation in MkDocs format.

If a package contains `docs/`, it should also contain a package-local:

```text
mkdocs.yml
```

at the package root. The conventional MkDocs layout is therefore:

```text
<package>/
    mkdocs.yml
    docs/
        index.md
        ...
```

A package's documentation should be independently buildable from the package root, for example:

```text
mkdocs serve
mkdocs build
```

The package-local `mkdocs.yml` should preferably remain small and focus on package-specific information and navigation. Theme selection, shared Markdown extensions, styling, and other global presentation settings should normally be owned by the aggregate DQ documentation build.

Example package configuration:

```yaml
site_name: packages/dqgui

nav:
  - Overview: index.md
  - Widgets: widgets.md
  - Layouts: layouts.md
```

Package documentation may also be included in a combined DQ documentation site. A suitable implementation is the MkDocs monorepo mechanism, where the main documentation configuration includes the package-local `mkdocs.yml` navigation and documentation tree.

Conceptually, a main documentation configuration may contain entries such as:

```yaml
plugins:
  - search
  - monorepo

nav:
  - Home: index.md
  - Standard Packages:
      - JSON: '!include ../stdpkg/json/mkdocs.yml'
      - Serial: '!include ../stdpkg/serial/mkdocs.yml'
```

The exact aggregate build mechanism is tooling policy rather than part of module resolution. The important package-level contract is that documentation stays with the package and uses the standard `mkdocs.yml` + `docs/` layout.

Future package tooling may provide:

```text
dq-pkg docs
dq-pkg docs <package>
dq-pkg docs --all
```

`dq-pkg docs` may build or serve the current/package-specific documentation. `dq-pkg docs --all` may discover registered packages and assemble an aggregate documentation configuration from their `mkdocs.yml` files.

### `examples/`

Contains example applications or sample code using the package.

Future tooling may support commands such as:

```text
dq-pkg examples dqgui
```

### `autotest/`

Contains tests intended for automated compilation and execution.

Future tooling may support:

```text
dq-pkg test dqgui
```

## 7. Package Metadata

Each package should contain:

```text
dqpkg.json
```

Initial metadata should remain intentionally small.

Example:

```json
{
    "name": "dqgui",
    "version": "0.1.0",
    "description": "OpenGL based GUI toolkit for DQ",
    "license": "MIT",
    "dq": ">=1.0"
}
```

Recommended initial fields:

- `name`
- `version`
- `description`
- `license`
- `dq` — required/supported DQ version

Dependency metadata may be added later, for example:

```json
{
    "dependencies": {
        "glfw": ">=1.0",
        "strmap": ">=1.2"
    }
}
```

The first package-system version does not need to implement dependency resolution.

---

## 8. Local Package Registration

The first version of `dq-pkg` may use symbolic links as package registrations.

Example source package:

```text
/home/vitya/work/dqgui
```

Command:

```text
dq-pkg link /home/vitya/work/dqgui
```

Result:

```text
~/.dq/packages/dqgui -> /home/vitya/work/dqgui
```

This provides an effective development workflow because changes in the source repository become immediately visible to the compiler.

The term `link` is preferred over `register` because it describes the actual mechanism.

---

## 9. Initial `dq-pkg` Commands

A minimal first version may provide:

```text
dq-pkg link <path>
dq-pkg unlink <package>
dq-pkg list
dq-pkg info <package>
dq-pkg check
```

User scope is the default.

System-global operations use an explicit option:

```text
dq-pkg link --system <path>
dq-pkg unlink --system <package>
```

System-global commands may need to be executed with administrator/root privileges:

```text
sudo dq-pkg link --system /opt/dq/dqgui
```

Useful listing variants:

```text
dq-pkg list
dq-pkg list --user
dq-pkg list --system
```

---

## 10. Package Information and Shadowing

Because later package roots override earlier roots, `dq-pkg` should make shadowing visible.

Example:

```text
dq-pkg info dqgui
```

Possible output:

```text
dqgui 0.4.0

active:
  ~/.dq/packages/dqgui

shadowed:
  /usr/lib/dq/packages/dqgui
```

`dq-pkg list` may also indicate overridden packages.

`dq-pkg check` should detect or warn about:

- broken symbolic links
- missing metadata
- invalid package names
- package-directory and metadata-name mismatches
- duplicate packages
- shadowed packages
- accidental overriding of standard packages
- missing `mkdocs.yml` when `docs/` exists
- missing `docs/index.md` when package documentation is present
- invalid package documentation configuration

---

## 11. Repositories Containing Multiple Packages

A source repository may contain several independent packages.

Example:

```text
dq-extra/
    packages/
        serial/
            dqpkg.json
            mkdocs.yml
            serial.dq
            linux.dq
            windows.dq
            docs/
            examples/
            autotest/

        sqlite/
            dqpkg.json
            mkdocs.yml
            sqlite.dq
            docs/
            examples/
            autotest/

        csv/
            dqpkg.json
            csv.dq
            autotest/
```

Each package can be linked separately:

```text
dq-pkg link /work/dq-extra/packages/serial
dq-pkg link /work/dq-extra/packages/sqlite
dq-pkg link /work/dq-extra/packages/csv
```

A future convenience command may scan a repository for package metadata:

```text
dq-pkg link-all /work/dq-extra
```

A repository-level manifest is not required initially.

---

## 12. Future Package Installation

A later version may support installation directly from Git repositories without requiring a dedicated DQ package server.

For example:

```text
dq-pkg install https://github.com/example/dqgui.git
```

The package manager could clone or cache sources under a private package cache such as:

```text
~/.dq/package-cache/
```

and expose installed packages through:

```text
~/.dq/packages/
```

System-wide installation could use:

```text
dq-pkg install --system ...
```

A central online package index may be added later, but it is not required for the package format or the initial package-management design.

---

## 13. Initial Scope for DQ 1.0

The recommended initial package system should stay deliberately small.

### Package identity

- package directory name
- matching `name` in `dqpkg.json`

### Package contents

- DQ modules directly in the package directory
- optional `docs/` with package-local `mkdocs.yml`
- `examples/`
- `autotest/`
- metadata

### Package scopes

```text
/usr/lib/dq/stdpkg
/usr/lib/dq/packages
~/.dq/packages
```

### Initial tool functionality

```text
dq-pkg link
dq-pkg unlink
dq-pkg list
dq-pkg info
dq-pkg check
```

### Documentation

Packages may provide independently buildable MkDocs documentation using:

```text
mkdocs.yml
docs/
```

Aggregate documentation support may be added to `dq-pkg` without changing compiler package resolution.

### Metadata

- name
- version
- description
- license
- supported DQ version

The initial implementation does not require:

- online package registry
- dependency solver
- lockfile
- automatic version resolution
- repository-level manifest

These features can be added after practical experience with real DQ packages shows where they are needed.

---

## 14. Design Principle

The package system should remain layered on top of the compiler's existing package search mechanism.

The compiler resolves packages.

The package manager prepares and maintains package search roots.

This separation allows package-management features to evolve without coupling them tightly to `dq-comp`.
