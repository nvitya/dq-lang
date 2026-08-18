# Operators in C

**In C the following operators have shared meanings:**

 * `&`: bitwise "and" operation / address of
 * `*`: multiplication / pointer dereference
 * `/`: truncated integer division / floating point division

**Further operators in C:**

 * `%`: integer division reminder
 * `&&` or `and`: logical "and"
 * `||` or `or`: logical "or"
 * `!` or `not`: logical "not"
 * `~`: bitwise "not"
 * `^`: bitwise "xor"

# Operators in DQ

**In DQ every different operation has its own symbol:**

 * `&`: address of (widespread standard)
 * `*`: multiplication only
 * `^`: pointer dereference (standard in other languages)
 * `/`: floating point division only (standard in other languages)
 * `IDIV`: truncated integer division
 * `IMOD`: integer division reminder
 * `and`: logical "and" (widespread standard)
 * `AND`: bitwise "and"
 * `or`: logical "or" (widespread standard)
 * `OR`: bitwise "or"
 * `not`: logical "not" (widespread standard)
 * `NOT`: bitwise "not"
 * `XOR`: bitwise "xor"

Further symbols in DQ:
 * `@`: namespace designator (e.g. `@def.LINUX`)
 * `$`: context local specials (e.g. `myarray[0:$end-2]`)
 * `#`: compiler directives
 * `?`: inference marker

If you want this kind of distinguishing you run out of the good symbols quickly, 
that's why many operators are words in DQ.

For the modify-assign statements with a word operator in DQ a leading `=` is required: 
```
regs.OSPEEDR =OR= (1 << pinx2)
```
Othewise it looks awkward:
```
regs.OSPEEDR OR= (1 << pinx2)
```

DQ cannot use the `^` for "xor" because it is used for the pointer-dereferencing, which is fixed.

I was thinking to change some word operators:

 * `band`: bitwise "and"
 * `bor`: bitwise "or"
 * `bnot`: bitwise "not"
 * `bxor`: bitwise "xor"
 * `idiv` or `div`: truncated integer division
 * `imod` or `mod`: integer division remainder

Or maybe prefixing with some not used char:

 * `%and`: bitwise "and"
 * `%or`: bitwise "or"
 * `%not`: bitwise "not"
 * `%xor`: bitwise "xor"

 * `~and`: bitwise "and"
 * `~or`: bitwise "or"
 * `~not`: bitwise "not"
 * `~xor`: bitwise "xor"

 * `!and`: bitwise "and"
 * `!or`: bitwise "or"
 * `!not`: bitwise "not"
 * `!xor`: bitwise "xor"

 * `%div`: truncated integer division
 * `%mod`: integer division remainder

```
a %div= 2
bitmask %or= 2

x = 3 %div 2
```

```
function PinSetup(aportnum : int, apinnum : int, flags : uint) -> bool:
    var regs : ^GPIO_TypeDef = GetGpioRegs(aportnum)
	if regs == null:
		return false
	endif

	if apinnum < 0  or  apinnum > 15:
		return false
	endif

	// 1. turn on port power
	GpioPortEnable(aportnum)

    var n : uint
    var pinx2 : int = apinnum * 2

    // set gpio initial state
    if flags band PINCFG_GPIO_INIT_1 <> 0:
      	regs.BSRR = (1 << apinnum)
    else:
        regs.BSRR = (0x10000 << apinnum)
    endif

    // set mode register
	if flags band PINCFG_AF_MASK <> 0:
		n = 2  // set alternate function mode
	elif flags band PINCFG_ANALOGUE <> 0:
		n = 3
	elif flags band PINCFG_OUTPUT <> 0:
		n = 1
	else:
	    n = 0
	endif
	regs.MODER =band= bnot (3 << pinx2)
	regs.MODER =bor=       (n << pinx2)

	regs.MODER =band= bnot (3 << pinx2)
	regs.MODER =bor=       (n << pinx2)


    // 3. set open-drain
    if flags band PINCFG_OPENDRAIN <> 0:
        regs.OTYPER =bor= (1 << apinnum)
    else:
        regs.OTYPER =band= bnot (1 << apinnum)
    endif

    // 4. set pullup / pulldown
    regs.PUPDR =band= bnot (3 << pinx2)
    if flags band PINCFG_PULLUP <> 0:
        regs.PUPDR =bor= (1 << pinx2) // pullup
    elif flags band PINCFG_PULLDOWN <> 0:
        regs.PUPDR =bor= (2 << pinx2) // pulldown
    endif

    // 5. set speed
    regs.OSPEEDR =band= bnot (3 << pinx2)
    if flags band PINCFG_SPEED_MASK == PINCFG_SPEED_MEDIUM:
        regs.OSPEEDR =bor= (1 << pinx2)
    elif (flags band PINCFG_SPEED_MASK == PINCFG_SPEED_MED2)  or  (flags band PINCFG_SPEED_MASK == PINCFG_SPEED_FAST):
        regs.OSPEEDR =bor= (2 << pinx2)
    elif flags band PINCFG_SPEED_MASK == PINCFG_SPEED_VERYFAST:
        regs.OSPEEDR =bor= (3 << pinx2)  // this is very special, and does not even work for SDRAM pins
    endif

	if flags band PINCFG_AF_MASK <> 0:
		// set the alternate function
		n = (flags >> PINCFG_AF_SHIFT) band 0xF
		if apinnum < 8:
			regs.AFR[0] =band= bnot (0xF << (apinnum * 4))
			regs.AFR[0] =bor=       (n   << (apinnum * 4))
		else:
			regs.AFR[1] =band= bnot (0xF << ((apinnum-8) * 4))
			regs.AFR[1] =bor=       (n   << ((apinnum-8) * 4))
		endif
	endif

    return true
endfunc
```
