## DQ Operators V2

 * `.`: structure/object member accesss
 * `[` + `]`: array definition, array member access, pointer offset
 * `#`: compiler directives (`#ifdef` etc)
 * `$`: context local specials (e.g. `myarray[0:$end-2]`)
 * `?`: inference marker
 * `@`: namespace designator (e.g. `@def.LINUX`)

Expression operators:

 * `%`: address-of operator
 * `^`: pointer dereference (standard in other languages)
 * `*`: multiplication only
 * `/`: floating point division only (standard in other languages)
 * `and`: logical "and" (widespread standard)
 * `or`: logical "or" (widespread standard)
 * `not`: logical "not" (widespread standard)
 * `is`: object type check
 * `as`: casting

 * `<<`: bit shift left
 * `>>`: bit shift right
 * `&`: bitwise "and"
 * `|`: bitwise "or"
 * `~`: bitwise "not"
 * `xor`: bitwise "xor"
 * `div`: truncated integer division (also used in other languages)
 * `mod`: integer division reminder (also used in other languages)

Unused symbols:

  `backtick`, `!`, `\`

  Reserve backtick (`) for Haskell type infix operators


## DQ V1 -> V2 migration

Expression precedence and evaluation semantics stay unchanged. `as` has the
same precedence as `is` and comparisons, is non-chainable, and uses the same
explicit conversion rules as the retained `Type(expression)` cast syntax.

The compiler accepts only V2 spellings. `SHL`, `SHR`, and `!=` are removed.

Modify-assignment uses `&=` and `|=` for the symbolic operators. Word operators
retain the leading `=` convention: `=xor=`, `=div=`, and `=mod=`.

The migration tool performs these compound replacements first:

`=AND=` -> `&=`
`=OR=` -> `|=`
`=XOR=` -> `=xor=`
`=IDIV=` -> `=div=`
`=IMOD=` -> `=mod=`

It then replaces original source tokens without cascading generated tokens:

`&` -> `%`

`AND` -> `&`
`OR` -> `|`
`NOT` -> `~`

`XOR` -> `xor`
`IDIV` -> `div`
`IMOD` -> `mod`

`SHL` -> `<<`
`SHR` -> `>>`
`!=` -> `<>`

The words `as`, `is`, `xor`, `div`, and `mod` are reserved.
