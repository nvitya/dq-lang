# Migrating to Operators V2

DQ 0.48.0 replaces the original operator spellings. The compiler accepts only
the V2 forms; it does not provide transitional aliases.

## Migration Tool

From a DQ source checkout, migrate one or more files or directory trees with:

```bash
python3 tools/dq_v1_to_v2.py path/to/project
```

The command updates files in place. Directory traversal selects `.dq`, `.dqh`,
and `.dqi` files. An explicitly named file is processed regardless of its
extension. Use `--check` in CI or before writing:

```bash
python3 tools/dq_v1_to_v2.py --check path/to/project
```

Check mode does not write files and returns status 1 when migration is needed.
The tool preserves all untouched source bytes, permissions, and line endings,
and writes each changed file atomically. Strings, character literals, and
comments are not modified. An unterminated quoted literal or block comment is
reported without changing that file.

## Replacements

| V1 | V2 |
| --- | --- |
| `&value` | `%value` |
| `a AND b` | `a & b` |
| `a OR b` | `a \| b` |
| `NOT a` | `~a` |
| `a XOR b` | `a xor b` |
| `a IDIV b` | `a div b` |
| `a IMOD b` | `a mod b` |
| `a SHL b`, `a SHR b` | `a << b`, `a >> b` |
| `a != b` | `a <> b` |
| `a =AND= b`, `a =OR= b` | `a &= b`, `a \|= b` |
| `a =XOR= b` | `a =xor= b` |
| `a =IDIV= b`, `a =IMOD= b` | `a =div= b`, `a =mod= b` |

The migration is token-aware and does not cascade a newly generated `&` into
the new address-of symbol. Running it again produces no further changes.

The words `as`, `is`, `xor`, `div`, `rem`, and `mod` are reserved in V2. Rename any
identifiers that used those names. Because comments and literals are preserved,
update syntax examples in prose and user-visible diagnostic labels separately.

## Casts and Precedence

V2 adds `expression as Type` as an alternative to `Type(expression)`. Existing
type-call casts do not need migration. The `as` form has the same precedence as
`is` and comparisons and is not chainable; use parentheses before applying
member access or another comparison-level operation.

All existing operator precedence and evaluation semantics remain unchanged.
Only the accepted spellings changed.
