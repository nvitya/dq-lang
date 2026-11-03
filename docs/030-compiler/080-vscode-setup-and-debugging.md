# VSCode Setup and Debugging

Install the DQ tools using the [Linux](020-linux-development-environment.md) or
[Windows](030-windows-development-environment.md) environment chapter first.
Confirm that `dq-comp --version` and `dq-run --help` work in the terminal from
which you start VS Code.

## DQ Extension

Install the bundled `VSCode/dq-syntax-*.vsix` with **Extensions: Install from
VSIX...** in the Command Palette, then reload VS Code. In a source checkout,
the extension files are under `tools/vscode-dq`.

The extension supplies syntax highlighting, compiler diagnostics, document
symbols, block completion, the `$dq` problem matcher, and **DQ: Run Current
File**. The run command uses `dq-run` with `-g -O0`. It does not start an
interactive debugger.

Both tools must be on the editor's `PATH`. Alternatively set
`dq.languageServerPath` and `dq.runPath` to their full paths in `settings.json`.
On Linux, starting VS Code with `code .` from a configured terminal lets it
inherit that terminal's environment. Restart the editor after changing persistent
environment variables.

## Debugging on Linux

Install GDB and the Microsoft C/C++ extension (`ms-vscode.cpptools`). Open the
program directory as a VS Code workspace. The following configuration builds
and debugs the active `.dq` file; keep that file active when starting the task.

Create `.vscode/tasks.json`:

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "DQ: build current file",
            "type": "shell",
            "command": "dq-comp",
            "args": [
                "-g", "-O0", "${file}",
                "-o", "${fileDirname}/${fileBasenameNoExtension}"
            ],
            "options": {"cwd": "${workspaceFolder}"},
            "group": "build",
            "problemMatcher": "$dq"
        }
    ]
}
```

Create `.vscode/launch.json`:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "DQ: Debug current program",
            "type": "cppdbg",
            "request": "launch",
            "program": "${fileDirname}/${fileBasenameNoExtension}",
            "args": [],
            "cwd": "${fileDirname}",
            "stopAtEntry": false,
            "externalConsole": false,
            "MIMode": "gdb",
            "preLaunchTask": "DQ: build current file"
        }
    ]
}
```

Set a breakpoint and press `F5`. Add program arguments to the launch
configuration's `args` array. For a `.dqproj` application, replace the active-file
build argument with the project path, and set `program` to the output executable.
The working directory controls relative file access at runtime.

You can also debug outside the editor:

```bash
dq-comp -g -O0 app.dq
gdb ./app
```

The repository contains optional GDB helpers in `tools/gdb/dq_gdbinit.py` and
`tools/gdb/dq_printers.py` for inspecting DQ values.

## Debugging on Windows

DQ programs can be source-debugged with the `lldb-dap` debugger included in
the Windows DQ package. The compiler must generate debug information, so the
debug build uses `-g -O0`.

### Install the extensions

Install **LLDB DAP** (`llvm-vs-code-extensions.lldb-dap`) from the VS Code
Marketplace and reload VS Code.

For DQ syntax support and the `$dq` build problem matcher, also install the
bundled `VSCode\dq-syntax-0.1.4.vsix` using **Extensions: Install from VSIX...**.

The Microsoft C/C++ extension may remain installed, but the configuration
below does not use its `cppdbg` adapter. On Windows, configuring `cppdbg` with
`lldb-mi.exe` can hang in `WindowsDebugLauncher.exe`.

Open the DQ project directory as the VS Code workspace. The project may be in a
different directory from the DQ installation.

### Configure the target

Add the DQ compiler's bin directory (e.g. `c:\\prg\\dq\\bin`) to the system path,
so the `dq-comp.exe` or `dq-run` exe are accessible from command line without full path specification.

Add these values to `.vscode/settings.json`:

```json
{
    "dq.debugTarget": "examples/langdemo",
    "dq.debugCwd": "examples",

    "lldb-dap.executable-path": "C:\\prg\\dq\\toolchain\\llvm-mingw\\bin\\lldb-dap.exe"
}
```
(`dq.debugTarget` has no `.dq` extension!)

For example, to debug the NanoNet echo client, use:

```json
{
    "dq.debugTarget": "stdpkg/nanonet/examples/binecho_client",
    "dq.debugCwd": "stdpkg/nanonet/examples"
}
```

### Add the build task

Create `.vscode/tasks.json`:

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "dq: build configured target",
            "type": "shell",
            "command": "dq-comp.exe",
            "args": [
                "-g",
                "-O0",
                "${config:dq.debugTarget}.dq",
                "-o",
                "${config:dq.debugTarget}.exe"
            ],
            "options": {
                "cwd": "${workspaceFolder}"
            },
            "group": "build",
            "problemMatcher": "$dq"
        }
    ]
}
```

### Add the launch configuration

Create `.vscode/launch.json`:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "DQ: Debug configured target (LLDB-DAP)",
            "type": "lldb-dap",
            "request": "launch",
            "program": "${workspaceFolder}\\${config:dq.debugTarget}.exe",
            "args": [],
            "cwd": "${workspaceFolder}\\${config:dq.debugCwd}",
            "stopOnEntry": false,
            "console": "internalConsole",
            "preLaunchTask": "dq: build configured target"
        }
    ]
}
```

### Start debugging

1. Open the `.dq` source file and set a breakpoint.
2. Open **Run and Debug** (`Ctrl+Shift+D`).
3. Select **DQ: Debug configured target (LLDB-DAP)**.
4. Press `F5`.

VS Code builds the selected target first, then starts it under LLDB. Command-line
arguments can be added to the `args` array in `launch.json`.

### Troubleshooting

- If VS Code does not recognize the `lldb-dap` debug type, verify that the
  **LLDB DAP** extension is installed and reload the window.
- If startup hangs while mentioning `WindowsDebugLauncher.exe` and
  `lldb-mi.exe`, VS Code is using a `cppdbg` configuration. Select the
  `lldb-dap` configuration above instead.
- If a breakpoint is not resolved, confirm the build task still contains both
  `-g` and `-O0`, then rebuild the target.
- If the executable or a runtime DLL cannot be found, check the target path and
  the DQ installation on `PATH`. Keep its toolchain directories together.
- If the program reads relative files, set `dq.debugCwd` to the directory it
  normally runs from.

## Diagnostics and Build Configuration

If diagnostics use the wrong compiler, check `dq.languageServerPath`. If the
run command cannot find `dq-run`, check `dq.runPath` and the editor environment.
The syntax extension and a debugger extension serve different purposes; install
both when using interactive breakpoints.

A breakpoint requires the executable built from the source currently open.
Use `-g -O0`, confirm the output and launch paths agree, and rebuild. Program
arguments and working directories belong to the launch configuration; package
roots and compiler flags belong to the build task or project file.
