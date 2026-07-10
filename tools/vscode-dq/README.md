# DQ for VSCode

This folder contains a minimal VSCode extension for `.dq` files. It is intentionally declarative: there is no language server and no custom debug adapter.

## What it covers

- DQ keywords such as `function`, `var`, `const`, `if`, `while`, `endfunc`
- Preprocessor directives such as `#if`, `#ifdef`, `#define`, `#{include ...}`, `#{endif}`
- Attributes such as `[[external]]`
- Builtins such as `len`, `sizeof`, `round`, `ceil`, `floor`
- Core types seen in this repository such as `int`, `cchar`, `cstring`, `float32`, `float64`
- Strings, numbers, operators, pointers like `^int` and `p^`, namespace access like `@def.MAXVAL`
- Test directives such as `//?check(...)` and `//?error(...)`
- Snippets for common DQ constructs
- A `DQ: Run Current File` command and editor play button powered by `dq-run`
- A `$dq` problem matcher for compiler output like `file.dq(3,11) ERROR(TypeSpecExpected): expected type specifier`

Rainbow bracket coloring is limited to `(` and `)`. Square brackets still match and autoclose, but they do not affect rainbow nesting.

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

This opens a new Extension Development Host window where `.dq` files should use the `dq` language.

## Packaging

```bash
cd tools/vscode-dq
npm run package
```

## Installing

1. Open Command Palette: Ctrl+Shift+P
2. Run: Extensions: Install from VSIX...
3. Select the .vsix file
