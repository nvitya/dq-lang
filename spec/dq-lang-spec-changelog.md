## Appendix B: Changelog

### v0.1.11 (2026-01-23)
- Complete redesign of for loop syntax (section 7.4)
- Added three distinct for loop forms: `to`/`downto` (range-based), `count`/`downcount` (iteration-based), and `while` (condition-based)
- `to`/`downto`: Pascal-style inclusive ranges with optional `step` modifier
- `count`/`downcount`: Iterate exactly N times with optional positive `step`
- `while`: C-style for loop with condition and optional `step` (can be negative)
- All forms support inline variable declaration
- Clarified step defaults: +1 for ascending, -1 for descending

### v0.1.10 (2026-01-23)
- Added section 6.8: Inline Conditional Expression (`iif()`)

### v0.1.9 (2026-01-23)
- Added **Strict Mode** vs **Relaxed Mode** syntax distinction
- Strict mode: assignments require `:=`, blocks use `: ... endXXX` delimiters
- Relaxed mode (default): allows `=` for assignment and `{ ... }` braces
- Added `elif` keyword for chained conditions in strict mode (`else if` is invalid)
- Block closers are delimiter-based (not indentation-based) — indentation is style-enforced only
- Added block closing keywords: `endif`, `endwhile`, `endfor`, `endfunc`, `endobject`, `endtry`, `endensure`, `endconst`, `endinitialization`, `endfinalization`
- Compact single-line form: `if cond: statement;` followed by `endif`
- One-liner blocks allowed when single statement and line ≤ 80 columns
- Updated all specification examples to use strict mode syntax

### v0.1.8 (2026-01-22)
- Added `byte` as alias for `uint8` (no `word` due to platform ambiguity)
- Clarified assignment is statement-only, no chaining allowed (`a := b := 1` is error)
- Added note about `result` variable for function return values
- Changed override syntax to attribute style: `function Speak() [[override]]`
- Simplified module declaration: removed `export` keyword, added braces to `initialization`/`finalization`
- Simplified `use` statements (removed `interface` and `for objects` variants)
- Simplified name resolution rules
- Added `defined()` function for compile-time conditionals (marked TODO for full spec)
- Updated operator precedence examples with `<<` shorthand

### v0.1.7 (2026-01-22)
- Changed constant syntax to `const(type)` for both single-line and block forms
- Added block form `const(type): ... endconst` for grouping related constants

### v0.1.6 (2026-01-22)
- Added enumeration types with mandatory underlying storage type
- Enum values use Pascal-style module scope (no qualification needed)
- Support for explicit values (for protocols, hardware registers)
- Explicit conversions required between enum and integer types

### v0.1.5 (2026-01-22)
- Changed dynamic array syntax from `array<T>` to `T[...]` for consistency with fixed arrays
- Added multi-dimensional array examples
- Added `char(codepoint)` syntax for creating characters from Unicode values

### v0.1.4 (2026-01-22)
- Added `@arg.` namespace for explicit argument access within functions

### v0.1.3 (2026-01-22)
- Added multi-line string literals using triple quotes (`"""..."""` or `'''...'''`)
- Auto-dedent based on closing quote indentation
- Newline trimming at start/end when quotes are on their own lines

### v0.1.2 (2026-01-22)
- Changed address-of operator from `ptr()` function to `&` symbol
- Changed integer modulo from `mod` to `IMOD` keyword (consistency with `IDIV`)
- Added attribute syntax using `[[ ... ]]` brackets
- Added function attributes section with common attributes
- Changed named parameter syntax from `=>` to `:=` for clarity
- Changed compiler directive syntax from `#{comp ...}` to `#{opt ...}`
- Unified string/char literals: both quote styles allowed, type determined by length

### v0.1.1 (2026-01-21)
- Initial draft specification
- Consolidated from multiple ChatGPT design conversations
- Core language features defined
- Many details still marked as TBD/Open Questions