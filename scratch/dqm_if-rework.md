# Standalone `.dqm_if` + `.o`, no `.dqm` anymore

The combined `.dqm` file was designed in order not to pollute the source code directory very much.
As the `.dqbuild` subdirectory was introduced, the combined `.dqm` object file with a `.dqm_if` section lost
its main reason to exist. The standalone `.dqm_if` is generated anyway during the compilation process, so it just
needs to be kept, and codegen should generate a pure `.o` file without the `.dqm_if` section.
This change should simplify and accelerate the compiling process.

The `.dqm` and `.dqm_if` files are temporary files created within `.dqbuild`; no migration is required.
Existing `.dqm` files should be ignored after this change.

# Separate DQ Header Files: .dqh

Special directive:
```
#include header
```
When called from the `file.dq` this directive includes `file.dqh`. This directive is invalid after `implementation` or in `.dqh` files.
No duplicate check is necessary: including the same declarations more than once will lead to a compiler error.

**Subtask**: The DQ syntax highlighting in `tools/vscode-dq` should be extended to `*.dqh` and also include `*.dqi` for code includes.

# Multiple Source Code Dependencies in the `.dqm_if`

With the introduction of `.dqh` files, the compiler needs to check additional files for changes, not only the original `.dq` file.
These are source code dependencies. If the `.dq` file contains further includes after the `implementation`, those are
not listed in the `.dqm_if`, because `.dqm_if` generation stops at the `implementation`.

The implementation body source code dependencies can be added into the header section with a specific directive:
```
#srcdep "somefunc_impl.dqi"
```

Paths in `#include` and `#srcdep` directives should be resolved in the same way as paths in a module `use`, except that a path without a leading dot should also be looked up relative to the directory of the current source file.

The DQ compiler should change the single `.dq` source file dependency to a list, adding the directly named `#include` files and the directly named manual `#srcdep` files before `implementation`. The `#srcdep` directive is invalid after the `implementation`.

Dependency tracking is not transitive. The `.dqm_if` and `.o` files must be regenerated when the timestamp or size of any of these files changes.
