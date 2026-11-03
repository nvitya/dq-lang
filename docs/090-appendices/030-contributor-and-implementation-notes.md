# Contributor and Implementation Notes

These pages explain maintenance and implementation. They are not additional
language rules. The [language reference](../020-language/index.md) describes
user-visible behavior; compiler internals can change without a source-language
change.

- [Building the Compiler from Source](020-building-the-compiler-from-source.md)
  covers prerequisites, the host build, and autotests.
- [Module and Interface Internals](031-module-and-interface-internals.md)
  describes `.o`/`.dqm_if` generation, identity, and concurrency.
- [Exceptions and Backtraces](032-exceptions-and-backtraces.md)
  describes native unwinding, generated cleanup, and fault-recovery limitations.
- [Release and Cross-Build Notes](033-release-and-cross-builds.md)
  covers distribution packages and compiler builds for other hosts.
- [Documentation Coverage](050-documentation-coverage.md) maps the previous
  documentation and archived design notes to their current destinations.

## Documentation Policy

### Published Sources

`docs/` and package-local `docs/` directories are the maintained sources for
the published DQ documentation. The book includes package-local API manuals
through the chapter files in `docs/040-packages/`. The site tracks the compiler
on the repository's `main` branch during Pre-V1 development.

Documentation has four explicitly different roles:

1. **Language Reference** records implemented behavior and is normative for the
   documented revision.
2. **Guides** teach normal usage and link to the reference for complete rules.
3. **Contributor Documentation** describes compiler, runtime, release, and
   maintenance implementation.
4. **Archive** preserves superseded specifications and design work without
   making it part of the current contract.

### Authority

When sources disagree, use this order:

1. compiler behavior demonstrated by autotests and the standard packages;
2. the published language reference;
3. guides and examples;
4. contributor implementation descriptions;
5. archived specifications and proposals.

A disagreement between the first two is a documentation defect or an uncovered
compiler defect. Resolve it by deciding the implemented intent, adding or fixing
an autotest, and updating the reference in the same change.

### Reference Rules

- Document only implemented behavior.
- State types, lifetime, ownership, errors, and boundary conditions explicitly.
- Keep one canonical location for each detailed rule; guides summarize and link.
- Use examples from current syntax and verify non-trivial examples against the
  compiler.
- Do not promise future compatibility while DQ is Pre-V1.
- Put future designs in a clearly non-normative proposal, never inline as if they
  were partially supported.

### Guide Rules

- Optimize for the common path and readable examples.
- Avoid duplicating large tables or edge-case algorithms from the reference.
- Link to the corresponding reference near the beginning of each page.
- Update navigation and cross-references when reorganizing the book.

### Contributor and Design Rules

Contributor pages must say that they describe implementation rather than the
language contract. A proposal must carry a visible **Design proposal — not
implemented** status and must not appear under Language Reference.

When implementation lands, migrate user-visible semantics into the reference,
update relevant tests, and either rewrite the proposal as current internals or
archive it.

### Archival Rules

Archived files live under `doc/archive/`, outside MkDocs. Each file starts with a
historical status banner and points to the
[coverage map](050-documentation-coverage.md) or replacement page.
Archived material is preserved, not maintained: corrections belong in `docs/`.

### Change Checklist

- Does a language change update its reference page and a test?
- Does a new user-facing tool option update the compiler guide?
- Are guide and reference links reciprocal where both exist?
- Does `mkdocs build --strict` pass with no omitted pages or broken anchors?
- Do relevant compiler autotests pass?
- Was superseded design material archived rather than left ambiguous?

## Writing Current DQ

This compact checklist helps contributors and AI agents produce code matching
the repository. It is not another specification: follow the
[Language Reference](../020-language/index.md) and prefer working examples in
`stdpkg/`, `examples/`, and `autotest/tests/` if this checklist becomes stale.

### Default Style

- Use colon/end-keyword blocks and four-space indentation in documentation examples.
- Use explicit types where they clarify an example; follow the [declaration rules](../020-language/030-variables-constants-references-and-scope.md) for inference.
- Use `str` for owned text, `rostr`, `strslice` for read-only borrowed text, and
  `embstr(N)` only for fixed C-compatible storage.
- Use double quotes for text. A one-character literal in single quotes denotes a character; see [Character and String Types](../020-language/070-strings-and-characters.md).
- Use lowercase Boolean operators and uppercase integer/bitwise word operators.
- Prefer `use print` and `PrintLn` for repository examples over raw `printf`.

### Ownership and Aliasing

- `[*]T` owns dynamic array storage; `[]T` borrows a view.
- `str` is managed copy-on-write; `.pchar`, `rostr`, `strslice`, slices, and `embstr`
  descriptors borrow storage.
- Object variables are references. Use `new`/`delete` for heap objects and `<-`
  for fixed storage.
- Use `name : ref T`, `refin`, `refout`, or `refnil` only when aliasing is part
  of the function contract.

### Explicit Boundaries

- Conditions must be `bool`; integers are not truthy.
- Use `Ord` and checked helpers for character or enum ordinals.
- Cast pointers explicitly and document lifetime/ABI assumptions.
- In object methods, qualify outside value symbols or use an allowed local
  `use`; do not assume unrestricted module lookup.

### Before Submitting

- Compile the changed example or run its closest autotest.
- Add an autotest for a new language rule or boundary case.
- Update the reference for language behavior and the guide only when common
  usage changes.
- Do not copy rules out of `doc/archive/` without revalidating them.
