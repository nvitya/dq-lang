# `regexp` package

The `regexp` standard package provides regular expression compilation, matching, searching, capture extraction, replacement, and splitting for DQ.

It is implemented as a native DQ port of Rob Pike's virtual machine (**PikeVM**), which extends Thompson's NFA simulation with submatch capture tracking. Unlike backtracking regex engines, PikeVM executes in guaranteed $O(N \cdot M)$ time with respect to input length $N$ and compiled program size $M$, avoiding catastrophic backtracking on pathological patterns.

Use it with:

```dq
use regexp
```

## Quick Start

```dq
use print
use regexp

func *Main():
    // Compile once and reuse with ORegExp
    var re <- ORegExp('(?P<user>[\\w.+-]+)@(?P<host>[\\w.-]+\\.[A-Za-z]{2,})')
    if re.Search('Contact support@example.org for details'):
        PrintLn('Full match: {} ({}..{})', [re.match, re.match_start, re.match_end])
        PrintLn('User:       {}', [re.named_group['user']])
        PrintLn('Host:       {}', [re[2]])
    endif

    // Iterate over all matches
    var kv <- ORegExp('(\\w+)=(\\d+)')
    kv.StartSearch('x=10, y=20, z=30')
    while kv.SearchNext():
        PrintLn('{} -> {}', [kv[1], kv[2]])
    endwhile

    // One-shot package helpers
    PrintLn('{}', [RegExpReplace('(\\w+)=(\\d+)', 'a=1, b=2', '$1:[$2]')])
endfunc
```

## Supported Regular Expression Syntax

### Literals and Escapes

| Pattern | Meaning |
| --- | --- |
| `a`, `1`, `_` | Literal character |
| `.` | Any character except `\n` (or any character including `\n` when the `s` flag is enabled) |
| `\t`, `\n`, `\r`, `\f`, `\v`, `\a`, `\e` | Control character escapes |
| `\xHH` | Byte value given by two hexadecimal digits `HH` |
| `\0` | Null byte (`char(0)`) |
| `\.`, `\*`, `\+`, `\?`, `\(`, `\)`, `\[`, `\]`, `\{`, `\}`, `\\`, `\^`, `\$`, `\|` | Escaped metacharacter literal |

### Character Classes

| Pattern | Meaning |
| --- | --- |
| `[abc]` | Any of `a`, `b`, or `c` |
| `[a-z0-9]` | Any character in the ranges `a`..`z` or `0`..`9` |
| `[^abc]` | Any byte not in the class |
| `\d` / `\D` | ASCII digit `[0-9]` / non-digit `[^0-9]` |
| `\w` / `\W` | ASCII word character `[0-9A-Za-z_]` / non-word character |
| `\s` / `\S` | ASCII whitespace `[ \t\n\r\f\v]` / non-whitespace |
| `[[:alnum:]]`, `[[:alpha:]]`, `[[:ascii:]]`, `[[:blank:]]`, `[[:cntrl:]]`, `[[:digit:]]`, `[[:graph:]]`, `[[:lower:]]`, `[[:print:]]`, `[[:punct:]]`, `[[:space:]]`, `[[:upper:]]`, `[[:word:]]`, `[[:xdigit:]]` | POSIX character classes inside `[...]` (negated form `[[:^digit:]]` is also supported) |

### Anchors and Zero-Width Assertions

| Pattern | Meaning |
| --- | --- |
| `^` | Start of text, or start of any line when the `m` flag is active |
| `$` | End of text, or end of any line (before `\n` or `\r\n`) when the `m` flag is active |
| `\A` | Start of text (ignores `m` flag) |
| `\z` | End of text (ignores `m` flag) |
| `\b` | Word boundary between `\w` and `\W` (or text boundary) |
| `\B` | Non-word boundary |

### Quantifiers

Quantifiers are greedy by default. Appending `?` makes any quantifier lazy (non-greedy, preferring the shortest match).

| Pattern | Meaning |
| --- | --- |
| `A*` / `A*?` | Zero or more repetitions (greedy / lazy) |
| `A+` / `A+?` | One or more repetitions (greedy / lazy) |
| `A?` / `A??` | Zero or one repetition (greedy / lazy) |
| `A{n}` | Exactly `n` repetitions |
| `A{n,}` / `A{n,}?` | At least `n` repetitions (greedy / lazy) |
| `A{n,m}` / `A{n,m}?` | Between `n` and `m` repetitions inclusive (greedy / lazy) |

### Alternation, Groups, and Inline Flags

| Pattern | Meaning |
| --- | --- |
| `A\|B` | Matches `A` or `B` (left branch has higher priority) |
| `(A)` | Numbered capturing group (`1`, `2`, ...) |
| `(?:A)` | Non-capturing group |
| `(?P<name>A)` or `(?<name>A)` | Named capturing group (also assigned the next group number) |
| `(?ims-ims)` | Inline flag modifier affecting the enclosing group scope |
| `(?ims-ims:A)` | Non-capturing group with scoped inline flags |

### Compilation Flags

Flags can be passed as a string to `ORegExp(pattern, flags)`, `re.Compile(pattern, flags)`, or inline via `(?...)`:

- `'i'`: Case-insensitive ASCII matching.
- `'m'`: Multiline mode (`^` and `$` match at line boundaries in addition to start/end of text).
- `'s'`: Dot-all mode (`.` matches `\n` as well).

## `ORegExp` Object Interface

`ORegExp` compiles a pattern into PikeVM bytecode and pre-allocates the VM execution state so repeated matching and searching perform no dynamic thread allocations.

### Construction and Compilation

- `ORegExp()`
  Creates an uncompiled regular expression object.
- `ORegExp(apattern : strslice)`
- `ORegExp(apattern : strslice, aflags : strslice)`
  Creates and compiles a regular expression. Raises `ERegExp` if the pattern or flags are invalid.
- `re.Compile(apattern : strslice)`
- `re.Compile(apattern : strslice, aflags : strslice)`
  Compiles a new pattern into `re`, replacing any previous program.
- `re.Compiled() -> bool` / `re.compiled`
  Returns `true` if a valid pattern is compiled.

### Matching and Searching

- `re.Match(asubject : strslice) -> bool`
  Returns `true` only if the **entire** subject string matches the pattern.
- `re.PrefixMatch(asubject : strslice, astart : int = 0) -> bool`
  Returns `true` if the pattern matches anchored at byte offset `astart`.
- `re.Search(asubject : strslice, astart : int = 0) -> bool`
  Searches for the first match starting at or after `astart`.
- `re.StartSearch(asubject : strslice, astart : int = 0)`
  Initializes stateful iteration over `asubject` starting at `astart`.
- `re.SearchNext() -> bool`
  Advances to the next match after `StartSearch` (or a previous `Search`/`SearchNext`). Properly advances past empty matches without duplicating zero-length matches at the end of a non-empty match.

### Inspecting Match Results and Captures

 Group `0` represents the entire match; groups `1..re.group_count` represent parenthesized capturing groups in left-to-right order of their opening parentheses.

- `re.matched -> bool`: Whether the last match/search succeeded.
- `re.match -> strslice`: Full matched substring (equivalent to `re.Group(0)`).
- `re.match_start -> int`: Start byte index of the full match (`-1` if unmatched).
- `re.match_end -> int`: End byte index (exclusive) of the full match (`-1` if unmatched).
- `re.match_length -> int`: Byte length of the full match (`0` if unmatched).
- `re[agroup : int] -> strslice` / `re.Group(agroup : int) -> strslice`: Captured slice for group `agroup` (empty slice if unmatched).
- `re.GroupMatched(agroup : int) -> bool`: Returns `true` if group `agroup` participated in the last match.
- `re.GroupStart(agroup : int) -> int`: Start byte index of group `agroup` (`-1` if unmatched).
- `re.GroupEnd(agroup : int) -> int`: End byte index of group `agroup` (`-1` if unmatched).
- `re.GroupLength(agroup : int) -> int`: Byte length of group `agroup` (`0` if unmatched).
- `re.Groups() -> [*]str`: Returns an array of strings `[Group(0), Group(1), ..., Group(group_count)]` for a successful match, or an empty array if unmatched.
- `re.GroupIndex(aname : strslice) -> int`: Returns the 1-based group index for a named capture group, or `-1` if not found.
- `re.GroupName(agroup : int) -> str`: Returns the name of capture group `agroup`, or `''` if unnamed.
- `re.named_group[aname : strslice] -> strslice` / `re.GroupByName(aname : strslice) -> strslice`: Captured slice for named group `aname`.
- `re.GroupMatchedByName(aname : strslice) -> bool`: Returns `true` if named group `aname` matched.

### Extraction, Replacement, and Splitting

- `re.FindAll(asubject : strslice, amax : int = 0) -> [*]str`
  Returns all non-overlapping full matches in `asubject` (up to `amax` matches when `amax > 0`).
- `re.Expand(areplacement : strslice) -> str`
  Expands a replacement template against the current match state. Supported placeholders:
  - `$$`: Literal `$`
  - `$&` or `$0`: Full match
  - `$1` .. `$99`: Numbered capture group
  - `${1}` .. `${99}`: Braced numbered capture group
  - `${name}`: Named capture group
- `re.Replace(asubject : strslice, areplacement : strslice, acount : int = 0) -> str`
  Replaces non-overlapping matches in `asubject` with expanded `areplacement`. If `acount > 0`, replaces at most `acount` matches; otherwise replaces all matches.
- `re.ReplaceFirst(asubject : strslice, areplacement : strslice) -> str`
  Shorthand for `re.Replace(asubject, areplacement, 1)`.
- `re.Split(asubject : strslice, amaxsplit : int = 0) -> [*]str`
  Splits `asubject` around matches of the pattern. If `amaxsplit > 0`, performs at most `amaxsplit` splits.

## Package-Level Convenience Functions

For quick one-off operations, the package provides helper functions (each overloaded with an optional `aflags : strslice` parameter):

- `RegExpMatch(apattern : strslice, asubject : strslice [, aflags : strslice]) -> bool`
- `RegExpSearch(apattern : strslice, asubject : strslice [, aflags : strslice]) -> bool`
- `RegExpFind(apattern : strslice, asubject : strslice [, aflags : strslice]) -> str`
- `RegExpFindAll(apattern : strslice, asubject : strslice [, aflags : strslice, amax : int = 0]) -> [*]str`
- `RegExpReplace(apattern : strslice, asubject : strslice, areplacement : strslice [, aflags : strslice, acount : int = 0]) -> str`
- `RegExpSplit(apattern : strslice, asubject : strslice [, aflags : strslice, amaxsplit : int = 0]) -> [*]str`
- `RegExpEscape(atext : strslice) -> str`: Escapes all regular expression metacharacters in `atext` so it matches as a literal string.

## Error Handling

Invalid patterns, unknown flags, or attempts to execute an uncompiled `ORegExp` raise `ERegExp`:

```dq
try:
    var re <- ORegExp()
    re.Compile('(?P<id>[0-9+')
except ERegExp as e:
    PrintLn('Invalid regex: {}', [e.message])
endtry
```
