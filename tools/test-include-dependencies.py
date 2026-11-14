#!/usr/bin/env python3
"""Check nested include freshness without cleaning module caches."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def run(compiler):
    with tempfile.TemporaryDirectory(prefix='dq-include-dependencies-') as directory:
        root = Path(directory)

        def write(name, text):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            return path

        main = write('main.dq',
                     'use board\n'
                     'func printf(fmt : ^char, ...) -> int [[external]] ..\n'
                     'func *Main():\n'
                     '    printf("value = %d\\n", VALUE)\n'
                     'endfunc\n')
        board = write('packages/board/board.dq', '')
        write('packages/board/board.dqh', '#include "platform/config.dqi"\n')
        write('packages/platform/config.dqi',
              '#include_once\n'
              '#include "^/deep/outer.dqi"\n')
        write('packages/platform/deep/outer.dqi', '#include "./inner.dqi"\n')
        inner = write('packages/platform/deep/inner.dqi', 'const VALUE : int = 17\n')
        executable = root / 'app'

        def build(expected):
            result = subprocess.run([
                str(compiler), str(main), '--no-use-sys',
                '--pkg-path', str(root / 'packages'),
                '--build-root', str(root), '--package-build-root', str(root),
                '--build', 'shared', '-o', str(executable),
            ], capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, result.stdout + result.stderr
            output = subprocess.check_output([str(executable)], text=True, timeout=30)
            assert output == f'value = {expected}\n', output
            artifacts = root / '.dqbuild/shared/pkg'
            stamps = {str(p.relative_to(artifacts)): p.stat().st_mtime_ns
                      for p in artifacts.rglob('*') if p.suffix in ('.o', '.dqm_if')}
            assert stamps, 'No package artifacts were generated'
            return stamps

        for include in ('"platform/config.dqi"', r'"\{PLATFORM_INC}"', 'header'):
            # Repeated includes also exercise include_once and dependency deduplication.
            board.write_text('const PLATFORM_INC : rostr = "platform/config.dqi"\n'
                             f'#include {include}\n#include {include}\n')
            inner.write_text('const VALUE : int = 17\n')
            first = build(17)
            assert build(17) == first, 'Unchanged package artifacts were rebuilt'

            # Equal-length edit with a deterministic timestamp change.
            previous = inner.stat()
            inner.write_text('const VALUE : int = 29\n')
            os.utime(inner, ns=(previous.st_atime_ns, previous.st_mtime_ns + 2_000_000_000))
            changed = build(29)
            assert changed != first, 'Nested include edit did not rebuild the package'
            assert build(29) == changed, 'Edited package did not settle into cache'

    print('PASS: nested include freshness, cache reuse, interpolation, headers and include_once')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, default=Path(__file__).resolve().parents[1] / 'build/dq-comp')
    run(parser.parse_args().compiler.resolve())
