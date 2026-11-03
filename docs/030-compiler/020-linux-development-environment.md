# Linux Development Environment

This chapter sets up a Linux computer for developing DQ applications. The
package commands below use Ubuntu; other distributions may use different
package names and installation commands.

## Download and Extract DQ

Download the Linux compiler archive for your CPU architecture from the
[DQ releases page](https://github.com/nvitya/dq-lang/releases). For an x86_64
computer, the full package is named
`dq-VERSION-x86_64-linux-full.tar.gz`, where `VERSION` is the release number.

The full package includes the DQ tools, standard packages, examples, and the
Clang/LLVM toolchain used to link programs. It is the simplest starting point
because it supplies the compiler and linker dependencies together.

Extract the complete archive into a directory you can keep. For the commands
below, replace `VERSION` with the version in the downloaded filename:

```bash
tar -xzf dq-VERSION-x86_64-linux-full.tar.gz
cd dq-VERSION-x86_64-linux-full
```

Keep the package directories together. The compiler locates the standard
packages and bundled tools relative to its installation.

## Set Up the Shell Environment

From the extracted full package directory, load its environment script:

```bash
source ./dq-env.sh
dq-comp --version
```

This configures the current Bash shell to find the DQ tools and their bundled
toolchain. `PATH` is the list of directories the shell searches for commands.
You need to source the script again in a new terminal unless you install DQ or
add the setup to your shell startup file.

To try an included example from the extracted package directory:

```bash
dq-run examples/basic/test1.dq
```

### Install DQ

Both full and smaller Linux compiler archives include an installer. From the
extracted archive, install for the current user with:

```bash
./install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
dq-comp --version
```

Add the `export PATH=...` line to `~/.bashrc` to make it available in future
Bash terminals. The installer places the tools and supporting files under
`~/.local/lib/dq` and creates command symlinks in `~/.local/bin`.

For a system-wide installation, use this alternative:

```bash
sudo ./install.sh
dq-comp --version
```

The default installation directory is `/usr/local/lib/dq`, with command
symlinks in `/usr/local/bin`. Installing a full archive also installs its
bundled toolchain. Examples and the VS Code extension remain in the extracted
archive, so keep it if you want to use those files.

### Using a Package Without a Bundled Toolchain

If the release provides a smaller archive named
`dq-VERSION-x86_64-linux.tar.gz`, it uses the system's compiler and linker
dependencies. Follow that release's dependency instructions.

For a release built with LLVM 21, an Ubuntu 26.04 setup is:

```bash
sudo apt update
sudo apt install clang-21 lld-21 g++ libc6-dev libbacktrace-dev
export DQ_LINKER_DRIVER=clang++-21
```

The selected linker driver must match the LLVM major version used to build DQ,
especially when using link-time optimization. Ubuntu 26.04 provides
[Clang 21](https://packages.ubuntu.com/resolute/clang-21). On distributions
without the required version, the [LLVM APT repository](https://apt.llvm.org/)
provides installation instructions and a script for selecting a specific
version. The full DQ package avoids this additional setup.

## Set Up Visual Studio Code

Install [Visual Studio Code](https://code.visualstudio.com/download), then
install the DQ extension shipped in the compiler archive:

1. Open the Command Palette with `Ctrl+Shift+P`.
2. Run **Extensions: Install from VSIX...**.
3. Select the `dq-syntax-*.vsix` file in the archive's `VSCode` directory.
4. Reload the VS Code window if prompted.

Open the extracted package's `examples` directory with **File > Open Folder**,
then open `langdemo.dq`. It should appear with DQ syntax highlighting. Click
the play button in the editor title bar or run **DQ: Run Current File** from
the Command Palette.

The extension runs the current file through `dq-run` with debugging options
`-g -O0`. It also starts `dq-comp` as a language server to provide compiler
diagnostics and document symbols. Both commands must be available to VS Code.

If you sourced `dq-env.sh` in a terminal, start VS Code from that terminal
with `code .` so it inherits the environment. After changing persistent
environment settings, close and reopen VS Code.

For a portable installation that is not on `PATH`, you can set explicit tool
paths in VS Code's `settings.json`:

```json
{
    "dq.languageServerPath": "/home/yourname/dq/bin/dq-comp",
    "dq.runPath": "/home/yourname/dq/bin/dq-run"
}
```

Replace these example paths with your actual installation paths. See
[VSCode Setup and Debugging](080-vscode-setup-and-debugging.md) for the chapter
dedicated to editor and debugger configuration.

## Add External Libraries

Some DQ packages call external C libraries. Install the development package
for each library you use so the linker can find it. For example, on Ubuntu:

```bash
sudo apt install libsqlite3-dev
```

For the MySQL client library:

```bash
sudo apt install libmysqlclient-dev
```

These are optional dependencies for applications that use the corresponding
database packages. A database server is a separate installation.

## Troubleshooting

### The Shell Cannot Find `dq-comp`

Check which command the shell finds:

```bash
command -v dq-comp
```

Source the full package's `dq-env.sh`, or add the installation's command
directory to `PATH`. If multiple versions are installed, check that the
reported path is the version you intend to use.

### Linking Fails

Keep the full package's `toolchain` and `stdpkg` directories with its tools.
When using system tools, check `clang++-21 --version` and `ld.lld-21 --version`
and select the matching driver with `DQ_LINKER_DRIVER`.

An error such as `unable to find library -lsqlite3` means the linker cannot
find the requested library. Install its development package. If the problem
only occurs in VS Code, check that the editor inherited the same environment
as the working terminal.

## Next Steps

Return to [Getting Started](../010-project/030-getting-started.md) to write and
run your first program.

Building the DQ compiler itself requires additional C++ and LLVM development
packages. That workflow belongs in
[Building the Compiler from Source](../090-appendices/020-building-the-compiler-from-source.md).
