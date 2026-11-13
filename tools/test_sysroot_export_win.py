#!/usr/bin/env python3

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from tools.sysroot_export import ExportError, SysrootExporter


class SysrootExportTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.destination = self.root / "export"
        for name in (
            "usr/include/stdio.h", "usr/include/c++/14/vector",
            "usr/lib/target-linux-gnu/libc.so.6",
            "usr/lib/target-linux-gnu/libgcc_s.so.1",
            "usr/lib/target-linux-gnu/libfoo.so.1.2",
            "usr/lib/target-linux-gnu/libunused.so.1",
            "usr/lib/target-linux-gnu/libfoo.a.2",
            "usr/lib/target-linux-gnu/foo.lib.3",
            "usr/lib/target-linux-gnu/foo.lib",
            "usr/lib/target-linux-gnu/crti.obj",
            "usr/lib/target-linux-gnu/crt1.o",
            "usr/lib/gcc/target-linux-gnu/14/libgcc.a",
            "usr/lib/gcc/target-linux-gnu/14/include/stddef.h",
            "usr/lib/gcc/target-linux-gnu/14/include-fixed/limits.h",
            "usr/lib/gcc/target-linux-gnu/14/specs",
            "usr/lib/target-linux-gnu/pkgconfig/libc.pc",
            "usr/lib/target-linux-gnu/cmake/libc/config.cmake",
            "usr/share/pkgconfig/common.pc", "usr/local/include/custom.h",
            "usr/local/lib/libcustom.a", "usr/lib64/ld-linux.so.2",
            "usr/lib/target-linux-gnu/custom/layout.ld",
            "usr/bin/sh", "etc/ssl/private/key", "var/cache/apt/cache",
            "usr/share/doc/manual", "usr/lib/locale/locale-archive",
            "usr/lib/target-linux-gnu/perl-base/auto/module.so",
            "usr/lib/target-linux-gnu/gconv/module.so",
            "usr/lib/target-linux-gnu/security/module.so",
            "usr/lib/gcc/target-linux-gnu/14/cc1",
            "etc/os-release", "usr/lib/runtime-data",
            "usr/lib/project/extensionless",
            "opt/sdk/headers/extensionless", "opt/sdk/libexternal.so.1",
            "opt/sdk/libdir.so",
        ):
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(name)
        for name, target in {
            "lib": "usr/lib",
            "bin": "usr/bin",
            "usr/lib/ssl": "/etc/ssl",
            "usr/lib/os-release": "/etc/os-release",
            "usr/lib/target-linux-gnu/libc-link.so": "/lib/target-linux-gnu/libc.so.6",
            "usr/lib/target-linux-gnu/libfoo.so": "libfoo.so.1.2",
            "usr/include/sdk": "/opt/sdk/headers",
            "usr/include/project": "/usr/lib/project",
            "usr/lib/sdk": "/opt/sdk",
            "usr/lib/libexternal.so": "/etc/alternatives/libexternal.so",
            "etc/alternatives/libexternal.so": "/opt/sdk/libexternal.so.1",
        }.items():
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.symlink_to(target)
        for name, script in {
            "usr/lib/target-linux-gnu/libc.so":
                "/* GNU ld script */\nGROUP (/lib/target-linux-gnu/libc.so.6)\n",
            "usr/lib/gcc/target-linux-gnu/14/libgcc_s.so":
                "GROUP ( libgcc_s.so.1 -lgcc )\n",
        }.items():
            (self.source / name).write_text(script)

    def export(self, mode="copy", dry_run=False):
        exporter = SysrootExporter(self.source, self.destination, mode, False, dry_run)
        exporter.export()
        return exporter

    def test_only_dq_linker_inputs_are_exported(self):
        self.export()
        for name in (
            "usr/lib/target-linux-gnu/libc.so", "usr/lib/target-linux-gnu/crt1.o",
            "usr/lib/gcc/target-linux-gnu/14/libgcc.a",
            "usr/local/lib/libcustom.a", "usr/lib/target-linux-gnu/custom/layout.ld",
            "usr/lib/libexternal.so", "usr/lib/target-linux-gnu/foo.lib",
            "usr/lib/sdk/libdir.so",
            "usr/lib/target-linux-gnu/crti.obj", "usr/lib/target-linux-gnu/libfoo.so",
        ):
            with self.subTest(name=name):
                self.assertTrue((self.destination / name).is_file())
        for name in ("bin", "etc", "var", "usr/bin", "usr/share", "usr/include",
                     "usr/local/include", "usr/lib/gcc/target-linux-gnu/14/include",
                     "usr/lib/gcc/target-linux-gnu/14/include-fixed",
                     "usr/lib/gcc/target-linux-gnu/14/specs",
                     "usr/lib/target-linux-gnu/pkgconfig", "usr/lib/target-linux-gnu/cmake",
                     "usr/lib/ssl", "usr/lib/os-release", "usr/lib/locale",
                     "usr/lib/target-linux-gnu/perl-base",
                     "usr/lib/target-linux-gnu/gconv", "usr/lib/runtime-data",
                     "usr/lib/target-linux-gnu/libfoo.so.1.2",
                     "usr/lib/target-linux-gnu/libunused.so.1",
                     "usr/lib/target-linux-gnu/libfoo.a.2", "usr/lib/target-linux-gnu/foo.lib.3",
                     "usr/lib/sdk/headers/extensionless",
                     "usr/lib64/ld-linux.so.2", "lib/gcc", "lib/target-linux-gnu/crt1.o"):
            with self.subTest(name=name):
                self.assertFalse((self.destination / name).exists())
        self.assertFalse(any(p.is_symlink() for p in self.destination.rglob("*")))
        self.assertEqual("opt/sdk/libexternal.so.1",
                         (self.destination / "usr/lib/libexternal.so").read_text())
        self.assertEqual("/opt/sdk/headers", str((self.source / "usr/include/sdk").readlink()))

    def test_only_linker_script_required_versioned_files_are_kept(self):
        self.export()
        for name in ("lib/target-linux-gnu/libc.so.6", "usr/lib/target-linux-gnu/libgcc_s.so.1"):
            self.assertTrue((self.destination / name).is_file(), name)
        # The .so link is materialized; its numbered source target is omitted.
        self.assertEqual("usr/lib/target-linux-gnu/libfoo.so.1.2",
                         (self.destination / "usr/lib/target-linux-gnu/libfoo.so").read_text())
        self.assertFalse((self.destination / "usr/lib/target-linux-gnu/libc.so.6").exists())

    def test_relative_mode_keeps_library_targets_usable(self):
        self.export("relative")
        self.assertTrue((self.destination / "lib").is_symlink())
        for path in self.destination.rglob("*"):
            if path.is_symlink():
                self.assertFalse(path.readlink().is_absolute())
                self.assertTrue(path.exists(), path)
        self.assertFalse((self.destination / "usr/include").exists())

    def test_script_includes_copy_only_named_dependencies(self):
        scripts = self.source / "usr/lib/target-linux-gnu"
        (scripts / "main.ld").write_text('INCLUDE "nested.lds"\n/* INPUT(libunused.so.1) */\n')
        (scripts / "nested.lds").write_text('INPUT ( "libfoo.so.1.2" )\n')
        self.export()
        self.assertTrue((self.destination / "usr/lib/target-linux-gnu/libfoo.so.1.2").is_file())
        self.assertFalse((self.destination / "usr/lib/target-linux-gnu/libunused.so.1").exists())

    def test_missing_script_dependency_reports_error(self):
        (self.source / "usr/lib/missing.ld").write_text("INPUT(libmissing.so.1)\n")
        with self.assertRaisesRegex(ExportError, "cannot resolve linker script input 'libmissing.so.1'"):
            self.export()

    def test_dry_run_does_not_create_destination(self):
        exporter = self.export(dry_run=True)
        self.assertGreater(exporter.stats.files, 0)
        self.assertFalse(self.destination.exists())

    def test_cli_defaults_to_copy_and_help_matches(self):
        script = Path(__file__).with_name("sysroot_export.py")
        result = subprocess.run([sys.executable, str(script), str(self.source),
                                 str(self.destination)], capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertFalse(any(p.is_symlink() for p in self.destination.rglob("*")))
        result = subprocess.run([sys.executable, str(script), "--help"],
                                capture_output=True, text=True, check=True)
        self.assertIn("copy (default)", result.stdout)
        self.assertNotIn("relative (default)", result.stdout)


if __name__ == "__main__":
    unittest.main()
