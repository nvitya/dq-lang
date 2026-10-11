#!/usr/bin/env python3
"""Opt-in Windows cross-compilation tests; requires llvm-mingw and Wine."""

import argparse
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import sys


class WindowsTests:
    def __init__(self, args):
        self.root = Path(__file__).resolve().parent.parent
        self.compiler = Path(args.compiler).resolve()
        self.mingw = Path(args.mingw_root).resolve()
        self.wine = str(Path(args.wine).resolve()) if os.sep in args.wine else args.wine
        self.build = self.root / "build" / "test-windows"
        self.cases = self.build / "cases"
        self.logs = self.build / "logs"
        self.env = dict(os.environ, DQ_MINGW_ROOT=str(self.mingw),
                        WINEPREFIX=str(self.build / "wine"), WINEDEBUG="-all",
                        WINEARCH="win64")
        self.command_count = 0

    def prepare(self):
        if not sys.platform.startswith("linux"):
            raise RuntimeError("The Windows cross-compilation suite requires a Linux host")
        driver = self.mingw / "bin" / "x86_64-w64-mingw32-clang++"
        for tool in (self.compiler, driver, self.mingw / "bin" / "ld.lld", self.wine):
            if not shutil.which(str(tool)):
                raise RuntimeError(f"Required executable is missing: {tool}")
        target = self.mingw / "x86_64-w64-mingw32"
        for name in ("lib/crt2.o", "lib/libucrt.a", "lib/libkernel32.a",
                     "bin/libc++.dll", "bin/libunwind.dll"):
            if not (target / name).is_file():
                raise RuntimeError(f"Required Windows runtime file is missing: {target / name}")
        self.logs.mkdir(parents=True, exist_ok=True)
        # Reset only this suite's cases; retain the dedicated Wine prefix.
        if self.cases.exists():
            shutil.rmtree(self.cases)
        shutil.copytree(self.root / "autotest" / "cross-windows", self.cases)
        for dll in (target / "bin").glob("*.dll"):
            shutil.copy2(dll, self.cases / dll.name)
        self.run([self.wine, "--version"])

    def run(self, args, *, env=None, expected=0, contains=()):
        args = [str(arg) for arg in args]
        self.command_count += 1
        log = self.logs / f"{self.command_count:03}.log"
        try:
            result = subprocess.run(args, cwd=self.cases, env=env or self.env,
                                    capture_output=True, text=True, errors="replace", timeout=120)
        except subprocess.TimeoutExpired as error:
            log.write_text(shlex.join(args) + "\nTIMEOUT\n" +
                           (error.stdout or b"").decode(errors="replace") +
                           (error.stderr or b"").decode(errors="replace"))
            raise RuntimeError(f"Command timed out; see {log}") from error
        output = result.stdout + result.stderr
        log.write_text(shlex.join(args) + "\n" + output)
        if result.returncode != expected:
            raise RuntimeError(f"Exit code {result.returncode}, expected {expected}; see {log}\n{output}")
        for text in contains:
            if text not in output.replace("\r\n", "\n"):
                raise RuntimeError(f"Missing output {text!r}; see {log}\n{output}")
        return output

    def compile(self, source, *options, target="x64-windows", env=None, expected=0, contains=()):
        return self.run([self.compiler, self.cases / source, "--target=" + target,
                         "--build-root", self.cases, "--package-build-root", self.cases,
                         *options], env=env, expected=expected, contains=contains)

    def check_image(self, name, executable=False):
        data = (self.cases / name).read_bytes()
        offset = 0
        if executable:
            if data[:2] != b"MZ":
                raise RuntimeError(f"{name} is not a PE executable")
            offset = struct.unpack_from("<I", data, 60)[0]
            if data[offset:offset + 4] != b"PE\0\0":
                raise RuntimeError(f"{name} has no PE signature")
            offset += 4
            if struct.unpack_from("<H", data, offset + 20)[0] != 0x20B:
                raise RuntimeError(f"{name} is not PE32+")
        if struct.unpack_from("<H", data, offset)[0] != 0x8664:
            raise RuntimeError(f"{name} is not AMD64 COFF")

    def execute(self, name, *, expected=0, contains=()):
        self.check_image(name, executable=True)
        return self.run([self.wine, self.cases / name], expected=expected, contains=contains)

    def test(self):
        self.prepare()
        targets = self.run([self.compiler, "--targets"])
        if targets.count("x86_64-win") != 1:
            raise RuntimeError("Expected exactly one canonical x86_64-win target")
        invalid = dict(self.env, DQ_MINGW_ROOT=str(self.build / "missing-toolchain"))
        invalid.pop("DQ_LINKER_DRIVER", None)
        for alias in ("x64-windows", "x64-win", "x86_64-windows", "x86_64-win"):
            # Object generation must also work without Windows libraries.
            self.compile("defines.dq", "--no-use-sys", "-c", "-o", "defines.obj",
                         target=alias, env=invalid)
            self.check_image("defines.obj")
        print("PASS aliases, definitions and library-free COFF generation", flush=True)

        smoke = ("abi = 42 4.00 mixed 2.50", "errno = 123", "object = 17", "strings = ok")
        for options in (("-g", "-O0"), ()):
            self.compile("smoke.dq", *options)
            self.execute("smoke.exe", contains=(*smoke, "exception = caught"))
        self.compile("smoke.dq", "--no-exceptions", "-o", "explicit-output")
        self.execute("explicit-output", contains=(*smoke, "exceptions = disabled"))
        if (self.cases / "explicit-output.exe").exists():
            raise RuntimeError("The linker appended .exe to an explicit output filename")
        self.compile("uncaught.dq", "-g", "-O0")
        self.execute("uncaught.exe", expected=255,
                     contains=("UNCAUGHT EXCEPTION:", "windows-uncaught", "Backtrace ="))
        print("PASS O0/O1 execution, C ABI, strings, objects and exceptions", flush=True)

        self.compile("smoke.dq", env=invalid, expected=1, contains=("DQ_MINGW_ROOT",))
        override = dict(invalid, DQ_LINKER_DRIVER=str(
            self.mingw / "bin" / "x86_64-w64-mingw32-clang++"))
        self.compile("smoke.dq", env=override)
        discovery = dict(self.env, PATH=str(self.mingw / "bin") + os.pathsep + self.env["PATH"])
        discovery.pop("DQ_MINGW_ROOT", None)
        discovery.pop("DQ_LINKER_DRIVER", None)
        self.compile("smoke.dq", env=discovery)
        self.execute("smoke.exe", contains=smoke)
        # Relocate the compiler to exercise PATH without compiler-relative SDKs.
        original_compiler = self.compiler
        self.compiler = self.cases / "isolated" / "bin" / "dq-comp"
        self.compiler.parent.mkdir(parents=True)
        shutil.copy2(original_compiler, self.compiler)
        try:
            self.compile("smoke.dq", "--pkg-path", self.root / "stdpkg", "-vv",
                         env=discovery, contains=("Link cmd: x86_64-w64-mingw32-clang++",))
        finally:
            self.compiler = original_compiler
        self.execute("smoke.exe", contains=smoke)
        print("PASS linker discovery, invalid root and driver override", flush=True)

        modules = ("module = initialized\nmodules = 1 42",)
        self.compile("project.dqproj")
        self.execute("project.exe", contains=modules)
        # Find the local module object independently of its generated filename.
        module_files = list((self.cases / ".dqbuild" / "x86_64-win" / "local").rglob("*"))
        module_times = {path: path.stat().st_mtime_ns for path in module_files
                        if path.is_file() and path.suffix in (".o", ".dqm_if")
                        and path.stem == "common"}
        if not module_times:
            raise RuntimeError("No compiled common module found")
        self.compile("project.dqproj")
        if any(path.stat().st_mtime_ns != stamp for path, stamp in module_times.items()):
            raise RuntimeError("Unchanged module was unnecessarily recompiled")
        self.execute("project.exe", contains=modules)
        self.compile("modules.dq", "--lto=full", "--build-suffix", "lto", "-o", "lto.exe")
        self.execute("lto.exe", contains=modules)

        native = dict(self.env)
        native.pop("DQ_LINKER_DRIVER", None)
        native.pop("DQ_MINGW_ROOT", None)
        host_target = targets.splitlines()[1].split()[0]
        self.compile("modules.dq", "-o", "native-modules", target=host_target, env=native)
        self.run([self.cases / "native-modules"], env=native, contains=modules)
        if any(path.stat().st_mtime_ns != stamp for path, stamp in module_times.items()):
            raise RuntimeError("Native compilation changed Windows module artifacts")
        if not (self.cases / ".dqbuild" / host_target / "local").is_dir():
            raise RuntimeError("Missing separate native module artifacts")
        self.execute("project.exe", contains=modules)
        print("PASS projects, module initialization/reuse, full LTO and artifact isolation", flush=True)
        print(f"Windows cross-compilation tests passed ({self.command_count} commands). Logs: {self.logs}")


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=str(root / "build" / "dq-comp"))
    parser.add_argument("--mingw-root", default=os.environ.get(
        "DQ_MINGW_ROOT", str(root / "sysroots" / "llvm-mingw-x86_64")))
    parser.add_argument("--wine", default="wine")
    try:
        WindowsTests(parser.parse_args()).test()
    except (RuntimeError, OSError, struct.error) as error:
        print(f"Windows tests failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
