# Linux Development Environment

Setting up the development environment on Linux is relative simple. If you are new to Linux I recommend to install the latest LTS version (currently 26.04) of the KUbuntu (Ubuntu with KDE desktop). These guide is tested for Ubuntu systems, but it should work similar, deb-based systems like Debian, Mint.

## 1. Install the required packages To Run the DQ Compiler

In order to get the dq-comp working the following setup is required on Ubuntu 26.04:
```
sudo apt update
sudo apt install clang lld libbacktrace-dev
```
This command installs the lld-21 and clang-21 packages required by the dq-comp.

On Ubuntu 24.04 you need to install the llvm-21 by the llvm install script:
```
sudo apt install lsb-release wget software-properties-common gnupg

wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh

sudo ./llvm.sh 21
```
This way the llvm-21-dev package is also installed (which is required for the DQ compiler development)

## 2. Download and Install the Linux DQ Compiler Package

Download the **x86_64-linux** archieve from the [DQ releases page](https://github.com/nvitya/dq-lang/releases). Example file name: `dq-0.93.8-x86_64-linux.tar.gz`

Extract the complete archive into some local directory. This guide uses `~/dq-0.93.8-x86_64-linux`.
Enter the extracted directory and execute the install script:
```
sudo ./install.sh
```
This installs the DQ compiler and its libraries to `/usr/local/bin` and `/usr/local/lib/dq`.

## 3. Setup VSCode with DQ Extension

For productive development you need an advanced source code editor with DQ programming language support.
The DQ release package contains an extension file for Visual Studio Code (VSCode for short).

If you don't have VSCode installed, download and install first from [https://code.visualstudio.com/download](https://code.visualstudio.com/download).

Start VSCode, press `CTRL+SHIFT+P` and select/search for **Extensions: Install from VSIX...**.
Then browse for the `~/dq-0.93.8-x86_64-linux/VSCode/dq-syntax-0.1.4.vsix` (or higher version number).

Restart the VSCode so the DQ extension gets fully working.

### Run DQ Examples from VSCode

Start VSCode and open the `~/dq-0.93.8-x86_64-linux\examples` folder using `File/Open Folder...`.
Open (double-click) on `langdemo.dq`. The file should appear syntax-highlighted.

At the right top corner there is a small "Play" (triangle) symbol with the tooltip "Run Current File". Click on that, and the output of the `langdemo.dq` should appear.

(Note: the "Run Current File" just executes `dq-run langdemo.dq`)

### Debugging DQ Programs from VSCode

To run DQ programs with breakpoints, stepping and variable inspections you need to set up the project for DQ debugging. See the ["VSCode Setup for DQ Development"](vscode-setup.md) page for the setup details.

## 4. Install Third-Party Developer Libraries

For using third-party libraries you might need to install the their developer packages. For SQLite3 and MySQL/MariaDB you need these commands too:
```
sudo apt install libsqlite3-dev
sudo apt install libmysqlclient-dev
```

## 5. Install the Required Packages To Compile The DQ Compiler

For the DQ compiler development (to compile the DQ compiler itself) more packages are required.

### Base Developer Packages
First the base compiler/development packages:
```bash
sudo apt install build-essential cmake zlib1g-dev libzstd-dev \
  libedit-dev libcurl4-openssl-dev libxml2-dev libffi-dev
```

### llvm-21-dev

The DQ compiler requires the llvm-21-dev package.

On Ubuntu 26.04 the llvm-21 can be simply installed from the Ubuntu repository:
```
sudo apt install llvm-21-dev clang-21 lld-21
```

On Ubuntu 24.04 the llvm must be installed from llvm.org:
```
sudo apt install lsb-release wget software-properties-common gnupg
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 21
sudo apt install clang-21 lld-21
```

### g++-14 or greater

The DQ compiler requires g++ version 14 or greater.

Check g++ version:
```
g++ --version
```

If less than 14:
```
sudo apt install gcc-14 g++-14
```

To make GCC 14 the default compiler for normal shell commands, register it with
`update-alternatives`:

```bash
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 140
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 140
sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-14 140
```

Verify the selected versions again:

```bash
g++ --version
```

