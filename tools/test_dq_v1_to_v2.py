#!/usr/bin/env python3

import contextlib
import io
import os
from pathlib import Path
import stat
import tempfile
import unittest

import cmsis2dq
import dq_v1_to_v2 as migrator


class MigrateBytesTest(unittest.TestCase):
    def test_all_operator_replacements(self):
        source = (
            b"var p = &value\n"
            b"value = (a AND NOT b) OR (c XOR d)\n"
            b"q = total IDIV count IMOD 4\n"
            b"shifted = a SHL 2 SHR 1\n"
            b"if a != b:\n"
            b"x =AND= 1; x =OR= 2; x =XOR= 3; x =IDIV= 4; x =IMOD= 5\n"
        )
        expected = (
            b"var p = %value\n"
            b"value = (a & ~ b) | (c xor d)\n"
            b"q = total div count mod 4\n"
            b"shifted = a << 2 >> 1\n"
            b"if a <> b:\n"
            b"x &= 1; x |= 2; x =xor= 3; x =div= 4; x =mod= 5\n"
        )
        self.assertEqual(expected, migrator.migrate_bytes(source))

    def test_strings_comments_and_longer_identifiers_are_unchanged(self):
        source = (
            b"var dividend = 1 // & AND != SHL\r\n"
            b"var text = \"& AND != SHL\"\r\n"
            b"var char = '\\\''\r\n"
            b"/* OR NOT\r\nXOR */ var p = &dividend\r\n"
        )
        expected = source.replace(b"&dividend\r\n", b"%dividend\r\n")
        self.assertEqual(expected, migrator.migrate_bytes(source))

    def test_output_is_idempotent(self):
        source = b"result = (&left OR right) AND NOT mask\np = &result\n&anchor\nresult =AND= mask\n"
        once = migrator.migrate_bytes(source)
        self.assertEqual(once, migrator.migrate_bytes(once))
        self.assertEqual(b"result = (%left | right) & ~ mask\np = %result\n%anchor\nresult &= mask\n", once)

    def test_unterminated_regions_fail(self):
        for source in (b'var s = "missing', b"/* missing"):
            with self.subTest(source=source):
                with self.assertRaises(migrator.MigrationError):
                    migrator.migrate_bytes(source)

    def test_no_final_newline_is_preserved(self):
        self.assertEqual(b"return %value", migrator.migrate_bytes(b"return &value"))

    def test_multiline_comment_starts_a_new_statement(self):
        source = b"Call() /* comment\ncontinued */ &value\n"
        self.assertEqual(b"Call() /* comment\ncontinued */ %value\n", migrator.migrate_bytes(source))


class CommandTest(unittest.TestCase):
    def test_recursive_write_check_and_explicit_extension(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            nested = root / "nested"
            nested.mkdir()
            source = nested / "sample.dq"
            ignored = nested / "sample.txt"
            source.write_bytes(b"var p = &value\r\n")
            ignored.write_bytes(b"var p = &value\n")
            os.chmod(source, 0o640)

            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(1, migrator.main(["--check", str(root)]))
                self.assertEqual(0, migrator.main([str(root)]))
                self.assertEqual(0, migrator.main(["--check", str(root)]))
                self.assertEqual(0, migrator.main([str(ignored)]))

            self.assertEqual(b"var p = %value\r\n", source.read_bytes())
            self.assertEqual(b"var p = %value\n", ignored.read_bytes())
            self.assertEqual(0o640, stat.S_IMODE(source.stat().st_mode))

    def test_failed_file_is_not_modified(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "bad.dq"
            original = b"var p = &value\n/* missing"
            source.write_bytes(original)
            with contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(2, migrator.main([str(source)]))
            self.assertEqual(original, source.read_bytes())


class CmsisGeneratorTest(unittest.TestCase):
    def test_c_operators_emit_v2_dq(self):
        converted = cmsis2dq.convert_c_expr_operators("(~A & B) | (C ^ D) / 4 % 3")
        self.assertNotRegex(converted, r"\b(?:AND|OR|NOT|XOR|IDIV|IMOD)\b")
        self.assertIn("~", converted)
        self.assertIn("&", converted)
        self.assertIn("|", converted)
        self.assertIn("xor", converted)
        self.assertIn("div", converted)
        self.assertIn("mod", converted)


if __name__ == "__main__":
    unittest.main()
