# DQ Module Artifacts

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
strict validation, regeneration, and versioning. It records all direct source
dependencies with their paths, sizes, and modification times. Interfaces emitted
by a full compile also identify the matching object by its size and modification
time.

It is not a source format, is not intended for hand editing, and is not a stable
ABI contract between unrelated compiler versions.

## Inspecting Interfaces

Use `--ifdump` to inspect a standalone `.dqm_if`.

```bash
dq-comp --ifdump .dqbuild/x86_64-linux/local/app.dqm_if
```

Use `--ifgen` when only the public interface is needed.

```bash
dq-comp --ifgen module.dq
```

Normal builds do not require checking in `.o` or `.dqm_if` files.
