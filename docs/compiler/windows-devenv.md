# Windows Development Environment

In order to develop DQ applications on Windows you need the Windows version of the DQ compiler package.
The DQ compiler translates your DQ source code (.dq files) into executable machine code (.exe files on Windows).

## 1. Download and Extract the DQ Compiler

Download the **x86_64 Windows UCRT** archive from the [DQ releases page](https://github.com/nvitya/dq-lang/releases). Example file name: `dq-0.93.4-x86_64-windows-ucrt.zip`

Extract the complete archive into some local directory. Avoid directories with spaces, like "C:\Program Files" because the DQ compiler uses several external tools that might not handle the spaces in the path properly.

This guide uses `D:\PRG\dq` as the installation directory, but you can keep the original long directory name too, e.g. `D:\PRG\dq-0.93.4-x86_64-windows-ucrt`

## 2. Add the DQ Compiler to the PATH

The next step is to add the `D:\PRG\dq\bin` directory to the `PATH` variable. This can be done temporarily from command line with this command:
```bat
set "PATH=D:\PRG\dq\bin;%PATH%"
```
But it is recommended to make it permanent using the environment variable editor in Windows.

1. Search the Start menu for **Edit environment variables for your account**.
2. Under **User variables**, edit **Path** and add a `PATH` directory in the table above.

After the path setup the newly opened command line windows should have the updated path settings.
Test the DQ compiler accessibility from the command line:
```
dq-comp --version
```
The response should be just the compiler version number, like `0.93.4`.

Now your system is ready to compile and run basic DQ programs (that don't require external libraries).

## 3. Setup VSCode with DQ Extension

For productive development you need an advanced source code editor with DQ programming language support.
The DQ release package contains an extension file for Visual Studio Code (VSCode for short).

If you don't have VSCode installed, download and install first from [https://code.visualstudio.com/download](https://code.visualstudio.com/download).

Start VSCode, press `CTRL+SHIFT+P` and select/search for **Extensions: Install from VSIX...**.
Then browse for the `D:\PRG\dq\VSCode\dq-syntax-0.1.4.vsix` (or higher version number).

Restart the VSCode so the DQ extension gets fully working.

### Run DQ Examples from VSCode

Start VSCode and open the `D:\PRG\dq\examples` folder using `File/Open Folder...`.
Open (double-click) on `langdemo.dq`. The file should appear syntax-highlighted.

At the right top corner there is a small "Play" (triangle) symbol with the tooltip "Run Current File". Click on that, and the output of the `langdemo.dq` should appear.

(Note: the "Run Current File" just executes `dq-run langdemo.dq`)

### Debugging DQ Programs from VSCode

To run DQ programs with breakpoints, stepping and variable inspections you need to set up the project for DQ debugging. See the ["Debugging DQ programs in VS Code on Windows"](windows-debugging.md) page for the setup details.

## 4. Adding External Libraries using MSYS2

Some programs require external libraries, like SQLite3, or MySQL/MariaDB. These libraries are not bundled with the DQ compiler release package.

The recommended way is to get windows versions of these open-source libraries through MSYS2.

### Install MSYS2 with SQLite and MySQL

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

Install the MariaDB C client in **MSYS2 UCRT64**:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-libmariadbclient
```

The [client package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-libmariadbclient)
provides `ucrt64\lib\libmysqlclient.dll.a` for bindings that request
`#linklib('mysqlclient')`, and `ucrt64\bin\libmariadb.dll` at runtime. Pacman
also installs the package's dependencies.

### Add MSYS2 directories to PATH and LIBRARY_PATH

Two environment variables are needed when using the MSYS2 libraries:

| Variable | Directory | Purpose |
| --- | --- | --- |
| `PATH` | `C:\msys64\mingw64\bin` | Find the runtime DLLs during application execution |
| `LIBRARY_PATH` | `C:\msys64\ucrt64\lib` | Find third-party libraries for DQ program linking |

1. Search the Start menu for **Edit environment variables for your account**.
2. Under **User variables**, edit **Path** and add the `PATH` directory `C:\msys64\mingw64\bin`
3. Create a user variable named **LIBRARY_PATH** with the value
   `C:\msys64\ucrt64\lib`. If it already exists, add this directory to its
   semicolon-separated list while preserving the existing directories.
4. Confirm the dialogs and open a new terminal. Completely close and reopen
   VS Code if it was already running, so its terminals, build tasks, and
   debugger inherit the new settings.


### Verify SQLite development

From command line:

Open a terminal and navigate to `D:\PRG\dq`. Then execute the following command:
```
dq-run stdpkg\sqlite3\examples\sqlite_test.dq
```

With VSCode:

Open the folder `D:\PRG\dq`. Browse the file `stdpkg\sqlite3\examples\sqlite_test.dq`.
Run the file with the icon at the top-right corner ("Run Current File").


## 5. Running the Compiled Executables on Other Computers

However the .exe file produced by dq-comp is running fine on your developer Windows environment, other Windows computer probably won't have this setup. In order to run the compiled executable you need to supply the dlls that it requires to run. Two .dll-s are required for every DQ program (which is available at `D:\PRG\dq\bin`):

 * libc++.dll
 * libunwind.dll

Further third-party libraries might be required from the MSYS2 like

 * libsqlite3-0.dll

Which can be found at `C:\msys64\mingw64\bin`.

The bundled toolchain can list an executable's imported DLLs:

```bat
"D:\PRG\dq\toolchain\llvm-mingw\bin\llvm-readobj.exe" --coff-imports sqlite_test.exe
```

For a build linked to the SQLite DLL, put the required runtime
DLLs beside the executable, preserving their names:

```text
MyApp\
    myapp.exe
    libsqlite3-0.dll
    libc++.dll
    libunwind.dll
    licenses.txt
    www\
        ...web content...
    data\
        some_data.db
```

Note: Include the applicable third-party license notices with the distribution.

### Static Linking

Sometimes also possible to create a larger .exe file that does not need that much external dll files by "static linking". This way the .exe file will contain these third-party libraries.

You can instruct the DQ compiler for static liking with the `--static` command line option:

```
dq-comp --static stdpkg\sqlite3\examples\sqlite_test.dq
```

Notes:

 * The executable is significantly bigger this way
 * Static linking might not be possible for every third-party library
 * Static linking might be not allowed (or not for free) by third-party library license

## 6. Troubleshooting

### `lld: error: unable to find library -lsqlite3`

This is a link-time error. Check both the environment and the import library:

```bat
echo %LIBRARY_PATH%
dir C:\msys64\ucrt64\lib\libsqlite3.dll.a
```

If the file exists, set `LIBRARY_PATH` in the same terminal that runs DQ:

```bat
set "LIBRARY_PATH=C:\msys64\ucrt64\lib"
dq-run stdpkg\sqlite3\examples\sqlite_test.dq
```

If the file is missing, install the UCRT64 SQLite package. Installing only
`sqlite3.exe`, or adding only `ucrt64\bin` to `PATH`, does not supply the
linker's search directory.

For a one-command diagnosis, an explicit search path is also supported:

```bat
dq-run --linker-arg=-LC:/msys64/ucrt64/lib stdpkg\sqlite3\examples\sqlite_test.dq
```

Use `LIBRARY_PATH` for normal machine setup so projects remain independent of
the local installation directory.

### A DLL is missing when the program starts

Check `where libsqlite3-0.dll` and the MSYS2 `c:\msys64\mingw64\bin` entry in `PATH`.
For missing `libc++.dll` or `libunwind.dll`, check the DQ `bin` entry.
After changing persistent environment settings, restart the terminal or IDE
that launches the program.

### A library has the wrong architecture or format

Use the x86_64 Windows UCRT DQ package with MSYS2 UCRT64 libraries. Libraries
from MSYS2's MSYS environment, a different CPU architecture, or Linux/WSL
cannot substitute for those libraries.
