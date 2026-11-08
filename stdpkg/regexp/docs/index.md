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
    // Prepare once and reuse with ORegExp
    var re <- ORegExp()
    re.Prepare('(?P<user>[\\w.+-]+)@(?P<host>[\\w.-]+\\.[A-Za-z]{2,})')
    if re.Search('Contact support@example.org for details'):
        PrintLn('Full match: {} ({}..{})', [re.match, re.match_start, re.match_end])
        PrintLn('User:       {}', [re.named_group['user']])
        PrintLn('Host:       {}', [re[2]])
    endif

    // Iterate over all matches
    re.StartSearch('(\\w+)=(\\d+)', 'x=10, y=20, z=30')
    while re.SearchNext():
        PrintLn('{} -> {}', [re[1], re[2]])
    endwhile

    // Prepare and replace in one call
    PrintLn('{}', [re.Replace('(\\w+)=(\\d+)', 'a=1, b=2', '$1:[$2]')])
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

Flags can be passed as a string to `re.Prepare(pattern, flags)`, to the pattern-first method overloads, or inline via `(?...)`:

- `'i'`: Case-insensitive ASCII matching.
- `'m'`: Multiline mode (`^` and `$` match at line boundaries in addition to start/end of text).
- `'s'`: Dot-all mode (`.` matches `\n` as well).

## `ORegExp` Object Interface

`ORegExp` compiles a pattern into PikeVM bytecode and pre-allocates the VM execution state so repeated matching and searching perform no dynamic thread allocations.

### Construction and Preparation

- `ORegExp()`
  Creates an empty, unprepared regular expression object without parsing a pattern or raising `ERegExp`.
- `re.Prepare(apattern : strslice, aflags : strslice = '')`
  Compiles a new pattern into `re`, replacing the previous program and clearing the subject, captures, and search state. Raises `ERegExp` if the pattern or flags are invalid, leaving the object unprepared.
- `re.Prepared() -> bool` / `re.prepared`
  Returns `true` if a valid pattern is prepared.
- `re.Clear()`
  Clears the pattern and all matching state, leaving the object unprepared.

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
- `re.match -> str`: Full matched substring (equivalent to `re.Group(0)`).
- `re.match_start -> int`: Start byte index of the full match (`-1` if unmatched).
- `re.match_end -> int`: End byte index (exclusive) of the full match (`-1` if unmatched).
- `re.match_len -> int`: Byte length of the full match (`0` if unmatched).
- `re[agroup : int] -> str` / `re.Group(agroup : int) -> str`: Captured string for group `agroup` (empty string if unmatched).
- `re.GroupMatched(agroup : int) -> bool`: Returns `true` if group `agroup` participated in the last match.
- `re.GroupStart(agroup : int) -> int`: Start byte index of group `agroup` (`-1` if unmatched).
- `re.GroupEnd(agroup : int) -> int`: End byte index of group `agroup` (`-1` if unmatched).
- `re.GroupLength(agroup : int) -> int`: Byte length of group `agroup` (`0` if unmatched).
- `re.Groups() -> [*]str`: Returns an array of strings `[Group(0), Group(1), ..., Group(group_count)]` for a successful match, or an empty array if unmatched.
- `re.GroupIndex(aname : strslice) -> int`: Returns the 1-based group index for a named capture group, or `-1` if not found.
- `re.GroupName(agroup : int) -> str`: Returns the name of capture group `agroup`, or `''` if unnamed.
- `re.named_group[aname : strslice] -> str` / `re.GroupByName(aname : strslice) -> str`: Captured string for named group `aname`.

### Extraction, Replacement, and Splitting

- `re.Find(asubject : strslice, astart : int = 0) -> str`
  Returns the first full match starting at or after `astart`, or an empty string if none is found. Updates the match and capture state just like `Search`.
- `re.FindAll(asubject : strslice) -> [*]str`
  Returns all non-overlapping full matches in `asubject`.
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

### Pattern-First Overloads

The following overloads call `Prepare(apattern, aflags)` on the same object before performing the operation. The pattern remains prepared for subsequent calls that only supply a subject, and match results and captures remain available on `re`.

```dq
re.Match(apattern : strslice, asubject : strslice, aflags : strslice = '') -> bool
re.PrefixMatch(apattern : strslice, asubject : strslice, aflags : strslice = '', astart : int = 0) -> bool
re.Search(apattern : strslice, asubject : strslice, aflags : strslice = '', astart : int = 0) -> bool
re.StartSearch(apattern : strslice, asubject : strslice, aflags : strslice = '', astart : int = 0)
re.Find(apattern : strslice, asubject : strslice, aflags : strslice = '', astart : int = 0) -> str
re.FindAll(apattern : strslice, asubject : strslice, aflags : strslice = '') -> [*]str
re.Replace(apattern : strslice, asubject : strslice, areplacement : strslice, aflags : strslice = '', acount : int = 0) -> str
re.ReplaceFirst(apattern : strslice, asubject : strslice, areplacement : strslice, aflags : strslice = '') -> str
re.Split(apattern : strslice, asubject : strslice, aflags : strslice = '', amaxsplit : int = 0) -> [*]str
```

For example, `re.Search('hello', text, 'i')` prepares a case-insensitive pattern and searches `text`. A subsequent `re.Search(other_text)` reuses that pattern and its flags. Each pattern-first call replaces the previous pattern and flags; omitted flags default to `''`.

## Escaping Literal Text

`RegExpEscape(atext : strslice) -> str` escapes all regular expression metacharacters in `atext` so it matches as a literal string. It is the package's only free-standing operation.

## Error Handling

Invalid patterns, unknown flags, or attempts to execute an unprepared `ORegExp` raise `ERegExp`:

```dq
try:
    var re <- ORegExp()
    re.Prepare('(?P<id>[0-9+')
except ERegExp as e:
    PrintLn('Invalid regex: {}', [e.message])
endtry
```
