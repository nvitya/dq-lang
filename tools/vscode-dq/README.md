# DQ for VSCode

This folder contains a VSCode extension for `.dq`, `.dqh`, and `.dqi` files. It starts `dq-comp --langserver` for DQ files; there is no custom debug adapter.

## What it covers

- DQ keywords such as `func`, `var`, `const`, `if`, `while`, `endfunc`
- Preprocessor directives such as `#if`, `#ifdef`, `#define`, `#include`, `#srcdep`, and `#{endif}`
- Attributes such as `[[external]]`
- Builtins such as `len`, `sizeof`, `round`, `ceil`, `floor`
- Core types seen in this repository such as `int`, `cchar`, `embstr`, `float32`, `float64`
- Strings, numbers, operators, pointers like `^int` and `p^`, namespace access like `@def.MAXVAL`
- Test directives such as `//?check(...)` and `//?error(...)`
- Snippets for common DQ constructs
- Pressing Enter after a new `if`, `for`, `while`, `func`, `object`, or `struct` header ending in `:` indents the body and adds its matching `end...` closer; it does not add one when an indented body is already nearby
- A `DQ: Run Current File` command and editor play button powered by `dq-run`
- A `$dq` problem matcher for compiler output like `file.dq(3,11) ERROR(TypeSpecExpected): expected type specifier`
- Compiler diagnostics and top-level document symbols supplied by the DQ language server

Rainbow bracket coloring is limited to `(` and `)`. Square brackets still match and autoclose, but they do not affect rainbow nesting.

## Language server

The extension detects a folder's project automatically when it contains one
`.dqproj` file. With several projects, click the **DQ** project name in the
status bar or run **DQ: Select Active Project**. The picker lists projects
across the workspace, so it also works while editing a library.

Library folders without their own project follow the application you last
edited. Selecting an application source file makes that folder's project active;
opening a library keeps the last application context. The choice is remembered
for this workspace. Files opened outside the workspace follow the same rule.
The status bar shows which project libraries are using.

Folders with their own project keep their settings. Files using the same project
share a language server, including application and library buffers, so unsaved
library edits are visible when checking the application. Compiler path, extra
arguments, package search paths, target and defines come from the application.
There is no need to add a project mapping for each library folder. Remove any
old library-folder mapping if you want that folder to follow the active project.

The compiler resolves symlinks when comparing source identities: a library
opened through a workspace symlink and through `~/.dq/packages` is the same
source. Unsaved overlays, diagnostics, symbols and navigation share that identity
while editor URIs and logical package roots are preserved. If both aliases have
open buffers, the latest received edit supplies the shared source text. This
requires the updated compiler as well as the extension.

By default the extension resolves `dq-comp` from `PATH`. To use a local compiler:

```json
{
  "dq.languageServerPath": "/path/to/dq-lang/build/dq-comp"
}
```

Use `dq.languageServerArgs` for options such as `--target`, `--pkg_path`, or
`--pkg=board:../boards/adcif`. Project `pkg_path` and `pkg` declarations are
also used by the language server, including navigation and unsaved mapped files.
Relative argument paths use the selected project's parent directory.
To select projects explicitly, existing settings remain supported:

```jsonc
{
  "folders": [
    { "name": "blinkled_linux", "path": "blinkled_linux" },
    { "name": "uart", "path": "uart" },
    { "name": "vihal", "path": "vihal" }
  ],
  "settings": {
    "dq.languageServerProjects": {
      "blinkled_linux": "blinkled_luckfox_lyra_ultra.dqproj",
      "uart": "uart_nucleo_f746.dqproj"
    }
  }
}
```

Here `vihal` follows whichever application you last edited. A map entry overrides
`dq.languageServerProject`, the folder-local fallback. Relative project paths
use their workspace folder. **DQ: Select Active Project** updates only the
selected application's entry and restarts clients whose context changed.

Run extension tests with `npm test`. Test compiler symlink handling with
`python3 tools/test-langserver.py --compiler build/dq-comp` from the repository root.

## Running the current file

With a DQ file open, click the play button in the editor title or run `DQ: Run Current File` from the Command Palette. The extension saves the file and runs:

```bash
dq-run -g -O0 /absolute/path/to/current-file.dq
```

The process runs with the file's directory as its working directory. `dq-run` must be available on `PATH`, or set `dq.runPath` to its executable path in VSCode settings, for example:

```json
{
  "dq.runPath": "/path/to/dq-lang/build/dq-run"
}
```

## Build task

The extension contributes the `$dq` problem matcher. A sample task is available in `examples/tasks.json`:

```json
{
  "label": "DQ: build current file",
  "type": "shell",
  "command": "dq-comp",
  "args": [
    "-g",
    "${file}",
    "-o",
    "${fileDirname}/${fileBasenameNoExtension}"
  ],
  "group": "build",
  "problemMatcher": "$dq"
}
```

The task uses `dq-comp`, not `dqcc`. The `-g` flag is included so the same build output can be used by GDB debugging.

## Debugging

There is no DQ debugger in this extension. Use the Microsoft C/C++ `cppdbg` debugger with GDB. A sample launch configuration is available in `examples/launch.json`:

```json
{
  "name": "DQ: Debug current program",
  "type": "cppdbg",
  "request": "launch",
  "program": "${fileDirname}/${fileBasenameNoExtension}",
  "args": [],
  "stopAtEntry": false,
  "cwd": "${fileDirname}",
  "environment": [],
  "externalConsole": false,
  "MIMode": "gdb",
  "preLaunchTask": "DQ: build current file"
}
```

This assumes `dq-comp -g` emits usable DWARF source information and GDB can resolve the `.dq` source paths.

## Quick local use

1. Open VSCode.
2. Run `Developer: Reload Window` after changing the grammar.
3. For extension development mode, launch VSCode with:

```bash
code --extensionDevelopmentPath /lindata/workvc/dq-comp/tools/vscode-dq
```

This opens a new Extension Development Host window where `.dq`, `.dqh`, and `.dqi` files should use the `dq` language.

## Packaging

```bash
cd tools/vscode-dq
npm run package
```

## Installing

1. Open Command Palette: Ctrl+Shift+P
2. Run: Extensions: Install from VSIX...
3. Select the .vsix file
