#!/usr/bin/env python3
"""Exercise constant imports across configuration switches without cleaning caches."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='build/dq-comp')
    args = parser.parse_args()
    compiler = Path(args.compiler).resolve()
    fixtures = Path(__file__).resolve().parents[1] / 'autotest/tests/modules/constant_use'
    with tempfile.TemporaryDirectory(prefix='dq-constant-use-') as directory:
        root = Path(directory)
        for source in fixtures.rglob('*.dq'):
            target = root / source.relative_to(fixtures)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        executable = root / 'app'

        def build(config, expected):
            subprocess.run([
                str(compiler), str(root / 'main_a.dq'),
                '--pkg-path', str(root / 'packages'),
                '--pkg-path', str(root / f'config_{config}'),
                '--build-root', str(root), '--package-build-root', str(root),
                '--build', 'shared', '-o', str(executable),
            ], check=True, capture_output=True, text=True)
            output = subprocess.check_output([str(executable)], text=True)
            assert output == f'selected = {expected}\nreexport = {expected}\n', output
            artifacts = root / '.dqbuild/shared/pkg'
            return {str(p.relative_to(artifacts)): p.stat().st_mtime_ns
                    for p in artifacts.rglob('*') if p.suffix in ('.o', '.dqm_if')}

        first = build('a', 17)
        assert build('a', 17) == first, 'Unchanged package artifacts were rebuilt'
        # A newly built facade must reject a warm configuration artifact from A.
        for suffix in ('.o', '.dqm_if'):
            (root / f'.dqbuild/shared/pkg/hwapi/uart{suffix}').unlink()
        build('b', 29)
        build('a', 17)

        # Equal-length edit: freshness must notice the timestamp, not just size.
        config = root / 'config_a/hwconfig/hwconfig.dq'
        previous = config.stat()
        config.write_text(config.read_text().replace('uart_a', 'uart_b'))
        os.utime(config, ns=(previous.st_atime_ns, previous.st_mtime_ns + 2_000_000_000))
        changed = build('a', 29)
        assert build('a', 29) == changed, 'Edited configuration did not settle into cache'
    print('Constant-use cache tests passed (reuse, A/B/A switching, constant edit).')


if __name__ == '__main__':
    main()
