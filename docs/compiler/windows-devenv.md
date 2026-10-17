# Windows Development Environment

This guide sets up an x86_64 Windows machine for developing DQ applications
with the Windows UCRT release package. DQ supplies the compiler and LLVM/MinGW
toolchain; MSYS2 supplies optional native libraries such as SQLite and the
MySQL-compatible MariaDB C client.

After setup, an application using SQLite can be built and run from a normal
Windows Command Prompt with:

```bat
dq-run thdatasvr.dqproj
```

See [Using the DQ Compiler](using-dq-comp.md) for compiler options,
[DQ Project Files](project-file.md) for project configuration, and
[Attributes, Directives, and Interoperability](../reference/attributes-directives-and-interop.md)
for the `#linklib` directive used by library bindings.

## 1. Install DQ

Download the **x86_64 Windows UCRT** archive from the
[DQ releases page](https://github.com/nvitya/dq-lang/releases), and extract the
complete archive. This guide uses `D:\PRG\dq` as the installation directory:

```text
D:\PRG\dq\
    bin\
        dq-comp.exe
        dq-run.exe
        libc++.dll
        libunwind.dll
    toolchain\llvm-mingw\
    stdpkg\
    examples\
```

Keep the package directories together. The compiler finds its bundled
toolchain relative to its installation; only `D:\PRG\dq\bin` needs to be on
`PATH`. Visual Studio Build Tools and a
separate GCC installation are not required to compile DQ applications with
this package.

For the current Command Prompt window, add the DQ tools:

```bat
set "PATH=D:\PRG\dq\bin;%PATH%"
dq-comp --version
```

Alternatively, run `D:\PRG\dq\dq-env.cmd` from Command Prompt to prepare the
DQ tools for that window. The release script sets up DQ itself; configure the
MSYS2 library directories separately as described below.

## 2. Install MSYS2 and SQLite

Install [MSYS2](https://www.msys2.org/) using its x86_64 installer. The examples
use the default directory, `C:\msys64`.

Open **MSYS2 UCRT64** from the Start menu. Update the installation:

```sh
pacman -Syu
```

If the update asks to close the terminal, accept, reopen **MSYS2 UCRT64**, and
run the update command again to finish. See the
[MSYS2 update guide](https://www.msys2.org/docs/updating/) for details.

Install SQLite in that MSYS2 terminal:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-sqlite3
```

Use packages beginning with `mingw-w64-ucrt-x86_64-` for this setup. They
provide native x86_64 Windows libraries using the Universal C Runtime.

The [SQLite package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-sqlite3)
includes:

| File | Purpose |
| --- | --- |
| `C:\msys64\ucrt64\bin\libsqlite3-0.dll` | SQLite runtime library |
| `C:\msys64\ucrt64\lib\libsqlite3.dll.a` | Import library used when linking to the DLL |
| `C:\msys64\ucrt64\lib\libsqlite3.a` | Static SQLite library |
| `C:\msys64\ucrt64\bin\sqlite3.exe` | Command-line database tool |

MSYS2 is used to install and update dependencies. Normal DQ development can
continue in Windows Command Prompt, PowerShell, or VS Code.

## 3. Configure the Windows environment

Two environment variables are needed when using the MSYS2 libraries:

| Variable | Directory | Purpose |
| --- | --- | --- |
| `PATH` | `D:\PRG\dq\bin` | Find DQ commands and DQ runtime DLLs |
| `PATH` | `C:\msys64\ucrt64\bin` | Find third-party runtime DLLs and tools |
| `LIBRARY_PATH` | `C:\msys64\ucrt64\lib` | Find third-party libraries during linking |

`PATH` and `LIBRARY_PATH` have different jobs. Adding the DLL directory to
`PATH` does not add the library directory to the linker search path. DQ's
bundled Clang driver honors `LIBRARY_PATH`; no additional DQ compiler option
or machine-specific project setting is needed.

### Configure the current Command Prompt

Run these commands in **Windows Command Prompt (`cmd.exe`)**:

```bat
set "PATH=D:\PRG\dq\bin;%PATH%;C:\msys64\ucrt64\bin"
set "LIBRARY_PATH=C:\msys64\ucrt64\lib"
```

The second command assumes a fresh setup. If `LIBRARY_PATH` already contains
directories needed by other projects, preserve them and add the MSYS2
directory, separated by a semicolon. Enter paths without surrounding quotes
inside either variable's value; the quotes in `set "NAME=value"` are command
syntax.

These commands affect only the current window and programs started from it.
Replace the example paths if DQ or MSYS2 is installed elsewhere.

### Save the settings permanently

1. Search the Start menu for **Edit environment variables for your account**.
2. Under **User variables**, edit **Path** and add the two `PATH` directories
   in the table above. Keep the DQ entry ahead of the MSYS2 entry when
   arranging these entries.
3. Create a user variable named **LIBRARY_PATH** with the value
   `C:\msys64\ucrt64\lib`. If it already exists, add this directory to its
   semicolon-separated list while preserving the existing directories.
4. Confirm the dialogs and open a new terminal. Completely close and reopen
   VS Code if it was already running, so its terminals, build tasks, and
   debugger inherit the new settings.

For a new `LIBRARY_PATH` variable, Command Prompt also supports:

```bat
setx LIBRARY_PATH "C:\msys64\ucrt64\lib"
```

This replaces any existing user value and takes effect in future terminals.
Use the environment-variable editor for permanent `Path` changes:
[`setx`](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/setx)
can truncate long values and expand embedded variable references.

Only the MSYS2 `ucrt64\bin` directory is needed on the Windows `PATH` for these
dependencies. Leave `C:\msys64\usr\bin` out of this setup; it contains tools
for the MSYS2 shell environment.

### PowerShell equivalent for the current window

```powershell
$env:Path = 'D:\PRG\dq\bin;' + $env:Path + ';C:\msys64\ucrt64\bin'
$env:LIBRARY_PATH = 'C:\msys64\ucrt64\lib'
```

As with the Command Prompt example, preserve an existing `LIBRARY_PATH` value
when adding this directory to a shared development environment.

## 4. Verify SQLite development

In a new **Windows Command Prompt**, check the tools and library files:

```bat
where dq-run
where dq-comp
where sqlite3
echo %LIBRARY_PATH%
dir C:\msys64\ucrt64\lib\libsqlite3.dll.a
dq-comp --version
sqlite3 --version
```

Create `sqlite_smoke.dq` in a writable working directory:

```dq
use db, sqlite3, print

func *Main():
    var conn <- OSqlite3Connection()
    conn.filename = ':memory:'
    conn.Open()
    conn.Execute('create table smoke (value integer)')
    conn.Execute('insert into smoke (value) values (42)')
    PrintLn('Rows inserted: {}', [conn.Changes()])
    conn.Close()
endfunc
```

Build and run it:

```bat
dq-run sqlite_smoke.dq
```

Expected application output:

```text
Rows inserted: 1
```

This exercises compilation, linking, DLL loading, and a database operation
without creating a database file. Existing projects can now run with their
normal command, for example:

```bat
dq-run thdatasvr.dqproj
```

Run the command from the application's working directory if it uses relative
paths for databases, configuration, or web content. For VS Code debugging,
continue with [Windows Debugging](windows-debugging.md).

## 5. Add the MySQL-compatible client library

When needed, install the MariaDB C client in **MSYS2 UCRT64**:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-libmariadbclient
```

The [client package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-libmariadbclient)
provides `ucrt64\lib\libmysqlclient.dll.a` for bindings that request
`#linklib('mysqlclient')`, and `ucrt64\bin\libmariadb.dll` at runtime. Pacman
also installs the package's dependencies. The same `PATH` and `LIBRARY_PATH`
settings cover these directories.

This installs a client library, not a database server. Use an existing server,
or install and configure a local MySQL or MariaDB server separately.

Installing the client library does not establish that a DQ binding supports
Win64 correctly. The current `stdpkg/mysql/mysql.dq` declarations for C
`unsigned long` fields and arrays still need a Windows ABI review, including
`MYSQL_FIELD` and `mysql_fetch_lengths()`. C `unsigned long` is 32 bits on
Win64. Validate the binding with real query results before relying on it;
the SQLite smoke test above does not validate MySQL support.

## Troubleshooting

### `dq-run` is not recognized

Check `where dq-run`. Add the DQ `bin` directory to `PATH` and open a new
terminal. If multiple DQ installations are listed, ensure the intended one
appears first.

### `lld: error: unable to find library -lsqlite3`

This is a link-time error. Check both the environment and the import library:

```bat
echo %LIBRARY_PATH%
dir C:\msys64\ucrt64\lib\libsqlite3.dll.a
```

If the file exists, set `LIBRARY_PATH` in the same terminal that runs DQ:

```bat
set "LIBRARY_PATH=C:\msys64\ucrt64\lib"
dq-run thdatasvr.dqproj
```

If the file is missing, install the UCRT64 SQLite package. Installing only
`sqlite3.exe`, or adding only `ucrt64\bin` to `PATH`, does not supply the
linker's search directory.

For a one-command diagnosis, an explicit search path is also supported:

```bat
dq-run --linker-arg=-LC:/msys64/ucrt64/lib thdatasvr.dqproj
```

Use `LIBRARY_PATH` for normal machine setup so projects remain independent of
the local installation directory.

### A DLL is missing when the program starts

Check `where libsqlite3-0.dll` and the MSYS2 `ucrt64\bin` entry in `PATH`.
For missing `libc++.dll` or `libunwind.dll`, check the DQ `bin` entry.
After changing persistent environment settings, restart the terminal or IDE
that launches the program.

### A library has the wrong architecture or format

Use the x86_64 Windows UCRT DQ package with MSYS2 UCRT64 libraries. Libraries
from MSYS2's MSYS environment, a different CPU architecture, or Linux/WSL
cannot substitute for those libraries.

## Deploy an application

Build an executable for distribution with `dq-comp`, for example:

```bat
dq-comp thdatasvr.dqproj
```

End users run the resulting executable directly. They do not need DQ or MSYS2
installed. For a build linked to the SQLite DLL, put the required runtime
DLLs beside the executable, preserving their names:

```text
MyApp\
    thdatasvr.exe
    libsqlite3-0.dll
    libc++.dll
    libunwind.dll
    start.cmd
    www\
        ...web content...
    data\
        terrace_data.db
```

Copy SQLite's DLL from `C:\msys64\ucrt64\bin`, and DQ's runtime DLLs from
`D:\PRG\dq\bin`. This is the dependency set observed for the SQLite data
server build; inspect each application's dependencies when packaging it.
The headers, `.a` import libraries, and `sqlite3.exe` are development tools
and do not belong in a normal runtime package.

The bundled toolchain can list an executable's imported DLLs:

```bat
"D:\PRG\dq\toolchain\llvm-mingw\bin\llvm-readobj.exe" --coff-imports thdatasvr.exe
```

Inspect each non-system DLL's imports as well, and include its required
non-system dependencies. Keep the DLL versions used to test the release.
MySQL client deployments can also require authentication plugins and TLS
configuration or certificate files that are loaded at runtime.

Windows searches the executable's directory for ordinary application DLLs,
so an application-local bundle needs no user `PATH` changes. See
[DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order)
for the Windows loading rules. Windows supplies system libraries and the
Universal C Runtime on supported Windows 10/11 installations.

For applications that use relative paths, a `start.cmd` can select the correct
working directory:

```bat
@echo off
cd /d "%~dp0"
thdatasvr.exe %*
```

For a portable application that writes to `data`, extract the package into a
writable folder. An application installed under `Program Files` should store
its writable database in an appropriate user or application-data directory.
Supply a fresh initialized database, or initialize its schema on first run;
preserve the user's database during upgrades. Use SQLite's backup facilities
when taking a deployment copy of a database that is in use.

Include the applicable third-party license notices with the distribution.
Finally, test the complete package on a clean Windows machine or VM without
DQ or MSYS2 installed. This detects dependencies that your development
machine's environment might otherwise supply.
