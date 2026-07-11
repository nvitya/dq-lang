#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/dist}"
VERSION="$(sed -n 's/^#define DQ_COMPILER_VERSION[[:space:]]*"\([^"]*\)".*/\1/p' "$ROOT_DIR/compiler/src/version.h")"
ARCH="${DQ_LINUX_PACKAGE_ARCH:-$(uname -m)}"
FULL_RELEASE="${DQ_LINUX_PACKAGE_FULL:-0}"
TOOLCHAIN_ROOT="${DQ_LINUX_TOOLCHAIN_ROOT:-}"
RUNTIME_LIB_ROOT="${DQ_LINUX_RUNTIME_LIB_ROOT:-}"
SYSROOT_ROOT="${DQ_LINUX_SYSROOT_ROOT:-}"

if [[ -z "$VERSION" ]]; then
  echo "Can not read DQ compiler version from compiler/src/version.h" >&2
  exit 1
fi

PACKAGE_NAME="${PACKAGE_NAME:-dq-$VERSION-$ARCH-linux}"
PACKAGE_TAR="$OUT_DIR/$PACKAGE_NAME.tar.gz"
PACKAGE_TMP_TAR="$OUT_DIR/$PACKAGE_NAME.tar.gz.tmp.$$"
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

default_toolchain_root() {
  local clang_bin
  clang_bin="$(command -v clang++ || true)"
  if [[ -z "$clang_bin" ]]; then
    return 1
  fi

  clang_bin="$(readlink -f "$clang_bin")"
  dirname "$(dirname "$clang_bin")"
}

install_linux_toolchain() {
  local src="${TOOLCHAIN_ROOT:-}"
  local dst="$STAGE/toolchain"

  if [[ -z "$src" ]]; then
    if ! src="$(default_toolchain_root)"; then
      echo "Can not find clang++; set DQ_LINUX_TOOLCHAIN_ROOT for the full Linux release package." >&2
      exit 1
    fi
  fi

  require_dir "$src"
  require_file "$src/bin/clang++"

  mkdir -p "$(dirname "$dst")"
  cp -a "$src" "$dst"

  resolve_external_toolchain_symlinks "$src" "$dst"
  wrap_linux_toolchain_driver
  bundle_linux_toolchain_shared_deps
  install_linux_sysroot
  require_file "$dst/bin/clang++"
}

install_linux_sysroot() {
  local dst="$STAGE/toolchain/sysroot"

  if [[ -z "$SYSROOT_ROOT" ]]; then
    return
  fi

  require_dir "$SYSROOT_ROOT"
  cp -a "$SYSROOT_ROOT" "$dst"
  if [[ -d "$dst/usr/lib" && ! -e "$dst/lib" ]]; then
    ln -s usr/lib "$dst/lib"
  fi
  if [[ -d "$dst/usr/lib64" && ! -e "$dst/lib64" ]]; then
    ln -s usr/lib64 "$dst/lib64"
  fi
}

resolve_external_toolchain_symlinks() {
  local src="$1"
  local dst="$2"
  local link rel target dst_link

  while IFS= read -r -d '' link; do
    target="$(readlink -f "$link" || true)"
    if [[ -z "$target" || "$target" == "$src"/* ]]; then
      continue
    fi

    rel="${link#"$src"/}"
    dst_link="$dst/$rel"
    if [[ -d "$target" ]]; then
      rm -f "$dst_link"
      continue
    fi

    rm -f "$dst_link"
    cp -pL "$target" "$dst_link"
  done < <(find "$src" -type l -print0)
}

wrap_linux_toolchain_driver() {
  local bin_dir="$STAGE/toolchain/bin"

  require_file "$bin_dir/clang"
  mv "$bin_dir/clang" "$bin_dir/clang.real"
  cat > "$bin_dir/clang" <<'EOF'
#!/bin/sh
case "$0" in
  */*) tool_bin=${0%/*} ;;
  *) tool_bin=. ;;
esac

old_pwd=$(pwd)
cd "$tool_bin" || exit 1
tool_bin=$(pwd -P)
cd "$old_pwd" || exit 1
tool_root=${tool_bin%/*}
sysroot=$tool_root/sysroot

LD_LIBRARY_PATH="$tool_root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

if [ -d "$sysroot" ]; then
  set -- "--sysroot=$sysroot" "$@"
fi

prog=${0##*/}
case "$prog" in
  clang++*) exec "$tool_bin/clang.real" --driver-mode=g++ "$@" ;;
  *) exec "$tool_bin/clang.real" "$@" ;;
esac
EOF
  chmod +x "$bin_dir/clang"
}

is_glibc_runtime_dep() {
  case "$(basename "$1")" in
    ld-linux*.so*|libBrokenLocale.so*|libanl.so*|libc.so*|libdl.so*|libm.so*|libmvec.so*|libnsl.so*|libnss_*.so*|libpthread.so*|libresolv.so*|librt.so*|libthread_db.so*|libutil.so*)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

bundle_linux_toolchain_shared_deps() {
  local toolchain="$STAGE/toolchain"
  local dep dep_base dst_dep pass file
  local -a scan_roots

  for pass in 1 2 3; do
    mapfile -d '' scan_roots < <(
      find "$toolchain/bin" -maxdepth 1 -type f -perm -111 -print0
      find "$toolchain/lib" -maxdepth 1 -type f \( -name '*.so' -o -name '*.so.*' \) -print0
    )

    for file in "${scan_roots[@]}"; do
      while IFS= read -r dep; do
        [[ -n "$dep" && -f "$dep" ]] || continue
        [[ "$dep" != "$toolchain"/* ]] || continue
        is_glibc_runtime_dep "$dep" && continue

        if [[ -n "$RUNTIME_LIB_ROOT" && -e "$RUNTIME_LIB_ROOT/$dep" ]]; then
          dep="$RUNTIME_LIB_ROOT/$dep"
        fi

        dep_base="$(basename "$dep")"
        dst_dep="$toolchain/lib/$dep_base"
        [[ -e "$dst_dep" ]] && continue
        cp -pL "$dep" "$dst_dep"
      done < <(LD_LIBRARY_PATH="$toolchain/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ldd "$file" 2>/dev/null |
                 awk '/=> \// { print $3 } /^\// { print $1 }')
    done
  done
}

copy_tracked_paths() {
  local path
  git -C "$ROOT_DIR" ls-files -z -- "$@" |
    while IFS= read -r -d '' path; do
      mkdir -p "$STAGE/$(dirname "$path")"
      cp -p "$ROOT_DIR/$path" "$STAGE/$path"
    done
}

require_file "$BUILD_DIR/dq-comp"
require_file "$BUILD_DIR/dq-run"
require_file "$BUILD_DIR/dqatrun"
require_file "$ROOT_DIR/LICENSE"
require_file "$ROOT_DIR/README.md"

if [[ -e "$STAGE_PARENT" ]]; then
  echo "Temporary staging path already exists: $STAGE_PARENT" >&2
  exit 1
fi

cleanup() {
  rm -rf "$STAGE_PARENT"
  rm -f "$PACKAGE_TMP_TAR"
}
trap cleanup EXIT

mkdir -p "$STAGE/bin" "$OUT_DIR"

cp -p "$BUILD_DIR/dq-comp" "$STAGE/bin/"
cp -p "$BUILD_DIR/dq-run" "$STAGE/bin/"
cp -p "$BUILD_DIR/dqatrun" "$STAGE/bin/"
cp -p "$ROOT_DIR/LICENSE" "$STAGE/LICENSE.txt"
cp -p "$ROOT_DIR/README.md" "$STAGE/README.md"

copy_tracked_paths stdpkg examples docs autotest/tests mkdocs.yml

if [[ "$FULL_RELEASE" == "1" ]]; then
  install_linux_toolchain
fi

if [[ "$FULL_RELEASE" == "1" ]]; then
  cat > "$STAGE/dq-env.sh" <<'EOF'
#!/usr/bin/env bash
DQ_ENV_FILE="${BASH_SOURCE[0]}"
DQ_ROOT="$(cd "$(dirname "$DQ_ENV_FILE")" && pwd)"
export DQ_ROOT
export PATH="$DQ_ROOT/bin:$DQ_ROOT/toolchain/bin:$PATH"
export LD_LIBRARY_PATH="$DQ_ROOT/toolchain/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DQ_LINKER_DRIVER="$DQ_ROOT/toolchain/bin/clang++"
echo "DQ environment ready."
echo "  dq-comp: $DQ_ROOT/bin/dq-comp"
echo "  clang++: $DQ_LINKER_DRIVER"
EOF
  chmod +x "$STAGE/dq-env.sh"
fi

cat > "$STAGE/README-LINUX.txt" <<EOF
DQ Compiler $VERSION for $ARCH Linux$(if [[ "$FULL_RELEASE" == "1" ]]; then printf " full release"; fi)

Quick start:
  1. Extract this archive.
  2. Add the package bin directory to PATH, or run tools directly from bin/.
  3. Run:
       bin/dq-comp --version
$(if [[ "$FULL_RELEASE" == "1" ]]; then cat <<'EOREADME'

For the bundled LLVM/Clang toolchain environment:
  . ./dq-env.sh
  dq-comp --version
EOREADME
fi)

Included:
  bin/dq-comp
  bin/dq-run
  bin/dqatrun
$(if [[ "$FULL_RELEASE" == "1" ]]; then printf "  toolchain/\n  dq-env.sh\n"; fi)
  stdpkg/
  examples/
  docs/
  autotest/tests/
EOF

(
  cd "$STAGE_PARENT"
  tar -czf "$PACKAGE_TMP_TAR" "$PACKAGE_NAME"
)
mv -f "$PACKAGE_TMP_TAR" "$PACKAGE_TAR"

echo "$PACKAGE_TAR"
