# Module and Interface Internals

> Contributor implementation note: this page is not part of the DQ language
> contract. It describes the current compiler and may change without a source
> language change.

## Artifact Pair

Each compiled module produces two independent files under `.dqbuild`:

- a native `.o` object containing generated code/data for the linker;
- a standalone `.dqm_if` file containing serialized public semantic data and
  freshness metadata for the DQ compiler.

The `.o` is a normal target object and does not embed the DQ interface. The
compiler reads public declarations from `.dqm_if`, not from the linked executable
or an object section. Earlier designs that used a `.dqm` object with an embedded
interface are obsolete.

## Interface Generation

Interface scanning parses enough of a source module to form its declarations,
types, constants, attributes, imports, and reexports without requiring private
code generation. `--ifgen` emits the standalone interface. A normal compile
emits both artifacts.

Public bodies are serialized only when importer-side compilation requires them,
such as implemented inline assembly templates. Ordinary function bodies remain
in source/code generation and are not general interface payloads.

## Identity and Dependencies

Every interface records canonical module identity, target/ABI-relevant context,
source dependencies, and format/compiler compatibility data. Full compilation
also associates the interface with its matching object file.

Freshness validation checks direct source records and matching artifact metadata.
An invalid, incompatible, truncated, or stale interface is regenerated rather
than trusted.

## Cycles and Child Compilation

The compiler can start interface-only child compilations to satisfy imports.
Build context—target, package roots, defines, optimization, debug/LTO settings,
and artifact root—is passed explicitly. Active interface dependency chains are
tracked so a non-resolvable cycle produces a useful diagnostic rather than blind
recursive retries.

## Writes and Concurrency

Generated files should be written atomically and coordinated per target path so
parallel compilers do not expose partial interfaces or objects. Readers validate
headers and records before constructing semantic objects.

## Format Ownership

The `.dqm_if` record format is compiler-owned and versioned. It is not a stable
third-party ABI or hand-editable interchange format. Use `dq-comp --ifdump` for
inspection rather than parsing it in application code.
