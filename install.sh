#!/usr/bin/env bash
# Install an extracted Linux release. Source builds use `make install` instead.
set -euo pipefail
umask 022

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
PREFIX=/usr/local
DEST_ROOT="${DESTDIR:-}"
STAGE_DIR=
TOOLS=(dq-comp dq-run dq-atrun)
COMPONENTS=(bin stdpkg gcclibs toolchain dq-env.sh LICENSE.txt README-LINUX.txt)

usage() {
  cat <<'EOF'
Usage: ./install.sh [--prefix DIR] [--destdir DIR]

Install the DQ tools and supporting files from this extracted Linux release.
Executables live in PREFIX/lib/dq/bin, with relative symlinks in PREFIX/bin.
The bundled toolchain is installed when present in the archive.

Options:
  --prefix DIR   Installation prefix (default: /usr/local; absolute path).
  --destdir DIR  Stage the installation under DIR (also accepts DESTDIR).
  -h, --help     Show this help.

Examples:
  sudo ./install.sh
  ./install.sh --prefix "$HOME/.local"
  DESTDIR=/tmp/dq-root ./install.sh --prefix /usr/local

Reinstallation replaces DQ's shipped components, including obsolete files.
Additional packages in PREFIX/lib/dq/packages are preserved.
EOF
}

die() {
  printf 'install.sh: %s\n' "$*" >&2
  exit 1
}

parse_options() {
  while (($#)); do
    case "$1" in
      --prefix|--destdir)
        (($# >= 2)) || die "Missing directory after $1."
        [[ -n "$2" && "$2" == /* ]] || die "$1 requires an absolute directory."
        if [[ "$1" == --prefix ]]; then
          PREFIX="$2"
        else
          DEST_ROOT="$2"
        fi
        shift 2
        ;;
      -h|--help) usage; exit 0 ;;
      *) die "Unknown option: $1 (use --help)." ;;
    esac
  done

  PREFIX="$(realpath -m -- "$PREFIX")"
  if [[ -n "$DEST_ROOT" ]]; then
    [[ "$DEST_ROOT" == /* ]] || die "DESTDIR requires an absolute directory."
    DEST_ROOT="$(realpath -m -- "$DEST_ROOT")"
  fi
}

validate_installation() {
  local tool component source_path destination_path
  [[ "$(uname -s)" == Linux ]] || die "This installer requires Linux."

  for tool in "${TOOLS[@]}"; do
    [[ -f "$SOURCE_DIR/bin/$tool" && -x "$SOURCE_DIR/bin/$tool" ]] ||
      die "Missing executable bin/$tool. Use an extracted release, or make install for a source build."
    [[ ! -d "$BIN_DIR/$tool" || -L "$BIN_DIR/$tool" ]] ||
      die "Command path is a directory: $BIN_DIR/$tool"
  done
  for component in stdpkg gcclibs; do
    [[ -d "$SOURCE_DIR/$component" ]] || die "Missing package directory: $component"
  done
  [[ ! -L "$INSTALL_DIR" ]] || die "Installation directory is a symlink: $INSTALL_DIR"

  # Do not let a custom prefix replace the archive we are installing from.
  source_path="$SOURCE_DIR/"
  destination_path="$(realpath -m -- "$INSTALL_DIR")/"
  [[ "$source_path" != "$destination_path"* && "$destination_path" != "$source_path"* ]] ||
    die "The installation directory must not overlap the source archive."
}

prepare_payload() {
  local tool component
  mkdir -p -- "$INSTALL_DIR" "$BIN_DIR"
  STAGE_DIR="$(mktemp -d "$INSTALL_DIR/../.dq-install.XXXXXX")"
  mkdir -- "$STAGE_DIR/bin"

  # Copy everything before changing installed files. Do not preserve archive
  # ownership: installed files belong to the user running this installer.
  for tool in "${TOOLS[@]}"; do
    install -m 755 -- "$SOURCE_DIR/bin/$tool" "$STAGE_DIR/bin/$tool"
  done
  for component in "${COMPONENTS[@]:1}"; do
    if [[ -e "$SOURCE_DIR/$component" ]]; then
      cp -a --no-preserve=ownership -- "$SOURCE_DIR/$component" "$STAGE_DIR/"
    fi
  done

  # Archives may be extracted with a restrictive umask. Make the installation
  # readable to all users while preserving the toolchain's executable bits.
  chmod -R u+rwX,go+rX,go-w -- "$STAGE_DIR"
}

install_payload() {
  local component tool link_target
  # Replace only shipped components. This removes stale standard modules and
  # toolchains on upgrade while keeping separately installed packages intact.
  for component in "${COMPONENTS[@]}"; do
    rm -rf -- "$INSTALL_DIR/$component"
    if [[ -e "$STAGE_DIR/$component" ]]; then
      mv -- "$STAGE_DIR/$component" "$INSTALL_DIR/$component"
    fi
  done

  # Usually ../lib/dq/bin; resolve directory symlinks for custom prefixes too.
  link_target="$(realpath --relative-to="$BIN_DIR" -- "$INSTALL_DIR/bin")"
  for tool in "${TOOLS[@]}"; do
    ln -sfnT -- "$link_target/$tool" "$BIN_DIR/$tool"
  done
}

main() {
  parse_options "$@"
  INSTALL_DIR="$DEST_ROOT${PREFIX%/}/lib/dq"
  BIN_DIR="$DEST_ROOT${PREFIX%/}/bin"
  validate_installation

  # STAGE_DIR is assigned only after mktemp succeeds. Always remove it on exit.
  trap 'if [[ -n "$STAGE_DIR" ]]; then rm -rf -- "$STAGE_DIR"; fi' EXIT
  prepare_payload
  install_payload

  printf 'Installed DQ in %s\nCommands linked in %s\n' "$INSTALL_DIR" "$BIN_DIR"
  if [[ -z "$DEST_ROOT" ]]; then
    printf 'Add %s to PATH if needed, then run: dq-comp --version\n' "$BIN_DIR"
  fi
}

main "$@"
