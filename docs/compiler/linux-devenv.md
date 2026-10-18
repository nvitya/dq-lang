# Linux Development Environment

## To Run the DQ Compiler

On Ubuntu 26.04, this is the minimum that the dq-comp works:
```
sudo apt update
sudo apt install clang lld libbacktrace-dev
```

On Ubuntu 24.04
```
sudo apt install lsb-release wget software-properties-common gnupg

wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh

sudo ./llvm.sh 21
```

## To Compile The DQ Compiler

```
sudo apt install cmake build-essential

```

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

Install the packages needed to build DQ:

```bash
sudo apt install \
  build-essential cmake llvm-21-dev clang-21 lld-21 \
  zlib1g-dev libzstd-dev libedit-dev libcurl4-openssl-dev \
  libxml2-dev libffi-dev
```
