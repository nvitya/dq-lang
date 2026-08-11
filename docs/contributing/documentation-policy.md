# Documentation Policy

## Published Sources

`docs/` is the only maintained source for the published DQ documentation. The
site tracks the compiler on the repository's `main` branch during Pre-V1
development.

Documentation has four explicitly different roles:

1. **Language Reference** records implemented behavior and is normative for the
   documented revision.
2. **Guides** teach normal usage and link to the reference for complete rules.
3. **Contributor Documentation** describes compiler, runtime, release, and
   maintenance implementation.
4. **Archive** preserves superseded specifications and design work without
   making it part of the current contract.

## Authority

When sources disagree, use this order:

1. compiler behavior demonstrated by autotests and the standard packages;
2. the published language reference;
3. guides and examples;
4. contributor implementation descriptions;
5. archived specifications and proposals.

A disagreement between the first two is a documentation defect or an uncovered
compiler defect. Resolve it by deciding the implemented intent, adding or fixing
an autotest, and updating the reference in the same change.

## Reference Rules

- Document only implemented behavior.
- State types, lifetime, ownership, errors, and boundary conditions explicitly.
- Keep one canonical location for each detailed rule; guides summarize and link.
- Use examples from current syntax and verify non-trivial examples against the
  compiler.
- Do not promise future compatibility while DQ is Pre-V1.
- Put future designs in a clearly non-normative proposal, never inline as if they
  were partially supported.

## Guide Rules

- Optimize for the common path and readable examples.
- Avoid duplicating large tables or edge-case algorithms from the reference.
- Link to the corresponding reference near the beginning of each page.
- Preserve existing guide URLs when reorganizing detail beneath the site.

## Contributor and Design Rules

Contributor pages must say that they describe implementation rather than the
language contract. A proposal must carry a visible **Design proposal — not
implemented** status and must not appear under Language Reference.

When implementation lands, migrate user-visible semantics into the reference,
update relevant tests, and either rewrite the proposal as current internals or
archive it.

## Archival Rules

Archived files live under `doc/archive/`, outside MkDocs. Each file starts with a
historical status banner and points to the coverage map or replacement page.
Archived material is preserved, not maintained: corrections belong in `docs/`.

## Change Checklist

- Does a language change update its reference page and a test?
- Does a new user-facing tool option update the compiler guide?
- Are guide and reference links reciprocal where both exist?
- Does `mkdocs build --strict` pass with no omitted pages or broken anchors?
- Do relevant compiler autotests pass?
- Was superseded design material archived rather than left ambiguous?
