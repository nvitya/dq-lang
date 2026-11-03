# Library Overview

DQ provides runtime services and source packages. The compiler builds imported
package modules for the selected target and stores the results in `.dqbuild`.
Installing a package does not require prebuilding it for every application.

## Runtime and Explicit Imports

The implicit [`sys`](015-sys.md) interface supplies basic runtime helpers.
Other packages are imported explicitly:

```dq
use print
use strutils

func *Main():
    var message : str = Format("{} items", [3])
    PrintLn(message)
endfunc
```

`print` writes to standard output. `strutils` provides `Format` and small
text helpers. The formatter and the string/array implementations themselves
belong to the runtime.

| Need | Documentation |
| --- | --- |
| Process arguments, memory blocks, runtime helpers | [`sys`](015-sys.md) |
| Console output | [`print`](020-print.md) |
| Formatting and text helpers | [`strutils`](030-strutils.md), [Text Formatting](../020-language/077-text-formatting.md) |
| Files, directories, paths | [`file`](040-file.md) |
| JSON trees | [`json`](050-json.md) |
| Native threads and channels | [`threads`](060-threads.md) |
| Event-driven sockets and HTTP | [`nanonet`](070-nanonet.md) |
| Math, time, parsing, databases, C bindings | [Additional Standard Packages](080-additional-standard-packages.md) |

## Package Discovery and Platform Dependencies

A bare import is resolved through the compiler's package search roots. Add
project packages with `--pkg-path` or `packagepath` in a `.dqproj` file. Later
roots take precedence. See [Modules and Packages](../020-language/170-modules-and-packages.md#package-search)
for identity and lookup rules.

Packages can depend on native libraries and platform APIs. For example,
`threads` has hosted Linux and Windows implementations; database bindings need
the corresponding client libraries. A package's presence in `stdpkg` does not
imply that it is available on a bare-metal or WebAssembly target.

The package chapters include the maintained package-local API manuals, so the
website, PDF book, and standalone package documentation use the same source.
