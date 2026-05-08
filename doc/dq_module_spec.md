# DQ Module System Specification

Status: draft
Scope: packages, module roots, module naming, file mapping, `use`, `usepath`, public interface handling, re-exporting, namespaces, aliases, relative module paths, package-root module paths, and linker-visible canonical identity.

---

## 1. Packages and Module Roots

A DQ package is a named module tree.

A package has:

```text
module root directory
package name
package-local module paths
canonical module ids
```

The package name is normally derived from the final directory name of the module root directory.

Example:

```text
module root directory:  /src/dqgui
package name:           dqgui
source file:            /src/dqgui/os/linux/gtk/core.dq
package-local path:     os/linux/gtk/core
canonical module id:    dqgui/os/linux/gtk/core
```

The canonical module id is:

```text
package-name "/" package-local-module-path
```

All absolute module paths are package-qualified. The first component of an absolute module path is a package or application name.

Examples:

```dq
use dqgui/widgets/button;
use dqstd/io;
use sqlite/core nomerge;
```

The first components are package names:

```text
dqgui
dqstd
sqlite
```

A package and an application use the same module identity rules. An application is simply a package that produces an executable.

There is no required package manifest for simple builds. The module root may be determined from the source file location by `#opt module_root_depth`.

---

## 2. `#opt module_root_depth`

The source option `module_root_depth` defines how far the current source file directory is below the module root directory.

Syntax:

```dq
#opt module_root_depth = 3
```

Meaning:

```text
The directory containing this source file is 3 directory levels below the module root directory.
```

The value must be a non-negative integer constant.

The default is:

```dq
#opt module_root_depth = 0
```

A value of `0` means that the directory containing the source file is the module root directory.

Example source file:

```text
/src/dqgui/os/linux/gtk/core.dq
```

With:

```dq
#opt module_root_depth = 3
```

the compiler computes:

```text
source file directory:  /src/dqgui/os/linux/gtk
up 1:                  /src/dqgui/os/linux
up 2:                  /src/dqgui/os
up 3:                  /src/dqgui

module root directory: /src/dqgui
package name:          dqgui
package-local path:    os/linux/gtk/core
canonical module id:   dqgui/os/linux/gtk/core
```

Another example:

```text
/src/dqgui/system/utils.dq
```

With:

```dq
#opt module_root_depth = 1
```

this becomes:

```text
module root directory: /src/dqgui
package name:          dqgui
package-local path:    system/utils
canonical module id:   dqgui/system/utils
```

The option affects module path canonicalization and must be known before resolving `use` and `usepath` declarations in the file.

If a source file is loaded through an already-known package root, and the file also specifies `#opt module_root_depth`, the computed root must match the already-known root. A mismatch is an error.

Example error:

```text
ERROR(ModuleRootMismatch): source file declares module root '/src/dqgui/os',
  but it was imported as part of package root '/src/dqgui'
```

The `#opt module_root_depth` form is the only source-level syntax for this option. There is no `module ...` declaration form for setting it.

---

## 3. Source File = Module

Every DQ source file defines exactly one module.

The package-local module path is derived from the file path relative to the module root directory.

```text
/src/dqgui/core.dq                    -> dqgui/core
/src/dqgui/widgets.dq                 -> dqgui/widgets
/src/dqgui/widgets/button.dq          -> dqgui/widgets/button
/src/dqgui/paint/image.dq             -> dqgui/paint/image
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
canonical module id:  package/a/b/c
source file:          <module-root>/a/b/c.dq
```

A module may have child modules. Therefore this layout is valid:

```text
dqgui/widgets.dq
dqgui/widgets/button.dq
dqgui/widgets/label.dq
```

Here `dqgui/widgets.dq` defines canonical module `dqgui/widgets`, while `dqgui/widgets/button.dq` defines child module `dqgui/widgets/button`.

Directories alone do not define modules. If `dqgui/widgets.dq` does not exist, then `use dqgui/widgets;` is an error, even if directory `dqgui/widgets/` exists.

---

## 4. Implicit Interface Section

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

## 5. Implementation Section

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

## 6. Statement Terminators

Active statements require `;`.

Examples:

```dq
use dqgui/widgets;
use ./button reexport;
use ^/system/utils nomerge;
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

Preprocessor/source options beginning with `#`, such as `#opt module_root_depth = 3`, do not require `;`.

---

## 7. Module Paths vs Namespaces

DQ separates module paths from namespace names.

```text
module path  = used by `use` and `usepath` to resolve source files
namespace    = local name created by `use` for qualified access
```

Module paths may contain `/`:

```dq
use dqgui/widgets/button;
use ^/system/utils;
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
@^/system/utils.Version
```

Qualified lookup has the form:

```text
@namespace.Symbol
@namespace.Symbol.member
@namespace.Symbol.method()
```

Only a local namespace identifier may appear after `@`.

---

## 8. Local Namespace Creation

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

```dq
use ^/system/utils;
```

creates namespace:

```dq
@utils
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

The canonical module id is used internally by the compiler. The namespace name is local to the source file.

---

## 9. Namespace Uniqueness

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

## 10. Basic `use`

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

For a package-root-relative module:

```dq
use ^/system/utils;

ver = @utils.version;
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

Each item creates its own namespace, using the same rules as a separate `use` declaration:

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

## 11. Module Namespace Aliases

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

## 12. `nomerge`

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

## 13. `only(...)`

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

## 14. `exclude(...)`

The `exclude(...)` modifier merges all public symbols into the current namespace except the selected symbols.

The module namespace remains available.

```dq
use dqgui/widgets exclude(TList);
```

Effects:

```text
1. Resolves canonical module dqgui/widgets.
2. Creates local namespace @widgets.
3. Merges all public symbols except TList into the current namespace.
```

Example:

```dq
use dqgui/widgets exclude(TList);

btn = TButton("OK");     // OK
lst = TList();           // error
lst = @widgets.TList();  // OK
```

With alias:

```dq
use dqgui/widgets as w exclude(TList);

btn = TButton("OK");
lst = @w.TList();
```

`exclude(...)` is useful when a module's public interface should mostly be merged, but one or more names would conflict with local code or another import.

---

## 15. `reexport`

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
use ./widgets exclude(TList) reexport;
```

Effects:

```text
1. The imported symbols selected by `only(...)`, or left after `exclude(...)`, are merged.
2. Those merged symbols are re-exported.
3. The local namespace, such as @button or @widgets, is still available inside the current source file.
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

Style guide: do not rely on a trailing modifier to visually suggest that it applies to the whole list. Repeat `reexport` for each re-exported item.

---

## 16. `usepath`

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

`usepath` can also use package-root-relative paths:

```dq
usepath ^/widgets as w;
use w/button, w/list, w/edit;
```

The compiler expands path aliases before module resolution.

---

## 17. Package-Root and Relative Module Paths

Module paths have four source-level forms:

```text
absolute package path:      dqgui/system/utils
package-root-relative path: ^/system/utils
relative path:              ./local
relative parent path:       ../sibling
path alias path:            w/button
```

Absolute paths are package-qualified:

```dq
use dqgui/widgets/button;
usepath dqgui/widgets as w;
```

The first component is the package name.

A path starting with `^/` is relative to the current module root directory.

Example package layout:

```text
dqgui/os/linux/gtk/core.dq
dqgui/system/utils.dq
```

In `dqgui/os/linux/gtk/core.dq`:

```dq
#opt module_root_depth = 3

use ^/system/utils;
```

resolves to:

```text
dqgui/system/utils
```

A path starting with `./` or `../` is relative to the current module directory inside the same package.

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

Relative paths are source-level shortcuts only. The compiler canonicalizes them to package-qualified canonical module ids before semantic analysis.

Qualified access uses the local namespace created by `use`, not the canonical module id.

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

`usepath` can also use relative and package-root-relative paths:

```dq
usepath ./internal as i;
use i/platformbutton nomerge;

usepath ^/widgets as w;
use w/button;
```

---

## 18. Above-Root Path Errors

A relative module path must not escape above the current module root directory.

During canonicalization, every `..` component removes one component from the current package-local module directory. If there is no component left to remove, the path is invalid.

This is a compile-time error before module lookup.

Example:

```text
current source file:     dqgui/os/linux/gtk/core.dq
current module dir:      os/linux/gtk
module root directory:   dqgui
```

Valid:

```dq
use ../../../system/utils;
```

Resolution:

```text
os/linux/gtk
-> os/linux
-> os
-> .
+ system/utils

= dqgui/system/utils
```

Invalid:

```dq
use ../../../../../utils;
```

Resolution attempt:

```text
os/linux/gtk
-> os/linux
-> os
-> .
-> ERROR: above module root directory
```

Example diagnostic:

```text
ERROR(ModulePathAboveRoot): relative module path '../../../../../utils'
  escapes above module root 'dqgui'
```

The same rule applies after `usepath` expansion.

Invalid:

```dq
usepath ../../../../.. as bad;
```

Invalid:

```dq
usepath ../../../system as s;
use s/../../utils;  // escapes above root after expansion
```

A package-root-relative path also must not contain components that escape above the root.

Invalid:

```dq
use ^/../utils;
```

Path normalization uses a stack rule:

```text
for each path component:
  "."  -> ignore
  ".." -> pop one component; if stack empty, error
  name -> push name
```

For `^/`, the initial stack is empty.

For `./` and `../`, the initial stack is the current package-local module directory.

---

## 19. Facade Modules

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

Package-internal facade modules may use `^/` when a stable root-relative reference is clearer than a long relative path:

```dq
use ^/widgets/button reexport;
use ^/widgets/label reexport;
```

---

## 20. Linker Symbol Identity

All linker-visible symbols across all packages and modules must be unique.

The linker symbol identity is based on the canonical module id, not on the local namespace name and not on the spelling used in a `use` declaration.

Semantic identity:

```text
package-name / package-local-module-path . symbol-name
```

Example:

```text
package:             dqgui
module path:         system/utils
symbol:              version
semantic identity:   dqgui/system/utils.version
```

Possible readable linker symbol:

```text
dq$dqgui$system$utils$version
```

If overloads require signature encoding, the overload/signature suffix is added after the semantic symbol identity.

Example:

```text
dq$dqgui$system$utils$Open__str_i32
```

The linker symbol must never be based on:

```text
@utils
@button
local alias
relative import spelling
```

These are source-local conveniences only.

---

## 21. Conflict Rules

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

The user should resolve ambiguity with aliases, `only(...)`, `exclude(...)`, or `nomerge`:

```dq
use gui1/widgets as gw1 only(TWindow, TLabel);
use gui2/widgets as gw2 only(TButton);
```

or:

```dq
use gui1/widgets as gw1 exclude(TButton);
use gui2/widgets as gw2;
```

or:

```dq
use gui1/widgets as gw1 nomerge;
use gui2/widgets as gw2;

btn = TButton("OK");
other = @gw1.TButton("Other");
```

`reexport` conflicts also issue warnings.

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

## 22. Circular Module References

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

## 23. Grammar Sketch

Informal grammar:

```text
module-file:
    opt-declaration*
    interface-item*
    [ "implementation" implementation-item* ]

opt-declaration:
    "#opt" opt-name "=" const-expression

opt-name:
    "module_root_depth"

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
  | "exclude" "(" symbol-list ")"
  | "reexport"

module-path:
    absolute-package-module-path
  | package-root-relative-module-path
  | relative-module-path
  | path-alias-module-path

absolute-package-module-path:
    identifier { "/" identifier }

package-root-relative-module-path:
    "^" "/" path-tail

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
- `#opt module_root_depth = N` accepts only a non-negative integer constant.
- `#opt module_root_depth` must be known before resolving `use` and `usepath` declarations.
- There is no `module ...` declaration form for setting `module_root_depth`.
- Absolute module paths are package-qualified; the first component is a package/application name.
- `^/path` resolves relative to the current module root directory.
- `./path` and `../path` resolve relative to the current module directory.
- Relative paths and package-root-relative paths must not escape above the module root directory.
- The compiler canonicalizes all module paths before semantic analysis.
- `nomerge`, `only(...)`, and `exclude(...)` are mutually exclusive.
- `nomerge` and `reexport` are mutually exclusive.
- `only(...) reexport` is valid.
- `exclude(...) reexport` is valid.
- In a comma-separated `use` declaration, each `module-use-item` is equivalent to a separate `use` declaration.
- A `use` modifier belongs only to the `module-use-item` immediately before it.
- `use ... as alias` creates a local namespace alias.
- `usepath ... as alias` creates a local path alias only.
- A path alias may be used only as the first component of later `use` or `usepath` paths.
- Path aliases may be redefined without warning.
- Namespace names created by `use` must be unique in the current source file.
```

---

## 24. Summary of Supported Forms

```dq
#opt module_root_depth = 0
#opt module_root_depth = 3

use xpackage/xmodule;
use xpackage/xmodule as xm;

use xpackage/xmodule nomerge;
use xpackage/xmodule as xm nomerge;

use xpackage/xmodule only(Symbol1, Symbol2);
use xpackage/xmodule as xm only(Symbol1, Symbol2);
use xpackage/xmodule exclude(Symbol1, Symbol2);
use xpackage/xmodule as xm exclude(Symbol1, Symbol2);

use xpackage/xmodule reexport;
use xpackage/xmodule only(Symbol1, Symbol2) reexport;
use xpackage/xmodule exclude(Symbol1, Symbol2) reexport;

use xpackage/a, xpackage/b, xpackage/c;
use xpackage/a as a, xpackage/b only(SymbolB), xpackage/c nomerge;
use ./button reexport, ./list reexport, ./edit reexport;

use ^/root_child;
use ^/system/utils;
use ^/system/utils as utils nomerge;

use ./child;
use ../sibling;
use ../../other/module;

usepath package/submodule as sm;
use sm/child;
use sm/child as childmod;

usepath ^/widgets as w;
use w/button, w/list, w/textinput as ti;
```

Examples:

```dq
#opt module_root_depth = 3

use dqgui/widgets;
use dqgui/widgets as w;
use dqgui/widgets nomerge;
use dqgui/widgets only(TButton, TLabel);
use dqgui/widgets exclude(TList);
use dqgui/widgets only(TButton, TLabel) reexport;
use dqgui/widgets exclude(TList) reexport;

use ^/system/utils nomerge;
use ^/widgets/button reexport;

use ./button reexport;
use ./button reexport, ./list reexport, ./edit reexport;
use ../core only(TEvent);
use ../../paint/image as img nomerge;

usepath dqgui/widgets as widgets_path;
use widgets_path/button, widgets_path/list, widgets_path/textinput as ti;

usepath ^/widgets as local_widgets;
use local_widgets/button, local_widgets/list;
```

Qualified access examples:

```dq
use dqgui/widgets;
use dqgui/widgets/button as btnmod;
use ^/system/utils nomerge;
use ../../paint/image as img nomerge;

w = @widgets.TWindow();
b = @btnmod.TButton("OK");
v = @utils.version;
i = @img.TImage("logo.png");
```
