#!/usr/bin/env python3
"""
sysroot_export_win.py

Export the development files from a Linux sysroot to another filesystem
(for example an NTFS drive mounted on Linux or a Windows drive under WSL).

Only headers, libraries, startup objects, linker scripts, and pkg-config/CMake
metadata are exported. Target executables, runtime configuration, package
databases, caches, documentation, and other operating-system data are omitted.

The source tree is never modified.

Symlink modes:

  copy
      Dereference symlinks using *sysroot semantics*.  In particular,
      an absolute Linux symlink such as

          /usr/lib/libfoo.so -> /lib/libfoo.so.1

      is resolved as

          <SYSROOT>/lib/libfoo.so.1

      rather than against the host Linux root.

      This is the most portable mode and is recommended when the exported
      tree will be consumed by native Windows tools.

  relative (default)
      Preserve symlinks, rewriting their targets as relative paths to the
      resolved files/directories inside the sysroot. Referenced targets are
      included even when outside the normal development directories, so the
      exported sysroot remains relocatable and links remain usable.

      Whether such symlinks are usable by native Windows applications depends
      on how the destination filesystem/mount represents symlinks.  A POSIX
      symlink created by Linux is not automatically guaranteed to be a
      Windows-native NTFS symlink.

Examples:

    # Robust export for native Windows compilation:
    python3 sysroot_export_win.py \
        /lindata/dev_riscv/sysroot_riscv64_trixie \
        /mnt/windows/dq/sysroots/riscv64-trixie \
        --link-mode copy --clean

    # Preserve/rewrite symlinks (the default):
    python3 sysroot_export_win.py \
        /lindata/dev_riscv/sysroot_riscv64_trixie \
        /mnt/windows/dq/sysroots/riscv64-trixie \
        --clean
"""

from __future__ import annotations

import argparse
import os
import posixpath
import shutil
import stat
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath


MAX_SYMLINK_HOPS = 128


@dataclass
class Stats:
    dirs: int = 0
    files: int = 0
    symlinks: int = 0
    links_materialized: int = 0
    skipped_special: int = 0
    bytes_copied: int = 0


class ExportError(RuntimeError):
    pass


def normalize_rel_posix(path: str) -> str:
    """
    Normalize a POSIX path lexically and reject attempts to escape above root.
    Returns "." for the root itself.
    """
    parts: list[str] = []

    for part in PurePosixPath(path).parts:
        if part in ("", ".", "/"):
            continue
        if part == "..":
            if not parts:
                raise ExportError(f"path escapes sysroot: {path!r}")
            parts.pop()
        else:
            parts.append(part)

    return "/".join(parts) if parts else "."


class SysrootExporter:
    # Keep entire header trees: C++ headers often have no filename extension.
    HEADER_ROOTS = ("include", "usr/include", "usr/local/include")
    LIBRARY_ROOTS = tuple(
        f"{prefix}{name}"
        for prefix in ("", "usr/", "usr/local/")
        for name in ("lib", "lib32", "lib64", "libx32")
    )
    METADATA_ROOTS = ("usr/share/pkgconfig", "usr/local/share/pkgconfig",
                      "usr/share/cmake", "usr/local/share/cmake")
    # These contain target runtime plugins/data rather than link-time inputs.
    RUNTIME_DIRS = {"apt", "dpkg", "gconv", "locale", "firmware", "modules",
                    "systemd", "ssl", "security", "audit", "engines-3",
                    "ossl-modules", "libmariadb3"}

    def __init__(
        self,
        srcroot: Path,
        dstroot: Path,
        link_mode: str,
        verbose: bool,
        dry_run: bool,
    ):
        self.srcroot = srcroot
        self.dstroot = dstroot
        self.link_mode = link_mode
        self.verbose = verbose
        self.dry_run = dry_run
        self.stats = Stats()
        self.exported: dict[Path, bool] = {}
        self.active_dirs: set[str] = set()

    def should_export(self, rel: str, is_dir: bool) -> bool:
        path = PurePosixPath(rel)
        if self.whole_tree(rel):
            return True
        roots = self.HEADER_ROOTS + self.LIBRARY_ROOTS + self.METADATA_ROOTS
        if is_dir and any(path in PurePosixPath(root).parents for root in roots):
            return True
        for root in self.LIBRARY_ROOTS:
            if not path.is_relative_to(root):
                continue
            parts = path.relative_to(root).parts
            if any(p in self.RUNTIME_DIRS or p.startswith(("perl", "python"))
                   for p in parts):
                return False
            if is_dir:
                return True
            return (path.suffix in (".a", ".o", ".so", ".ld", ".lds", ".pc", ".cmake", ".h")
                    or ".so." in path.name or path.name == "specs")
        return False

    def whole_tree(self, rel: str) -> bool:
        path = PurePosixPath(rel)
        return (any(path.is_relative_to(root) for root in self.HEADER_ROOTS + self.METADATA_ROOTS)
                or (any(path.is_relative_to(root) for root in self.LIBRARY_ROOTS)
                    and bool({"include", "include-fixed", "cmake", "pkgconfig"}.intersection(path.parts))))

    def log(self, msg: str) -> None:
        if self.verbose:
            print(msg)

    def src_path(self, rel: str) -> Path:
        if rel == ".":
            return self.srcroot
        return self.srcroot.joinpath(*PurePosixPath(rel).parts)

    def dst_path(self, rel: str) -> Path:
        if rel == ".":
            return self.dstroot
        return self.dstroot.joinpath(*PurePosixPath(rel).parts)

    def resolve_sysroot_rel(self, rel: str) -> str:
        """
        Resolve all symlinks in a source path using the source sysroot as "/".

        This resolver does not allow an absolute symlink to escape to the host
        root and also handles symlinks in intermediate path components.
        """
        rel = normalize_rel_posix(rel)
        todo = [] if rel == "." else list(PurePosixPath(rel).parts)
        resolved: list[str] = []
        hops = 0

        while todo:
            part = todo.pop(0)

            if part in ("", "."):
                continue

            if part == "..":
                if not resolved:
                    raise ExportError(
                        f"symlink resolution escapes sysroot while resolving {rel!r}"
                    )
                resolved.pop()
                continue

            candidate_rel = "/".join(resolved + [part])
            candidate = self.src_path(candidate_rel)

            try:
                st = os.lstat(candidate)
            except FileNotFoundError as exc:
                raise ExportError(
                    f"broken symlink/path while resolving {rel!r}: "
                    f"{candidate_rel!r} does not exist"
                ) from exc

            if stat.S_ISLNK(st.st_mode):
                hops += 1
                if hops > MAX_SYMLINK_HOPS:
                    raise ExportError(
                        f"too many symlink levels while resolving {rel!r}"
                    )

                target = os.readlink(candidate)

                # Absolute target means absolute *inside the sysroot*.
                if target.startswith("/"):
                    resolved = []

                target_parts = list(PurePosixPath(target).parts)
                target_parts = [
                    p for p in target_parts if p not in ("", "/")
                ]

                # Process target before the remaining original path.
                todo = target_parts + todo
            else:
                resolved.append(part)

        return "/".join(resolved) if resolved else "."

    def rewritten_relative_link_target(self, link_rel: str, target: str) -> str:
        """
        Return a relocatable link target for the destination tree.

        Relative links can be retained unchanged.  Absolute Linux links are
        rewritten relative to the link's parent directory.
        """
        link_parent = str(PurePosixPath(link_rel).parent)
        if link_parent == "":
            link_parent = "."

        if not target.startswith("/"):
            # Validate that a relative target does not escape the sysroot.
            normalize_rel_posix(posixpath.join(link_parent, target))
            return target

        target_rel = normalize_rel_posix(target.lstrip("/"))
        rewritten = posixpath.relpath(target_rel, start=link_parent)
        return rewritten

    def ensure_parent(self, path: Path) -> None:
        if self.dry_run:
            return
        path.parent.mkdir(parents=True, exist_ok=True)

    def remove_existing(self, path: Path) -> None:
        if not os.path.lexists(path):
            return

        if self.dry_run:
            return

        # Do not follow destination symlinks while removing.
        if path.is_symlink() or not path.is_dir():
            path.unlink()
        else:
            shutil.rmtree(path)

    def ensure_dir(self, dst: Path) -> None:
        if os.path.lexists(dst) and (dst.is_symlink() or not dst.is_dir()):
            self.remove_existing(dst)

        if not self.dry_run:
            dst.mkdir(parents=True, exist_ok=True)

    def copy_metadata(self, src: Path, dst: Path, follow_symlinks: bool = True) -> None:
        if self.dry_run:
            return
        try:
            shutil.copystat(src, dst, follow_symlinks=follow_symlinks)
        except (OSError, NotImplementedError):
            # Windows-mounted filesystems may not support all POSIX metadata.
            pass

    def export_regular_file(self, src: Path, dst: Path) -> None:
        self.ensure_parent(dst)

        if os.path.lexists(dst):
            if dst.is_symlink() or dst.is_dir():
                self.remove_existing(dst)

        self.log(f"FILE  {src} -> {dst}")

        size = 0
        try:
            size = src.stat().st_size
        except OSError:
            pass

        if not self.dry_run:
            # copy2() may fail on some Windows-mounted filesystems when POSIX
            # metadata cannot be represented.  Copy the data first and treat
            # metadata as best-effort.
            shutil.copyfile(src, dst)
            self.copy_metadata(src, dst)

        self.stats.files += 1
        self.stats.bytes_copied += size

    def export_symlink_relative(self, src_rel: str, src: Path, dst: Path) -> None:
        target = os.readlink(src)
        new_target = self.rewritten_relative_link_target(src_rel, target)

        # Determine directory-ness using sysroot semantics.  This is only
        # significant on platforms where os.symlink uses target_is_directory.
        target_is_directory = False
        try:
            resolved_rel = self.resolve_sysroot_rel(src_rel)
            target_is_directory = self.src_path(resolved_rel).is_dir()
        except ExportError:
            # Preserve broken links in relative mode.
            pass
        else:
            # Link directly to the resolved target: intermediate links may
            # belong to runtime-only directories excluded from this export.
            new_target = posixpath.relpath(resolved_rel, start=str(PurePosixPath(src_rel).parent))
            self.export_entry(resolved_rel, self.dst_path(resolved_rel),
                              required=self.whole_tree(src_rel)
                              or not self.should_export(resolved_rel, target_is_directory))

        self.ensure_parent(dst)
        if os.path.lexists(dst):
            self.remove_existing(dst)

        self.log(f"LINK  {dst} -> {new_target}")

        if not self.dry_run:
            os.symlink(new_target, dst, target_is_directory=target_is_directory)

        self.stats.symlinks += 1

    def export_symlink_copy(self, src_rel: str, dst: Path, required: bool = False) -> None:
        resolved_rel = self.resolve_sysroot_rel(src_rel)

        if resolved_rel == src_rel:
            raise ExportError(f"internal error resolving symlink: {src_rel}")

        resolved_src = self.src_path(resolved_rel)
        self.log(f"LINK* {src_rel} => {resolved_rel}")

        self.stats.links_materialized += 1
        self.export_resolved_source(resolved_rel, resolved_src, dst,
                                    required=required or self.whole_tree(src_rel)
                                    or not self.should_export(resolved_rel, resolved_src.is_dir()))

    def export_resolved_source(self, src_rel: str, src: Path, dst: Path,
                               required: bool = False) -> None:
        """
        Export a source path that is expected not to be a final symlink.
        """
        st = os.lstat(src)

        if stat.S_ISLNK(st.st_mode):
            # Can happen if called from another path after lexical resolution.
            if self.link_mode == "relative":
                self.export_symlink_relative(src_rel, src, dst)
            else:
                self.export_symlink_copy(src_rel, dst, required=required)
            return

        if stat.S_ISDIR(st.st_mode):
            if src_rel in self.active_dirs:
                raise ExportError(f"directory symlink cycle while exporting {src_rel!r}")
            self.active_dirs.add(src_rel)
            self.ensure_dir(dst)
            self.stats.dirs += 1

            with os.scandir(src) as it:
                entries = sorted(it, key=lambda e: e.name)

            for entry in entries:
                child_rel = (
                    entry.name
                    if src_rel == "."
                    else f"{src_rel}/{entry.name}"
                )
                child_dst = dst / entry.name
                self.export_entry(child_rel, child_dst, required=required)

            self.copy_metadata(src, dst)
            self.active_dirs.remove(src_rel)
            return

        if stat.S_ISREG(st.st_mode):
            self.export_regular_file(src, dst)
            return

        self.stats.skipped_special += 1
        print(
            f"warning: skipping special file: {src}",
            file=sys.stderr,
        )

    def export_entry(self, src_rel: str, dst: Path, required: bool = False) -> None:
        required = required or self.whole_tree(src_rel)
        # A header-directory link may require files omitted during an earlier
        # library-only traversal of the same directory.
        if dst in self.exported and (self.exported[dst] or not required):
            return
        src = self.src_path(src_rel)
        st = os.lstat(src)
        is_dir = stat.S_ISDIR(st.st_mode)
        if stat.S_ISLNK(st.st_mode):
            # Only inspect directory links that can contain development files.
            # Runtime links such as usr/lib/ssl/private must never be followed.
            if not required and not self.should_export(src_rel, True):
                return
            try:
                is_dir = self.src_path(self.resolve_sysroot_rel(src_rel)).is_dir()
            except ExportError:
                is_dir = False
        if not required and not self.should_export(src_rel, is_dir):
            return
        self.exported[dst] = required or not is_dir

        if stat.S_ISLNK(st.st_mode):
            if self.link_mode == "relative":
                self.export_symlink_relative(src_rel, src, dst)
            else:
                self.export_symlink_copy(src_rel, dst, required=required)
            return

        self.export_resolved_source(src_rel, src, dst, required=required)

    def export(self) -> None:
        self.ensure_dir(self.dstroot)

        # Export root contents, rather than counting root itself as a copied
        # directory in logs.
        with os.scandir(self.srcroot) as it:
            entries = sorted(it, key=lambda e: e.name)

        for entry in entries:
            rel = entry.name
            self.export_entry(rel, self.dstroot / entry.name)


def clear_destination(dstroot: Path, dry_run: bool) -> None:
    """
    Remove the previous export completely.

    This deliberately favors correctness over incremental update complexity:
    in --link-mode copy, a source directory symlink becomes a real destination
    directory, so determining stale files requires resolving the complete
    materialized view of the sysroot.  Clearing first guarantees an exact
    export.
    """
    if not os.path.lexists(dstroot):
        return

    print(f"CLEAR       : {dstroot}")
    if dry_run:
        return

    if dstroot.is_symlink() or not dstroot.is_dir():
        dstroot.unlink()
    else:
        shutil.rmtree(dstroot)


def is_within(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=(
            "Export only Linux cross-compilation development files (headers, "
            "libraries, startup objects, linker scripts, and build metadata), "
            "handling Linux absolute symlinks correctly."
        )
    )
    p.add_argument("source", help="Linux sysroot directory")
    p.add_argument("destination", help="export destination directory")
    p.add_argument(
        "--link-mode",
        choices=("copy", "relative"),
        default="relative",
        help=(
            "relative (default): preserve symlinks with relocatable relative targets; "
            "copy: materialize symlink targets for filesystems without usable symlinks"
        ),
    )
    p.add_argument(
        "--clean",
        action="store_true",
        help="clear the destination before exporting, producing an exact fresh copy",
    )
    p.add_argument(
        "-n",
        "--dry-run",
        action="store_true",
        help="show actions without changing the destination",
    )
    p.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="show copied files and links",
    )
    return p.parse_args()


def main() -> int:
    args = parse_args()

    srcroot = Path(args.source).expanduser().resolve()
    dstroot = Path(args.destination).expanduser().absolute()

    if not srcroot.is_dir():
        print(f"error: source is not a directory: {srcroot}", file=sys.stderr)
        return 2

    # Avoid recursively exporting into ourselves.
    dst_resolved_parent = dstroot.parent.resolve()
    dst_check = dst_resolved_parent / dstroot.name

    if is_within(dst_check, srcroot):
        print(
            "error: destination must not be inside the source sysroot",
            file=sys.stderr,
        )
        return 2

    print(f"Source      : {srcroot}")
    print(f"Destination : {dstroot}")
    print(f"Link mode   : {args.link_mode}")
    if args.dry_run:
        print("Mode        : DRY RUN")

    if args.link_mode == "relative":
        print(
            "Note: relative mode uses os.symlink(). On a Windows-mounted "
            "filesystem, verify that the mount/driver creates symlinks "
            "which native Windows applications can follow."
        )

    try:
        if args.clean and os.path.lexists(dstroot):
            clear_destination(dstroot, args.dry_run)

        exporter = SysrootExporter(
            srcroot=srcroot,
            dstroot=dstroot,
            link_mode=args.link_mode,
            verbose=args.verbose,
            dry_run=args.dry_run,
        )
        exporter.export()

    except (ExportError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    s = exporter.stats
    print()
    print("Export complete.")
    print(f"  directories       : {s.dirs}")
    print(f"  files             : {s.files}")
    print(f"  symlinks preserved: {s.symlinks}")
    print(f"  links materialized: {s.links_materialized}")
    print(f"  special skipped   : {s.skipped_special}")
    print(f"  bytes copied      : {s.bytes_copied}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
