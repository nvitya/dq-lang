# Writing Current DQ

This compact checklist helps contributors and AI agents produce code matching
the repository. It is not another specification: follow the
[Language Reference](../reference/index.md) and prefer working examples in
`stdpkg/`, `examples/`, and `autotest/tests/` if this checklist becomes stale.

## Default Style

- Use colon/end-keyword blocks and two-space indentation.
- Declare variable types explicitly; `[?]T` infers only a fixed-array length.
- Use `str` for owned text, `strview` for read-only borrowed text, and
  `cstring(N)` only for fixed C-compatible storage.
- Use double quotes for text and single quotes for a character scalar.
- Use lowercase Boolean operators and uppercase integer/bitwise word operators.
- Prefer `use print` and `PrintLn` for repository examples over raw `printf`.

## Ownership and Aliasing

- `[*]T` owns dynamic array storage; `[]T` borrows a view.
- `str` is managed copy-on-write; `.pchar`, `strview`, slices, and `cstring`
  descriptors borrow storage.
- Object variables are references. Use `new`/`delete` for heap objects and `<-`
  for fixed storage.
- Use `name : ref T`, `refin`, `refout`, or `refnull` only when aliasing is part
  of the function contract.

## Explicit Boundaries

- Conditions must be `bool`; integers are not truthy.
- Use `Ord` and checked helpers for character or enum ordinals.
- Cast pointers explicitly and document lifetime/ABI assumptions.
- In object methods, qualify outside value symbols or use an allowed local
  `use`; do not assume unrestricted module lookup.

## Before Submitting

- Compile the changed example or run its closest autotest.
- Add an autotest for a new language rule or boundary case.
- Update the reference for language behavior and the guide only when common
  usage changes.
- Do not copy rules out of `doc/archive/` without revalidating them.
