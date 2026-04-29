# DQ Module System Specification

Status: draft
Scope: module naming, file mapping, `use`, `usepath`, public interface handling, re-exporting, namespaces, aliases, and relative module paths.

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
usepath dqgui/widgets as w;
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

## 5. Module Paths vs Namespaces

DQ separates module paths from namespace names.

```text
module path  = used by `use` and `usepath` to resolve source files
namespace    = local name created by `use` for qualified access
```

Module paths may contain `/`:

```dq
use dqgui/widgets/button;
```

Namespace usage after `@` does not contain module paths.

Valid:

```dq
@button.TButton
@widgets.TButton
@img.TImage
```

Invalid:

```dq
@dqgui/widgets/button.TButton
@dqgui/widgets.TButton
@w/button.TButton
```

Qualified lookup has the form:

```text
@namespace.Symbol
@namespace.Symbol.member
@namespace.Symbol.method()
```

Only a local namespace identifier may appear after `@`.

---

## 6. Local Namespace Creation

Every `use` declaration creates exactly one local namespace, unless this is disallowed by a namespace conflict.

Without `as`, the namespace name is the last component of the resolved module path.

```dq
use dqgui/widgets;
```

creates namespace:

```dq
@widgets
```

```dq
use dqgui/widgets/button;
```

creates namespace:

```dq
@button
```

With `as`, the alias is the namespace name:

```dq
use dqgui/widgets/button as btnmod;
```

creates namespace:

```dq
@btnmod
```

Examples:

```dq
use dqgui/widgets/button;

btn = @button.TButton("OK");
```

```dq
use dqgui/widgets/button as btnmod;

btn = @btnmod.TButton("OK");
```

The canonical module path is used internally by the compiler. The namespace name is local to the source file.

---

## 7. Namespace Uniqueness

Namespace names must be unique within the current source file.

This is an error:

```dq
use dqgui/widgets/button;
use mygui/widgets/button;
```

Both declarations try to create namespace `@button`.

The fix is to use explicit aliases:

```dq
use dqgui/widgets/button as dqbutton;
use mygui/widgets/button as mybutton;

b1 = @dqbutton.TButton();
b2 = @mybutton.TButton();
```

The uniqueness rule also applies to `nomerge` imports:

```dq
use dqgui/widgets/button nomerge;
use mygui/widgets/button nomerge;  // error: namespace `button` already exists
```

`nomerge` disables symbol merging, not namespace creation.

---

## 8. Basic `use`

The `use` declaration imports another module.

Default behavior:

```dq
use dqgui/widgets;
```

Effects:

```text
1. Resolves canonical module dqgui/widgets.
2. Creates local namespace @widgets.
3. Merges all public symbols of dqgui/widgets into the current namespace.
```

Example:

```dq
use dqgui/widgets;

btn = TButton("OK");
btn2 = @widgets.TButton("Cancel");
```

For a leaf module:

```dq
use dqgui/widgets/button;

btn = TButton("OK");
btn2 = @button.TButton("Cancel");
```

`use` may import a comma-separated list of independent module-use items:

```dq
use dqgui/widgets/button, dqgui/widgets/list, dqgui/widgets/edit;
```

This is exactly equivalent to:

```dq
use dqgui/widgets/button;
use dqgui/widgets/list;
use dqgui/widgets/edit;
```

Each item creates its own namespace, using the same rules as a separate `use`
declaration:

```dq
@button
@list
@edit
```

Each item may also have its own alias and modifiers:

```dq
use dqgui/widgets/button as btn, dqgui/widgets/list only(TList), dqgui/widgets/edit nomerge;
```

This is exactly equivalent to:

```dq
use dqgui/widgets/button as btn;
use dqgui/widgets/list only(TList);
use dqgui/widgets/edit nomerge;
```

---

## 9. Module Namespace Aliases

A module can be given a local namespace alias:

```dq
use dqgui/widgets as w;
```

Effects:

```text
1. Resolves canonical module dqgui/widgets.
2. Creates local namespace @w.
3. Merges all public symbols of dqgui/widgets into the current namespace.
```

Example:

```dq
use dqgui/widgets as w;

btn = TButton("OK");
btn2 = @w.TButton("Cancel");
```

The original namespace spelling is not automatically introduced by this declaration:

```dq
use dqgui/widgets as w;

@w.TButton("OK");       // OK
@widgets.TButton("OK"); // error, unless another use created @widgets
```

A `use ... as ...` alias is a namespace alias only. It is not a module path shortcut.

This is invalid unless a separate `usepath` defines `w` as a path alias:

```dq
use dqgui/widgets as w;
use w/button;  // error: `w` is not a path alias
```

Use `usepath` for path shortcuts.

---

## 10. `nomerge`

The `nomerge` modifier imports only the module namespace and does not merge public symbols into the current namespace.

```dq
use dqgui/widgets nomerge;
```

Effects:

```text
1. Resolves canonical module dqgui/widgets.
2. Creates local namespace @widgets.
3. Does not merge public symbols.
```

Example:

```dq
use dqgui/widgets nomerge;

btn = @widgets.TButton("OK");  // OK
btn = TButton("OK");           // error
```

With alias:

```dq
use dqgui/widgets as w nomerge;

btn = @w.TButton("OK");        // OK
btn = TButton("OK");           // error
```

---

## 11. `only(...)`

The `only(...)` modifier merges only selected public symbols into the current namespace.

The module namespace remains available.

```dq
use dqgui/widgets only(TButton, TLabel);
```

Effects:

```text
1. Resolves canonical module dqgui/widgets.
2. Creates local namespace @widgets.
3. Merges only TButton and TLabel into the current namespace.
```

Example:

```dq
use dqgui/widgets only(TButton, TLabel);

btn = TButton("OK");       // OK
lbl = TLabel("Name:");     // OK
win = TWindow();           // error
win = @widgets.TWindow();  // OK
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

## 12. `reexport`

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
use ./widget reexport, ./window reexport, ./button reexport, ./label reexport;
```

Then user code can write:

```dq
use dqgui/widgets;

btn = TButton("OK");
win = TWindow();
```

and qualified access is through the namespace created by the user's `use`:

```dq
btn = @widgets.TButton("OK");
```

The local child namespaces used inside the facade module, such as `@button` and `@label`, are not automatically exposed to users of the facade module.

Selective re-export:

```dq
use ./button only(TButton) reexport;
```

Effects:

```text
1. Only TButton is merged.
2. Only TButton is re-exported.
3. The local namespace @button is still available inside the current source file.
```

Invalid combination:

```dq
use ./button nomerge reexport;
```

Reason: `nomerge` says symbols are not inserted into the current namespace, while `reexport` says imported symbols become part of the current module's public namespace.

A modifier belongs only to the module-use item immediately before it.

For `reexport`, write it on every item that should be re-exported:

```dq
use ./button reexport, ./list reexport, ./edit reexport;
```

This declaration:

```dq
use ./button, ./list, ./edit reexport;
```

is valid, but means:

```dq
use ./button;
use ./list;
use ./edit reexport;
```

Style guide: do not rely on a trailing modifier to visually suggest that it
applies to the whole list. Repeat `reexport` for each re-exported item.

---

## 13. `usepath`

`usepath` defines a local module path shortcut.

It does not import a module.

It does not create a namespace.

It does not require the target path itself to be an existing module file.

Example:

```dq
usepath dqgui/widgets as w;

use w/button, w/textedit, w/list;
```

This is equivalent to:

```dq
use dqgui/widgets/button;
use dqgui/widgets/textedit;
use dqgui/widgets/list;
```

The namespaces created are:

```dq
@button
@textedit
@list
```

No namespace `@w` is created.

Therefore:

```dq
@w.TButton();       // error
@button.TButton();  // OK, if `use w/button;` was used
```

Path aliases are expanded independently for each comma-separated `use` item:

```dq
usepath dqgui/widgets as w;

use w/button as btn, w/list as lst, w/edit;
```

This is equivalent to:

```dq
use dqgui/widgets/button as btn;
use dqgui/widgets/list as lst;
use dqgui/widgets/edit;
```

Qualified access uses the namespace created by each item:

```dq
b = @btn.TButton();
l = @lst.TList();
e = @edit.TEdit();
```

`usepath` aliases are usable only as the first component of later `use` or `usepath` module paths.

Example:

```dq
usepath dqgui/widgets as w;
usepath w/internal as wi;

use wi/platformbutton nomerge;
```

`usepath` aliases are local to the current source file.

`usepath` affects only declarations that appear after it.

A `usepath` alias may be redefined without warning:

```dq
usepath dqgui/widgets as w;
use w/button;       // dqgui/widgets/button

usepath mygui/widgets as w;
use w/button;       // mygui/widgets/button
```

The compiler expands path aliases before module resolution.

---

## 14. Relative Module Paths

Module paths are absolute by default:

```dq
use dqgui/widgets/button;
usepath dqgui/widgets as w;
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

Qualified access uses the local namespace created by `use`, not the canonical module path.

Example:

```dq
use ./button;

btn = TButton("OK");
btn2 = @button.TButton("Cancel");
```

With alias:

```dq
use ./button as btnmod nomerge;

btn = @btnmod.TButton("OK");
```

A relative path that escapes above the module root is an error.

`usepath` can also use relative paths:

```dq
usepath ./internal as i;
use i/platformbutton nomerge;
```

---

## 15. Facade Modules

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
use ./core reexport;
use ./widgets reexport;
use ./paint reexport;
```

Sub-facade:

```text
dqgui/widgets.dq
```

```dq
use ./button reexport;
use ./label reexport;
use ./textinput reexport;
use ./list reexport;
```

User code:

```dq
use dqgui/widgets;

btn = TButton("OK");
lbl = TLabel("Name:");
btn2 = @widgets.TButton("Cancel");
```

For repeated leaf imports without requiring a facade module:

```dq
usepath dqgui/widgets as w;

use w/button;
use w/textinput;
use w/list;
```

This does not require `dqgui/widgets.dq` to exist.

---

## 16. Conflict Rules

Merged symbols should not silently overwrite each other.

This is a warning if both modules export `TButton`:

```dq
use gui1/widgets as gw1;
use gui2/widgets as gw2;
```

Example diagnostic:

```text
WARN(UseNameConflict): symbol 'TButton' imported from both @gw1 and @gw2
```

The user should resolve ambiguity with aliases, `only(...)`, or `nomerge`:

```dq
use gui1/widgets as gw1 only(TWindow, TLabel);
use gui2/widgets as gw2 only(TButton);
```

or:

```dq
use gui1/widgets as gw1 nomerge;
use gui2/widgets as gw2;

btn = TButton("OK");
other = @gw1.TButton("Other");
```

`reexport` conflicts are also issue warning.

Example:

```dq
use ./widgets reexport;
use ./graphics reexport;
```

If both export `TItem`, the facade module should explicitly restrict or rename one side.

Namespace conflicts are errors:

```dq
use dqgui/widgets/button;
use mygui/widgets/button;  // error: namespace `button` already exists
```

Fix:

```dq
use dqgui/widgets/button as dqbutton;
use mygui/widgets/button as mybutton;
```

---

## 17. Circular Module References

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

## 18. Grammar Sketch

Informal grammar:

```text
module-file:
    interface-item*
    [ "implementation" implementation-item* ]

interface-item:
    use-declaration
  | usepath-declaration
  | public-declaration

implementation-item:
    use-declaration
  | usepath-declaration
  | private-declaration
  | implementation-definition

use-declaration:
    "use" module-use-item { "," module-use-item } ";"

module-use-item:
    module-path [ "as" identifier ] use-modifier*

usepath-declaration:
    "usepath" module-path "as" identifier ";"

use-modifier:
    "nomerge"
  | "only" "(" symbol-list ")"
  | "reexport"

module-path:
    absolute-module-path
  | relative-module-path
  | path-alias-module-path

absolute-module-path:
    identifier { "/" identifier }

path-alias-module-path:
    identifier "/" path-tail

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
- In a comma-separated `use` declaration, each `module-use-item` is equivalent to
  a separate `use` declaration.
- A `use` modifier belongs only to the `module-use-item` immediately before it.
- `use ... as alias` creates a local namespace alias.
- `usepath ... as alias` creates a local path alias only.
- A path alias may be used only as the first component of later `use` or `usepath` paths.
- Path aliases may be redefined without warning.
- Namespace names created by `use` must be unique in the current source file.
```

---

## 19. Summary of Supported Forms

```dq
use xmodule;
use xmodule as xm;

use xmodule nomerge;
use xmodule as xm nomerge;

use xmodule only(Symbol1, Symbol2);
use xmodule as xm only(Symbol1, Symbol2);

use xmodule reexport;
use xmodule only(Symbol1, Symbol2) reexport;

use xmodule/a, xmodule/b, xmodule/c;
use xmodule/a as a, xmodule/b only(SymbolB), xmodule/c nomerge;
use ./button reexport, ./list reexport, ./edit reexport;

use ./child;
use ../sibling;
use ../../other/module;

usepath package/submodule as sm;
use sm/child;
use sm/child as childmod;
```

Examples:

```dq
use dqgui/widgets;
use dqgui/widgets as w;
use dqgui/widgets nomerge;
use dqgui/widgets only(TButton, TLabel);
use dqgui/widgets only(TButton, TLabel) reexport;

use ./button reexport;
use ./button reexport, ./list reexport, ./edit reexport;
use ../core only(TEvent);
use ../../paint/image as img nomerge;

usepath dqgui/widgets as widgets_path;
use widgets_path/button, widgets_path/list, widgets_path/textinput as ti;
```

Qualified access examples:

```dq
use dqgui/widgets;
use dqgui/widgets/button as btnmod;
use ../../paint/image as img nomerge;

w = @widgets.TWindow();
b = @btnmod.TButton("OK");
i = @img.TImage("logo.png");
```
