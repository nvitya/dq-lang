#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-cross/x86_64-win}"
WINDOWS_TOOLCHAIN_ZIP="${DQ_WINDOWS_TOOLCHAIN_ZIP:-$ROOT_DIR/sysroots/llvm-mingw-20251216-ucrt-x86_64.zip}"
WINDOWS_TOOLCHAIN_ROOT="${DQ_WINDOWS_TOOLCHAIN_ROOT:-}"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/dist}"
VERSION="$(sed -n 's/^#define DQ_COMPILER_VERSION[[:space:]]*"\([^"]*\)".*/\1/p' "$ROOT_DIR/compiler/src/version.h")"

if [[ -z "$VERSION" ]]; then
  echo "Can not read DQ compiler version from compiler/src/version.h" >&2
  exit 1
fi

PACKAGE_NAME="${PACKAGE_NAME:-dq-$VERSION-x86_64-windows-ucrt}"
PACKAGE_ZIP="$OUT_DIR/$PACKAGE_NAME.zip"
PACKAGE_TMP_ZIP="$OUT_DIR/$PACKAGE_NAME.zip.tmp.$$"
STAGE_PARENT="$OUT_DIR/.stage-$PACKAGE_NAME-$$"
STAGE="$STAGE_PARENT/$PACKAGE_NAME"

require_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "Required file is missing: $path" >&2
    exit 1
  fi
}

require_dir() {
  local path="$1"
  if [[ ! -d "$path" ]]; then
    echo "Required directory is missing: $path" >&2
    exit 1
  fi
}

copy_tracked_paths() {
  local path
  git -C "$ROOT_DIR" ls-files -z -- "$@" |
    while IFS= read -r -d '' path; do
      mkdir -p "$STAGE/$(dirname "$path")"
      cp -p "$ROOT_DIR/$path" "$STAGE/$path"
    done
}

install_windows_toolchain() {
  local dst="$STAGE/toolchain/llvm-mingw"

  mkdir -p "$STAGE/toolchain"

  if [[ -n "$WINDOWS_TOOLCHAIN_ROOT" ]]; then
    require_dir "$WINDOWS_TOOLCHAIN_ROOT"
    cp -R "$WINDOWS_TOOLCHAIN_ROOT" "$dst"
  else
    require_file "$WINDOWS_TOOLCHAIN_ZIP"
    if ! command -v unzip >/dev/null 2>&1; then
      echo "The unzip tool is required to stage the Windows llvm-mingw toolchain." >&2
      exit 1
    fi

    local extract_dir="$STAGE/toolchain/.extract"
    mkdir -p "$extract_dir"
    unzip -q "$WINDOWS_TOOLCHAIN_ZIP" -d "$extract_dir"

    local topdirs=("$extract_dir"/*)
    if [[ ${#topdirs[@]} -ne 1 || ! -d "${topdirs[0]}" ]]; then
      echo "Unexpected Windows llvm-mingw archive layout: $WINDOWS_TOOLCHAIN_ZIP" >&2
      exit 1
    fi
    mv "${topdirs[0]}" "$dst"
    rmdir "$extract_dir"
  fi

  require_file "$dst/bin/clang.exe"
  require_file "$dst/bin/clang-21.exe"
  require_file "$dst/bin/ld.lld.exe"
  require_file "$dst/lib/clang/21/lib/windows/libclang_rt.builtins-x86_64.a"
  require_file "$dst/x86_64-w64-mingw32/lib/crt2.o"
  require_file "$dst/x86_64-w64-mingw32/lib/crtbegin.o"
  require_file "$dst/x86_64-w64-mingw32/lib/crtend.o"
  require_file "$dst/x86_64-w64-mingw32/lib/libmingw32.a"
  require_file "$dst/x86_64-w64-mingw32/lib/libucrt.a"
  require_file "$dst/x86_64-w64-mingw32/lib/libkernel32.a"
  require_file "$dst/x86_64-w64-mingw32/bin/libc++.dll"
  require_file "$dst/x86_64-w64-mingw32/bin/libunwind.dll"
  require_file "$dst/LICENSE.TXT"
}

require_file "$BUILD_DIR/bin/dq-comp.exe"
require_file "$BUILD_DIR/bin/dq-run.exe"
require_file "$BUILD_DIR/bin/dqatrun.exe"
require_file "$ROOT_DIR/LICENSE"
require_file "$ROOT_DIR/README.md"
require_dir "$ROOT_DIR/examples"
require_dir "$ROOT_DIR/autotest/tests"

if [[ -e "$STAGE_PARENT" ]]; then
  echo "Temporary staging path already exists: $STAGE_PARENT" >&2
  exit 1
fi

cleanup() {
  rm -rf "$STAGE_PARENT"
  rm -f "$PACKAGE_TMP_ZIP"
}
trap cleanup EXIT

mkdir -p "$STAGE/bin" "$STAGE/doc"
install_windows_toolchain

cp "$BUILD_DIR/bin/dq-comp.exe" "$STAGE/bin/"
cp "$BUILD_DIR/bin/dq-run.exe" "$STAGE/bin/"
cp "$BUILD_DIR/bin/dqatrun.exe" "$STAGE/bin/"
cp "$STAGE/toolchain/llvm-mingw/x86_64-w64-mingw32/bin/libc++.dll" "$STAGE/bin/"
cp "$STAGE/toolchain/llvm-mingw/x86_64-w64-mingw32/bin/libunwind.dll" "$STAGE/bin/"
if [[ -f "$STAGE/toolchain/llvm-mingw/x86_64-w64-mingw32/bin/libwinpthread-1.dll" ]]; then
  cp "$STAGE/toolchain/llvm-mingw/x86_64-w64-mingw32/bin/libwinpthread-1.dll" "$STAGE/bin/"
fi

copy_tracked_paths stdpkg examples autotest/tests
cp "$ROOT_DIR/LICENSE" "$STAGE/LICENSE.txt"
cp "$STAGE/toolchain/llvm-mingw/LICENSE.TXT" "$STAGE/doc/llvm-mingw-LICENSE.txt"
cp "$ROOT_DIR/README.md" "$STAGE/doc/README-project.md"
cp "$ROOT_DIR/doc/dq-cross-compiling.md" "$STAGE/doc/"
cp "$ROOT_DIR/doc/dq_package_spec.md" "$STAGE/doc/"

cat > "$STAGE/dq-env.cmd" <<'EOF'
@echo off
set "DQ_ROOT=%~dp0"
set "PATH=%DQ_ROOT%bin;%DQ_ROOT%toolchain\llvm-mingw\bin;%PATH%"
echo DQ environment ready.
echo   dq-comp: %DQ_ROOT%bin\dq-comp.exe
echo   clang:   %DQ_ROOT%toolchain\llvm-mingw\bin\clang.exe
EOF

cat > "$STAGE/README-WINDOWS.txt" <<EOF
DQ Compiler $VERSION for x86_64 Windows/UCRT

Quick start:
  1. Extract this archive.
  2. Open cmd.exe in the extracted directory.
  3. Run:
       dq-env.cmd
       dq-comp --version

Included:
  bin/dq-comp.exe
  bin/dq-run.exe
  bin/dqatrun.exe
  bin/libc++.dll
  bin/libunwind.dll
  toolchain/llvm-mingw/
  stdpkg/
  examples/
  autotest/tests/

Linking:
  dq-comp links application executables with:
    toolchain\llvm-mingw\bin\clang.exe --target=x86_64-w64-windows-gnu -fuse-ld=lld

  Set DQ_LINKER_DRIVER to override the linker driver path.
EOF

mkdir -p "$OUT_DIR"
(
  cd "$STAGE_PARENT"
  zip -qr "$PACKAGE_TMP_ZIP" "$PACKAGE_NAME"
)
mv -f "$PACKAGE_TMP_ZIP" "$PACKAGE_ZIP"

echo "$PACKAGE_ZIP"
