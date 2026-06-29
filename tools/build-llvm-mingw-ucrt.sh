#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

LLVM_VERSION="${LLVM_VERSION:-21.1.8}"
MINGW_ROOT="${MINGW_ROOT:-$ROOT_DIR/sysroots/llvm-mingw-x86_64}"
SRC_ARCHIVE="${SRC_ARCHIVE:-$ROOT_DIR/sysroots/llvm-project-$LLVM_VERSION.src.tar.xz}"
SRC_DIR="${SRC_DIR:-$ROOT_DIR/sysroots/llvm-project-$LLVM_VERSION.src}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-cross/llvm-x86_64-win-ucrt}"
INSTALL_DIR="${INSTALL_DIR:-$ROOT_DIR/sysroots/llvm-x86_64-win-ucrt}"
JOBS="${JOBS:-$(nproc)}"

LLVM_TBLGEN="${LLVM_TBLGEN:-/usr/lib/llvm-21/bin/llvm-tblgen}"

if [[ ! -x "$MINGW_ROOT/bin/x86_64-w64-mingw32-clang++" ]]; then
  echo "Missing llvm-mingw toolchain: $MINGW_ROOT" >&2
  echo "Extract llvm-mingw-*-ucrt-ubuntu-22.04-x86_64.tar.xz there first." >&2
  exit 1
fi

if [[ ! -x "$LLVM_TBLGEN" ]]; then
  echo "Missing native llvm-tblgen: $LLVM_TBLGEN" >&2
  echo "Install or build host LLVM $LLVM_VERSION tools, then set LLVM_TBLGEN." >&2
  exit 1
fi

if [[ ! -f "$SRC_ARCHIVE" ]]; then
  mkdir -p "$(dirname "$SRC_ARCHIVE")"
  curl -L \
    "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/llvm-project-$LLVM_VERSION.src.tar.xz" \
    -o "$SRC_ARCHIVE"
fi

if [[ ! -d "$SRC_DIR/llvm" ]]; then
  rm -rf "$SRC_DIR"
  mkdir -p "$SRC_DIR"
  tar -xf "$SRC_ARCHIVE" -C "$SRC_DIR" --strip-components=1
fi

cmake -S "$SRC_DIR/llvm" -B "$BUILD_DIR" \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER="$MINGW_ROOT/bin/x86_64-w64-mingw32-clang" \
  -DCMAKE_CXX_COMPILER="$MINGW_ROOT/bin/x86_64-w64-mingw32-clang++" \
  -DCMAKE_RC_COMPILER="$MINGW_ROOT/bin/x86_64-w64-mingw32-windres" \
  -DCMAKE_AR="$MINGW_ROOT/bin/x86_64-w64-mingw32-llvm-ar" \
  -DCMAKE_RANLIB="$MINGW_ROOT/bin/x86_64-w64-mingw32-llvm-ranlib" \
  -DCMAKE_FIND_ROOT_PATH="$MINGW_ROOT/x86_64-w64-mingw32;$MINGW_ROOT" \
  -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TABLEGEN="$LLVM_TBLGEN" \
  -DLLVM_HOST_TRIPLE=x86_64-w64-windows-gnu \
  -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-w64-windows-gnu \
  -DLLVM_TARGETS_TO_BUILD=X86 \
  -DLLVM_ENABLE_PROJECTS="" \
  -DLLVM_INCLUDE_TESTS=OFF \
  -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_INCLUDE_BENCHMARKS=OFF \
  -DLLVM_INCLUDE_DOCS=OFF \
  -DLLVM_INCLUDE_TOOLS=OFF \
  -DLLVM_BUILD_TOOLS=OFF \
  -DLLVM_ENABLE_BINDINGS=OFF \
  -DLLVM_ENABLE_ZLIB=OFF \
  -DLLVM_ENABLE_ZSTD=OFF \
  -DLLVM_ENABLE_TERMINFO=OFF \
  -DLLVM_ENABLE_LIBXML2=OFF \
  -DBUILD_SHARED_LIBS=OFF \
  -DLLVM_BUILD_LLVM_DYLIB=OFF

cmake --build "$BUILD_DIR" -j "$JOBS"
cmake --install "$BUILD_DIR"

echo
echo "LLVM_DIR=$INSTALL_DIR/lib/cmake/llvm"
