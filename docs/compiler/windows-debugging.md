# Debugging DQ programs in VS Code on Windows

DQ programs can be source-debugged with the `lldb-dap` debugger included in
the Windows DQ package. The compiler must generate debug information, so the
debug build uses `-g -O0`.

## 1. Install the extensions

Install **LLDB DAP** (`llvm-vs-code-extensions.lldb-dap`) from the VS Code
Marketplace and reload VS Code.

For DQ syntax support and the `$dq` build problem matcher, also install the
bundled `VSCode\dq-syntax-0.1.4.vsix` using **Extensions: Install from VSIX...**.

The Microsoft C/C++ extension may remain installed, but the configuration
below does not use its `cppdbg` adapter. On Windows, configuring `cppdbg` with
`lldb-mi.exe` can hang in `WindowsDebugLauncher.exe`.

Open the DQ project directory as the VS Code workspace. The project may be in a
different directory from the DQ installation.

## 2. Configure the target

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

## 3. Add the build task

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

## 4. Add the launch configuration

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

## 5. Start debugging

1. Open the `.dq` source file and set a breakpoint.
2. Open **Run and Debug** (`Ctrl+Shift+D`).
3. Select **DQ: Debug configured target (LLDB-DAP)**.
4. Press `F5`.

VS Code builds the selected target first, then starts it under LLDB. Command-line
arguments can be added to the `args` array in `launch.json`.

## Troubleshooting

- If VS Code does not recognize the `lldb-dap` debug type, verify that the
  **LLDB DAP** extension is installed and reload the window.
- If startup hangs while mentioning `WindowsDebugLauncher.exe` and
  `lldb-mi.exe`, VS Code is using a `cppdbg` configuration. Select the
  `lldb-dap` configuration above instead.
- If a breakpoint is not resolved, confirm the build task still contains both
  `-g` and `-O0`, then rebuild the target.
- If the executable or a runtime DLL cannot be found, check the target path and
  retain the `PATH` entry from the launch configuration.
- If the program reads relative files, set `dq.debugCwd` to the directory it
  normally runs from.
