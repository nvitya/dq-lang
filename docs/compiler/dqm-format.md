# DQM Format

DQ compilation uses `.dqm` and `.dqm_if` artifacts.

| Artifact | Meaning |
| --- | --- |
| `.dqm` | compiled module object file |
| `.dqm_if` | serialized public module interface |
| `.dqm` section `.dqm_if` | interface payload embedded in the object |

The compiler normally writes `.dqm` files. A `.dqm` is linkable object code plus
the public interface metadata needed by other modules.

## Why Interfaces Exist

When a module imports another module, the compiler needs that imported module's
public declarations: exported functions, objects, types, constants, properties,
and imported/reexported interface symbols. Loading a compact interface is faster
and simpler than reparsing implementation code for every import.

## Interface Payload

The `.dqm_if` payload is a compiler-owned binary format. It is designed for:

| Goal | Detail |
| --- | --- |
| fast loading | whole-interface loading with a compact record stream |
| strict validation | reject stale, corrupt, or incompatible files |
| regeneration | missing or stale interfaces can be rebuilt |
| versioning | format and compiler compatibility checks |

It is not a source format, not intended for hand editing, and not a stable ABI
contract between unrelated compiler versions.

## Inspecting Interfaces

Use `--ifdump` to inspect a standalone `.dqm_if` or a `.dqm` with an embedded
interface section.

```bash
dq-comp --ifdump .dqbuild/x86_64-linux/local/app.dqm
```

Use `--ifgen` only when you specifically need a standalone interface file.

```bash
dq-comp --ifgen module.dq
```

Normal builds do not require checking in `.dqm` or `.dqm_if` files.

