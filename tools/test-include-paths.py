#!/usr/bin/env python3
"""Check package-root includes across nested fragments and restored contexts."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def run(compiler):
    with tempfile.TemporaryDirectory(prefix='dq-include-paths-') as directory:
        root = Path(directory)

        def write(name, text):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            return path

        def compile(source, *paths, fails=False, error=None):
            args = [str(compiler), '--no-use-sys', '-c', str(source)]
            for path in paths:
                args += ['--pkg-path', str(path)]
            result = subprocess.run(args, cwd=root, capture_output=True, text=True, timeout=30)
            output = result.stdout + result.stderr
            assert (result.returncode != 0) == fails, output
            if fails:
                assert 'include' in output.lower(), output
            if error:
                assert error in output, output

        packages = root / 'packages'
        board = write('packages/board/board.dq',
                      '#include "platform/deep/select.dqi"\n'
                      '#include "^/board_only.dqi"\n'
                      'const TOTAL : int = PLATFORM + OTHER + RESTORED + BOARD\n')
        write('packages/platform/deep/select.dqi',
              '#include "^/registers.dqi"\n'
              '#include "other/deep/select.dqi"\n'
              '#include "^/restored.dqi"\n')
        write('packages/platform/registers.dqi', 'const PLATFORM : int = 1\n')
        write('packages/platform/restored.dqi', 'const RESTORED : int = 2\n')
        write('packages/other/deep/select.dqi', '#include "^/registers.dqi"\n')
        write('packages/other/registers.dqi', 'const OTHER : int = 3\n')
        write('packages/board/board_only.dqi', 'const BOARD : int = 4\n')
        compile(board, packages)
        # Also exercise the selected board as a dependency, with explicit module identity.
        main = write('app/main.dq', 'use board\nconst VALUE : int = TOTAL\n')
        compile(main, packages)
        compile(main, packages)  # cached interface
        (root / 'alias').symlink_to(packages, target_is_directory=True)
        compile(board, root / 'alias')
        standalone = write('standalone/main.dq', '#include "^/local.dqi"\n')
        write('standalone/local.dqi', 'const LOCAL : int = 1\n')
        compile(standalone, fails=True)
        # Local relative includes remain usable outside packages.
        standalone.write_text('#include "./local.dqi"\n')
        compile(standalone)
        # Bare filenames diagnose the required prefix even when the file exists.
        standalone.write_text('#include "local.dqi"\n')
        compile(standalone, fails=True, error="Local include paths must start with './'")
        # A local directory cannot shadow a package-qualified include.
        write('standalone/platform/config.dqi', '#error "local shadow selected"\n')
        write('packages/platform/config.dqi', 'const SELECTED : int = 7\n')
        standalone.write_text('#include "platform/config.dqi"\nconst VALUE : int = SELECTED\n')
        compile(standalone, packages)
        # Select the package first: a missing file cannot fall through to an older package.
        override = root / 'override'
        write('override/platform/other.dqi', 'const UNUSED : int = 0\n')
        compile(standalone, packages, override, fails=True, error='CDirIncFileLoad')
        write('override/platform/config.dqi', 'const SELECTED : int = 9\n')
        write('packages/platform/config.dqi', '#error "lower priority package selected"\n')
        compile(standalone, packages, override)
        # Explicit local paths still select the local directory.
        write('standalone/platform/config.dqi', 'const LOCAL : int = 3\n')
        standalone.write_text('#include "./platform/config.dqi"\nconst VALUE : int = LOCAL\n')
        compile(standalone, packages, override)
        # Parent-relative includes and the special header form are unchanged.
        child = write('standalone/deep/main.dq', '#include "../local.dqi"\n#include header\n')
        write('standalone/deep/main.dqh', 'const HEADER : int = 5\n')
        compile(child)
        print('PASS: include roots, context restoration, package precedence, missing files, explicit local paths and headers')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, default=Path(__file__).resolve().parents[1] / 'build/dq-comp')
    run(parser.parse_args().compiler.resolve())
