# DQ Module System Specification

Status: draft  
Scope: module naming, file mapping, `use` declarations, public interface handling, re-exporting, aliases, and relative module paths.

---

## 1. Source File = Module

Every DQ source file defines exactly one module.

The module name is derived from the file path relative to a configured source root.

```text
src/dqgui.dq                    -> dqgui
src/dqgui/widgets.dq            -> dqgui/widgets
src/dqgui/widgets/button.dq     -> dqgui/widgets/button
src/dqgui/paint/image.dq        -> dqgui/paint/image
```

There is no required `module` declaration inside the file.

A module path uses `/` as separator:

```dq
use dqgui/widgets;
use dqgui/widgets/button;
use dqgui/paint/image;
```

The source file path rule is:

```text
module path:  a/b/c
source file:  a/b/c.dq
```

A module may have child modules. Therefore this layout is valid:

```text
dqgui/widgets.dq
dqgui/widgets/button.dq
dqgui/widgets/label.dq
```

Here `dqgui/widgets.dq` defines module `dqgui/widgets`, while `dqgui/widgets/button.dq` defines child module `dqgui/widgets/button`.

Directories alone do not define modules. If `dqgui/widgets.dq` does not exist, then `use dqgui/widgets;` is an error, even if directory `dqgui/widgets/` exists.

---

## 2. Implicit Interface Section

A DQ module begins in interface mode by default.

Everything before the optional `implementation` keyword belongs to the public interface of the module.

Example:

```dq
use ../core only(TEvent);
use ./widget only(TWidget);

type TButton = object(TWidget)
  text : str;
endobj

function NewButton(text : str) -> TButton;

implementation

function NewButton(text : str) -> TButton
  result.text = text;
endfunc
```

There is no required `interface` keyword.

There is no required `endmodule`.

A file without `implementation` is an interface-only module.

Example facade module:

```dq
use ./label reexport;
use ./textinput reexport;
use ./button reexport;
use ./window reexport;
```

---

## 3. Implementation Section

The optional `implementation` keyword ends the implicit interface section.

Everything after `implementation` is private implementation content, except definitions that implement declarations from the interface section.

Example:

```dq
use ../core only(TEvent);

function HandleEvent(ev : TEvent);

implementation

use ../platform nomerge;

function HandleEvent(ev : TEvent)
  ...
endfunc
```

The `implementation` keyword is structural and does not require a semicolon.

---

## 4. Statement Terminators

Active statements require `;`.

Examples:

```dq
use dqgui/widgets;
use ./button reexport;
x = y + 1;
return x;
```

Structural keywords do not require `;`.

Examples:

```dq
implementation
endfunc
endobj
```

---

## 5. Qualified Module Access

Qualified module access uses `@`.

The module path ends at the first `.`.

```dq
@dqgui/widgets.TButton
@dqgui/widgets/button.TButton
@dqgui/paint/image.TImage
```

Meaning:

```text
@dqgui/widgets.TButton
 ^^^^^^^^^^^^^ module path
              ^ symbol separator
               ^^^^^^^ exported symbol
```

Everything after the first `.` is normal symbol/member lookup.

Examples:

```dq
btn = @dqgui/widgets.TButton("OK");
btn2 = @dqgui/widgets/button.TButton("Cancel");

w = @dqgui/widgets.TWindow();
w.Show();
```

---

## 6. Basic `use`

The `use` declaration imports another module.

Default behavior:

```dq
use dqgui/widgets;
```

Effects:

```text
1. The namespace @dqgui/widgets becomes available.
2. All public symbols of dqgui/widgets are merged into the current namespace.
```

Example:

```dq
use dqgui/widgets;

btn = TButton("OK");
btn2 = @dqgui/widgets.TButton("Cancel");
```

---

## 7. Module Aliases

A module can be given a local alias:

```dq
use dqgui/widgets as w;
```

Effects:

```text
1. The namespace @w becomes available.
2. All public symbols of dqgui/widgets are merged into the current namespace.
3. The original namespace spelling @dqgui/widgets is not automatically introduced by this use declaration.
```

Example:

```dq
use dqgui/widgets as w;

btn = TButton("OK");
btn2 = @w.TButton("Cancel");
```

A module alias may also be used as the first component of later module paths in the same source file:

```dq
use dqgui/widgets as w;
use w/button;
use w/window;
```

This is equivalent to:

```dq
use dqgui/widgets as w;
use dqgui/widgets/button;
use dqgui/widgets/window;
```

Qualified access with alias paths is allowed:

```dq
btn = @w/button.TButton("OK");
```

Aliases are local to the current source file.

---

## 8. `nomerge`

The `nomerge` modifier imports only the module namespace and does not merge public symbols into the current namespace.

```dq
use dqgui/widgets nomerge;
```

Effects:

```text
1. @dqgui/widgets is available.
2. TButton, TWindow, etc. are not directly available.
```

Example:

```dq
use dqgui/widgets nomerge;

btn = @dqgui/widgets.TButton("OK");  // OK
btn = TButton("OK");                 // error
```

With alias:

```dq
use dqgui/widgets as w nomerge;

btn = @w.TButton("OK");              // OK
btn = TButton("OK");                 // error
```

---

## 9. `only(...)`

The `only(...)` modifier merges only selected public symbols into the current namespace.

The full module namespace remains available.

```dq
use dqgui/widgets only(TButton, TLabel);
```

Effects:

```text
1. @dqgui/widgets is available.
2. TButton and TLabel are directly available.
3. Other public symbols remain available only through @dqgui/widgets.
```

Example:

```dq
use dqgui/widgets only(TButton, TLabel);

btn = TButton("OK");                 // OK
lbl = TLabel("Name:");               // OK
win = TWindow();                     // error
win = @dqgui/widgets.TWindow();      // OK
```

With alias:

```dq
use dqgui/widgets as w only(TButton, TLabel);

btn = TButton("OK");
win = @w.TWindow();
```

Future extension:

```dq
use math only(sin as fsin, cos, PI);
```

Symbol aliases inside `only(...)` are not required for the first implementation, but the grammar should remain compatible with them.

---

## 10. `reexport`

The `reexport` modifier makes imported public symbols part of the current module's public interface.

It is intended for facade modules.

```dq
use ./button reexport;
use ./label reexport;
```

Meaning:

```text
1. Use the imported module normally.
2. Merge its public symbols into the current module namespace.
3. Re-export those symbols as public symbols of the current module.
```

Example file:

```text
dqgui/widgets.dq
```

```dq
use ./widget reexport;
use ./window reexport;
use ./button reexport;
use ./label reexport;
```

Then user code can write:

```dq
use dqgui/widgets;

btn = TButton("OK");
win = TWindow();
```

and qualified access is also available:

```dq
btn = @dqgui/widgets.TButton("OK");
```

Selective re-export:

```dq
use ./button only(TButton) reexport;
```

Effects:

```text
1. Only TButton is merged.
2. Only TButton is re-exported.
3. The full @.../button namespace is still available locally.
```

Invalid combination:

```dq
use ./button nomerge reexport;
```

Reason: `nomerge` says symbols are not inserted into the current namespace, while `reexport` says imported symbols become part of the current module's public namespace.

---

## 11. Relative Module Paths

Module paths are absolute by default:

```dq
use dqgui/widgets/button;
```

A path starting with `./` or `../` is relative to the current module path.

From module:

```text
dqgui/widgets
```

this declaration:

```dq
use ./button;
```

resolves to:

```text
dqgui/widgets/button
```

From module:

```text
dqgui/widgets/button
```

these declarations:

```dq
use ../widget;
use ../../core;
use ../../paint/image;
```

resolve to:

```text
dqgui/widgets/widget
dqgui/core
dqgui/paint/image
```

Relative paths are source-level shortcuts only. The compiler canonicalizes them to absolute module names before semantic analysis.

Qualified access uses either the canonical absolute module name or an explicit alias.

Example:

```dq
use ./button;

btn = TButton("OK");
btn2 = @dqgui/widgets/button.TButton("Cancel");
```

With alias:

```dq
use ./button as btnmod nomerge;

btn = @btnmod.TButton("OK");
```

A relative path that escapes above the module root is an error.

---

## 12. Facade Modules

Facade modules collect and re-export public APIs from child modules.

Example package layout:

```text
dqgui.dq
dqgui/core.dq
dqgui/widgets.dq
dqgui/widgets/button.dq
dqgui/widgets/label.dq
dqgui/paint.dq
dqgui/paint/image.dq
```

Root facade:

```text
dqgui.dq
```

```dq
use ./dqgui/core reexport;
use ./dqgui/widgets reexport;
use ./dqgui/paint reexport;
```

Sub-facade:

```text
dqgui/widgets.dq
```

```dq
use ./widgets/widget reexport;
use ./widgets/window reexport;
use ./widgets/button reexport;
use ./widgets/label reexport;
```

Alternative layout with module path `dqgui/widgets` stored in `dqgui/widgets.dq`:

```text
dqgui/widgets.dq
dqgui/widgets/button.dq
```

Inside `dqgui/widgets.dq`:

```dq
use ./button reexport;
use ./label reexport;
```

This is the preferred compact facade style.

---

## 13. Conflict Rules

Merged symbols must not silently overwrite each other.

This is an error if both modules export `TButton`:

```dq
use gui1/widgets;
use gui2/widgets;
```

Example diagnostic:

```text
ERROR(UseNameConflict): symbol 'TButton' imported from both @gui1/widgets and @gui2/widgets
```

The user must resolve the ambiguity:

```dq
use gui1/widgets only(TWindow, TLabel);
use gui2/widgets only(TButton);
```

or:

```dq
use gui1/widgets nomerge;
use gui2/widgets;

btn = TButton("OK");
other = @gui1/widgets.TButton("Other");
```

`reexport` conflicts are also errors.

Example:

```dq
use ./widgets reexport;
use ./graphics reexport;
```

If both export `TItem`, the facade module must explicitly restrict or rename one side.

---

## 14. Circular Module References

The implicit interface/implementation split is intended to support Pascal-like circular handling.

The compiler processes modules in two conceptual phases:

```text
1. Load and resolve interface sections.
2. Load and resolve implementation sections.
```

Implementation-to-interface cycles may be allowed.

Interface-to-interface cycles are restricted and should be rejected when they require by-value type layout dependencies.

Allowed style:

```dq
// a.dq

type TA = object
  ...
endobj

function MakeA() -> TA;

implementation

use ./b;

function MakeA() -> TA
  @b.Helper();
  ...
endfunc
```

```dq
// b.dq

function Helper();

implementation

use ./a;

function Helper()
  x = @a.MakeA();
endfunc
```

The exact incomplete-type rules are defined separately.

---

## 15. Grammar Sketch

Informal grammar:

```text
module-file:
    interface-item*
    [ "implementation" implementation-item* ]

interface-item:
    use-declaration
  | public-declaration

implementation-item:
    use-declaration
  | private-declaration
  | implementation-definition

use-declaration:
    "use" module-path [ "as" identifier ] use-modifier* ";"

use-modifier:
    "nomerge"
  | "only" "(" symbol-list ")"
  | "reexport"

module-path:
    absolute-module-path
  | relative-module-path

absolute-module-path:
    identifier { "/" identifier }

relative-module-path:
    "." "/" path-tail
  | ".." "/" path-tail
  | ".." "/" relative-module-path

path-tail:
    identifier { "/" identifier }

symbol-list:
    identifier { "," identifier }
```

Semantic restrictions:

```text
- `nomerge` and `only(...)` are mutually exclusive.
- `nomerge` and `reexport` are mutually exclusive.
- `only(...) reexport` is valid.
- `as alias` creates a local module alias.
- A module alias may be used as the first component of later module paths.
```

---

## 16. Summary of Supported Forms

```dq
use xmodule;
use xmodule as xm;

use xmodule nomerge;
use xmodule as xm nomerge;

use xmodule only(Symbol1, Symbol2);
use xmodule as xm only(Symbol1, Symbol2);

use xmodule reexport;
use xmodule only(Symbol1, Symbol2) reexport;

use ./child;
use ../sibling;
use ../../other/module;

use package/submodule as sm;
use sm/child;
```

Examples:

```dq
use dqgui/widgets;
use dqgui/widgets as w;
use dqgui/widgets nomerge;
use dqgui/widgets only(TButton, TLabel);
use dqgui/widgets only(TButton, TLabel) reexport;

use ./button reexport;
use ../core only(TEvent);
use ../../paint/image as img nomerge;
```
