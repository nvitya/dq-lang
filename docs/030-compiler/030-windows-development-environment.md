# Windows Development Environment

This chapter sets up Windows for developing DQ applications with the Windows
UCRT compiler package. The compiler produces native `.exe` files. Its archive
includes the compiler, standard packages, and the toolchain used to link
programs.

The commands marked `bat` below are for **Command Prompt** (`cmd.exe`). The
`sh` commands in the external-library section run inside **MSYS2 UCRT64**.

## Download and Extract DQ

Download the **x86_64 Windows UCRT** archive from the
[DQ releases page](https://github.com/nvitya/dq-lang/releases). Its filename is
`dq-VERSION-x86_64-windows-ucrt.zip`, where `VERSION` is the release number.

Extract the complete archive to a directory such as `C:\dq`. In this guide,
`C:\dq\bin\dq-comp.exe` is the resulting compiler path. If extraction creates
an additional directory containing the version number, use that directory as
your DQ root instead.

Use a simple directory name without spaces, and keep all the package
directories together. In particular, retain `bin`, `stdpkg`, `gcclibs`, and
`toolchain`. The compiler finds its bundled linker relative to its own
installation, so the toolchain directory does not need a separate `PATH` entry.

## Make the Tools Available

For the current Command Prompt window, run:

```bat
set "PATH=C:\dq\bin;%PATH%"
dq-comp --version
```

The command should print the compiler version. The package also includes
`dq-env.cmd`, which prepares the current Command Prompt when run from the
extracted package directory.

For permanent setup:

1. Open **Edit environment variables for your account** from the Start menu.
2. Under **User variables**, select **Path** and choose **Edit**.
3. Add `C:\dq\bin`, using your actual installation path if different.
4. Confirm the dialogs and open a new terminal.

Completely close and reopen VS Code if it was running, so it inherits the new
environment. `PATH` is the list of directories Windows searches for commands
and runtime DLLs.

Check which compiler Windows finds:

```bat
where dq-comp
dq-comp --version
```

If several versions are listed, Windows normally uses the first one.

## Compile and Run a Program

Follow [Getting Started](../010-project/030-getting-started.md) to create
`hello.dq`, then open a terminal in the directory containing that file:

```bat
dq-run hello.dq
```

To compile and run separately:

```bat
dq-comp -g -O0 hello.dq
.\hello.exe
```

The Windows compiler package provides the tools needed for this basic workflow.
External libraries are required only when your application uses them.

## Set Up Visual Studio Code

Install [Visual Studio Code](https://code.visualstudio.com/download), then
install the DQ extension from the compiler archive:

1. Open the Command Palette with `Ctrl+Shift+P`.
2. Run **Extensions: Install from VSIX...**.
3. Select `C:\dq\VSCode\dq-syntax-*.vsix`, using the actual filename and path.
4. Reload the VS Code window if prompted.

Open `C:\dq\examples` with **File > Open Folder**, then open `langdemo.dq`.
Click the play button in the editor title bar or run **DQ: Run Current File**
from the Command Palette. The extension saves the file and invokes `dq-run`
with `-g -O0`.

The extension uses `dq-comp` as its language server. If you keep a portable
installation outside `PATH`, set the paths explicitly in VS Code's
`settings.json`:

```json
{
    "dq.languageServerPath": "C:\\dq\\bin\\dq-comp.exe",
    "dq.runPath": "C:\\dq\\bin\\dq-run.exe"
}
```

Running a program with the play button does not start a debugger. For
breakpoints and stepping, the Windows setup uses the **LLDB DAP** extension
(`llvm-vs-code-extensions.lldb-dap`) with the bundled `lldb-dap.exe`. Its
executable path can be configured in `settings.json`:

```json
{
    "lldb-dap.executable-path": "C:\\dq\\toolchain\\llvm-mingw\\bin\\lldb-dap.exe"
}
```

Build debug executables with `-g -O0`. The
[VSCode Setup and Debugging](080-vscode-setup-and-debugging.md) chapter is the
place for build-task and debugger launch configurations.

## Add External Libraries with MSYS2

Packages such as the SQLite and MySQL bindings need external C libraries. A
convenient source of Windows builds is [MSYS2](https://www.msys2.org/).

Install MSYS2, using `C:\msys64` for the paths below. Open **MSYS2 UCRT64** from
the Start menu and update the installation:

```sh
pacman -Syu
```

If the update asks you to close the terminal, reopen **MSYS2 UCRT64** and run
the command again to finish. Follow the
[MSYS2 update instructions](https://www.msys2.org/docs/updating/) for further
details.

Install whichever client libraries your application needs:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-sqlite3
pacman -S --needed mingw-w64-ucrt-x86_64-libmariadbclient
```

The [SQLite package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-sqlite3)
provides the SQLite runtime and development libraries. The
[MariaDB client package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-libmariadbclient)
provides libraries for the MySQL-compatible client interface. Installing a
client library does not install a database server.

Use the **UCRT64** environment and packages beginning with
`mingw-w64-ucrt-x86_64-` for this setup. Its libraries live under `ucrt64`.
MSYS2's other environments use different toolchain or runtime combinations;
see the [MSYS2 environment overview](https://www.msys2.org/docs/environments/).

### Configure Library and DLL Search Paths

DQ needs to find the libraries during linking and their DLLs when the program
runs. Add these directories to your user environment:

| Variable | Directory | Purpose |
| --- | --- | --- |
| `PATH` | `C:\dq\bin` | DQ tools and their runtime DLLs. |
| `PATH` | `C:\msys64\ucrt64\bin` | External libraries' runtime DLLs. |
| `LIBRARY_PATH` | `C:\msys64\ucrt64\lib` | External libraries used by the linker. |

Use the same Windows environment-variable editor as for the DQ tools. Create
the user variable `LIBRARY_PATH` if it does not exist. If it already exists,
add the new directory to its semicolon-separated list while preserving the
existing directories. Keep the DQ `bin` entry ahead of the MSYS2 entry in `PATH`.

To configure the current Command Prompt temporarily:

```bat
set "PATH=C:\dq\bin;C:\msys64\ucrt64\bin;%PATH%"
set "LIBRARY_PATH=C:\msys64\ucrt64\lib;%LIBRARY_PATH%"
```

Open a new terminal and restart VS Code after changing persistent settings.
You can then run the SQLite example from the extracted DQ package directory:

```bat
cd /d C:\dq
dq-run stdpkg\sqlite3\examples\sqlite_test.dq
```

## Run Applications on Another Computer

When distributing a compiled program, include the runtime DLLs it needs.
For the standard Windows runtime, these normally include `libc++.dll` and
`libunwind.dll` from the DQ package's `bin` directory. Include any additional
DLLs required by the particular build, such as `libwinpthread-1.dll` when used.

Applications that use external libraries also need those libraries' runtime
DLLs and their dependencies. For example, SQLite's DLL is
`C:\msys64\ucrt64\bin\libsqlite3-0.dll`.

Place the required DLLs beside the application executable, preserving their
filenames. You can inspect direct DLL imports with the bundled tool:

```bat
"C:\dq\toolchain\llvm-mingw\bin\llvm-readobj.exe" --coff-imports hello.exe
```

Static linking is available through `dq-comp --static`, when the required
static libraries are available. It can reduce external DLL dependencies, but
does not guarantee a completely standalone executable. Include the applicable
license notices for libraries you distribute.

## Troubleshooting

### Windows Cannot Find `dq-comp`

Run `where dq-comp`. Check that your DQ `bin` directory is in `PATH` and that
you opened a new terminal after editing the environment. If running from VS
Code, restart the entire editor rather than only its integrated terminal.

### The Linker Cannot Find SQLite

For an error such as `unable to find library -lsqlite3`, check:

```bat
echo %LIBRARY_PATH%
dir C:\msys64\ucrt64\lib\libsqlite3.dll.a
```

Install the UCRT64 SQLite package if the import library is missing. Adding
`ucrt64\bin` to `PATH` supplies DLLs for execution; `LIBRARY_PATH` supplies
libraries for linking.

### A Runtime DLL Is Missing

Check `where libsqlite3-0.dll` for SQLite and your DQ `bin` directory for
`libc++.dll` and `libunwind.dll`. Verify the `PATH` inherited by the program or
place the required DLLs beside its executable.

### A Library Has the Wrong Architecture or Format

Use native x86_64 Windows UCRT libraries with this compiler package. Linux/WSL
libraries and MSYS libraries do not replace those native Windows libraries.
Check that both the selected package and the search paths use `ucrt64`.
