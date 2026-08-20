#!/usr/bin/env python3
"""Migrate DQ source files from the V1 operator syntax to V2."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import stat
import sys
import tempfile


DQ_SUFFIXES = {".dq", ".dqh", ".dqi"}

COMPOUND_OPERATORS = (
    (b"=IDIV=", b"=div="),
    (b"=IMOD=", b"=mod="),
    (b"=AND=", b"&="),
    (b"=XOR=", b"=xor="),
    (b"=OR=", b"|="),
)

WORD_OPERATORS = {
    b"AND": b"&",
    b"OR": b"|",
    b"NOT": b"~",
    b"XOR": b"xor",
    b"IDIV": b"div",
    b"IMOD": b"mod",
    b"SHL": b"<<",
    b"SHR": b">>",
}

NON_TERMINATING_WORDS = {
    b"and", b"or", b"not", b"is", b"as", b"xor", b"div", b"mod",
    b"if", b"elif", b"while", b"for", b"step", b"return", b"raise",
    b"delete", b"new", b"var", b"const",
}


class MigrationError(Exception):
    """A source file cannot be migrated safely."""

    def __init__(self, message: str, offset: int):
        super().__init__(message)
        self.offset = offset


def _is_identifier_start(value: int) -> bool:
    return value == ord("_") or ord("A") <= value <= ord("Z") or ord("a") <= value <= ord("z")


def _is_identifier_continue(value: int) -> bool:
    return _is_identifier_start(value) or ord("0") <= value <= ord("9")


def migrate_bytes(source: bytes) -> bytes:
    """Return V2 source while preserving strings, comments, and all other bytes."""
    result = bytearray()
    index = 0
    length = len(source)
    can_end_expression = False
    expression_nesting = 0

    while index < length:
        if source.startswith(b"//", index):
            end = index + 2
            while end < length and source[end] not in (ord("\r"), ord("\n")):
                end += 1
            result.extend(source[index:end])
            index = end
            continue

        if source.startswith(b"/*", index):
            end = source.find(b"*/", index + 2)
            if end < 0:
                raise MigrationError("unterminated block comment", index)
            end += 2
            result.extend(source[index:end])
            if 0 == expression_nesting and (b"\r" in source[index:end] or b"\n" in source[index:end]):
                can_end_expression = False
            index = end
            continue

        if source[index] in (ord("'"), ord('"')):
            quote = source[index]
            start = index
            index += 1
            while index < length:
                value = source[index]
                if value in (ord("\r"), ord("\n")):
                    raise MigrationError("unterminated quoted string", start)
                if value == ord("\\"):
                    if index + 1 >= length or source[index + 1] in (ord("\r"), ord("\n")):
                        raise MigrationError("unterminated quoted string", start)
                    index += 2
                    continue
                index += 1
                if value == quote:
                    break
            else:
                raise MigrationError("unterminated quoted string", start)
            result.extend(source[start:index])
            can_end_expression = True
            continue

        replacement = None
        for old, new in COMPOUND_OPERATORS:
            if source.startswith(old, index):
                replacement = (old, new)
                break
        if replacement:
            old, new = replacement
            result.extend(new)
            index += len(old)
            can_end_expression = False
            continue

        if source.startswith(b"!=", index):
            result.extend(b"<>")
            index += 2
            can_end_expression = False
            continue

        if source[index] == ord("&"):
            # V1 uses unary &, while V2 uses binary &. Context makes rerunning
            # the migration safe: after an operand this is already a V2 token.
            result.extend(b"&" if can_end_expression else b"%")
            index += 1
            can_end_expression = False
            continue

        if _is_identifier_start(source[index]):
            end = index + 1
            while end < length and _is_identifier_continue(source[end]):
                end += 1
            word = source[index:end]
            result.extend(WORD_OPERATORS.get(word, word))
            can_end_expression = word not in WORD_OPERATORS and word not in NON_TERMINATING_WORDS
            index = end
            continue

        value = source[index]
        result.append(value)
        if value in (ord("\r"), ord("\n")):
            if 0 == expression_nesting:
                can_end_expression = False
        elif value not in b" \t":
            if ord("0") <= value <= ord("9") or value in b")]}":
                can_end_expression = True
            elif value == ord("^"):
                # In expressions ^ is a postfix dereference. Prefix pointer
                # type syntax does not create a context in which & can follow.
                can_end_expression = True
            else:
                can_end_expression = False
            if value in b"([":
                expression_nesting += 1
            elif value in b")]" and expression_nesting:
                expression_nesting -= 1
        index += 1

    return bytes(result)


def _location(source: bytes, offset: int) -> tuple[int, int]:
    line = source.count(b"\n", 0, offset) + 1
    line_start = source.rfind(b"\n", 0, offset)
    return line, offset - line_start


def _atomic_write(path: Path, data: bytes, mode: int) -> None:
    descriptor, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        os.chmod(temp_name, stat.S_IMODE(mode))
        if hasattr(os, "fchmod"):
            os.fchmod(descriptor, stat.S_IMODE(mode))
        with os.fdopen(descriptor, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temp_name, path)
    except BaseException:
        try:
            os.close(descriptor)
        except OSError:
            pass
        try:
            os.unlink(temp_name)
        except OSError:
            pass
        raise


def migrate_file(path: Path, check: bool) -> bool:
    source = path.read_bytes()
    try:
        migrated = migrate_bytes(source)
    except MigrationError as error:
        line, column = _location(source, error.offset)
        raise MigrationError(f"{path}:{line}:{column}: {error}", error.offset) from error

    if migrated == source:
        return False
    if not check:
        _atomic_write(path, migrated, path.stat().st_mode)
    return True


def collect_files(paths: list[Path]) -> list[Path]:
    files: dict[Path, Path] = {}
    for path in paths:
        if not path.exists():
            raise FileNotFoundError(path)
        if path.is_symlink():
            raise MigrationError(f"symbolic links are not supported: {path}", 0)
        if path.is_file():
            files[path.resolve()] = path
            continue

        for root, dirnames, filenames in os.walk(path, followlinks=False):
            root_path = Path(root)
            dirnames[:] = [
                name for name in dirnames
                if name != ".git" and not (root_path / name).is_symlink()
            ]
            for name in filenames:
                candidate = root_path / name
                if candidate.suffix in DQ_SUFFIXES and not candidate.is_symlink():
                    files[candidate.resolve()] = candidate
    return [files[key] for key in sorted(files, key=lambda item: str(item))]


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="report files requiring migration without modifying them; exit 1 if any are found",
    )
    parser.add_argument("paths", metavar="PATH", nargs="+", type=Path, help="source file or directory")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        files = collect_files(args.paths)
    except (OSError, MigrationError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    changed = 0
    failed = 0
    for path in files:
        try:
            if migrate_file(path, args.check):
                changed += 1
                action = "needs migration" if args.check else "updated"
                print(f"{action}: {path}")
        except (OSError, MigrationError) as error:
            failed += 1
            print(f"error: {error}", file=sys.stderr)

    if failed:
        print(f"migration failed: {failed} file(s)", file=sys.stderr)
        return 2

    action = "would update" if args.check else "updated"
    print(f"{action} {changed} of {len(files)} file(s)")
    return 1 if args.check and changed else 0


if __name__ == "__main__":
    raise SystemExit(main())
