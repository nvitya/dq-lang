## Title: Expressions with Word Operators: Which one would you coose?

I was thinking of the ideal expression syntax for the programming language DQ. I started with the (dominating) C syntax.

## Operators in C

**In C the following operators have shared meanings:**

 * `&`: bitwise "and" operation **OR** address of
 * `*`: multiplication **OR** pointer dereference
 * `/`: truncated integer division **OR** floating point division

***Further operators in C:***

 * `%`: integer division reminder
 * `&&` or `and`: logical "and"
 * `||` or `or`: logical "or"
 * `!` or `not`: logical "not"
 * `~`: bitwise "not"
 * `^`: bitwise "xor"
 * `?`: ternary operator

## Operators in DQ

I think **for the good source code readability and clarity every different operation should have a different symbol.** Therefore the shared symbols from C are not taken over. These operators are already fixed in DQ:

 * `&`: address-of operator (widespread standard)
 * `^`: pointer dereference (standard in other languages)
 * `*`: multiplication only
 * `/`: floating point division only (standard in other languages)
 * `or`: logical "or" (widespread standard)
 * `and`: logical "and" (widespread standard)
 * `not`: logical "not" (widespread standard)

These symbols are already fixed for special purposes:
 * `#`: compiler directives (`#ifdef` etc)
 * `$`: context local specials (e.g. `myarray[0:$end-2]`)
 * `?`: inference marker
 * `@`: namespace designator (e.g. `@def.LINUX`)

DQ cannot use the C standard `&`, `|`, `~`, `^` for the bitwise operations, because the `&` and `^` is used for other (fixed) purposes. But **we've run out of the good symbols.** The obvious choice, that other existing languages also use, is reserving some words for the remaining operations. In DQ these (all-capital) words are reserved currently as operators:

 * `AND`: bitwise "and"
 * `OR`: bitwise "or"
 * `NOT`: bitwise "not"
 * `XOR`: bitwise "xor"
 * `IDIV`: truncated integer division
 * `IMOD`: integer division reminder

For the modify-assign statements with a word operator a leading `=` is required, otherwise it looks awkward:
```
regs.OSPEEDR OR= (1 << pinx2)  // invalid
regs.OSPEEDR =OR= (1 << pinx2)
```

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

```
^byte(%wordarr[2])^

a =xor= 1

a = a xor 1

```

## Examples with All-Capital Operators
```
tmp = RCC.CFGR
tmp =AND= NOT 3
tmp =OR= RCC_CFGR_SW_HSI
RCC.CFGR = tmp
while (RCC.CFGR >> 2) AND 3 <> RCC_CFGR_SW_HSI:
endwhile

RCC.CR =AND= NOT RCC_CR_PLLON
while RCC.CR AND RCC_CR_PLLRDY != 0:
endwhile

var pllm : uint = basespeed IDIV pll_input_freq
var plln : uint = vcospeed  IDIV pll_input_freq
var pllq : uint = vcospeed  IDIV 48000000

RCC.PLLCFGR = (0
	OR (pllsrc << 22)
	OR (pllm   <<  0)
	OR (plln   <<  6)
	OR (((pllp >> 1) - 1) << 16)
	OR (pllq   << 24)
)

regs.MODER =AND= NOT (3 << pinx2)
regs.MODER =OR=      (n << pinx2)

if flags AND PINCFG_OPENDRAIN <> 0:
	regs.OTYPER =OR= (1 << apinnum)
else:
	regs.OTYPER =AND= NOT (1 << apinnum)
endif

regs.PUPDR =AND= NOT (3 << pinx2)
if flags AND PINCFG_PULLUP <> 0:
	regs.PUPDR =OR= (1 << pinx2)
elif flags AND PINCFG_PULLDOWN <> 0:
	regs.PUPDR =OR= (2 << pinx2)
endif

```

## Prefixed Word Operators

I'm thinking to change the all-capital word operators with a `%` prefixed lowercase words:

 * `%and`: bitwise "and"
 * `%or`:  bitwise "or"
 * `%not`: bitwise "not"
 * `%xor`: bitwise "xor"
 * `%div` or `%idiv`: truncated integer division
 * `%mod` or `%idiv`: integer division remainder

The sample code would look like this way:

```
tmp = RCC.CFGR
tmp %and= %not 3
tmp %or= RCC_CFGR_SW_HSI
RCC.CFGR = tmp
while (RCC.CFGR >> 2) %and 3 <> RCC_CFGR_SW_HSI:
endwhile

RCC.CR %and= %not RCC_CR_PLLON
while RCC.CR %and RCC_CR_PLLRDY != 0:
endwhile

var pllm : uint = basespeed %div pll_input_freq
var plln : uint = vcospeed  %div pll_input_freq
var pllq : uint = vcospeed  %div 48000000

RCC.PLLCFGR = (0
	%or (pllsrc << 22)  // select PLL source
	%or (pllm   <<  0)
	%or (plln   <<  6)
	%or (((pllp >> 1) - 1) << 16)
	%or (pllq   << 24)
)

regs.MODER %and= %not (3 << pinx2)
regs.MODER %or=       (n << pinx2)

if flags %and PINCFG_OPENDRAIN <> 0:
	regs.OTYPER %or= (1 << apinnum)
else:
	regs.OTYPER %and= %not (1 << apinnum)
endif

regs.PUPDR %and= %not (3 << pinx2)
if flags %and PINCFG_PULLUP <> 0:
	regs.PUPDR %or= (1 << pinx2)
elif flags %and PINCFG_PULLDOWN <> 0:
	regs.PUPDR %or= (2 << pinx2)
endif

```

**Which version do you like more?**
or
**Do you have some other ideas for the operator notation?**
