#!/usr/bin/env python3
"""Regression tests for package roots and unrestricted relative imports."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def run(compiler):
    with tempfile.TemporaryDirectory(prefix='dq-module-paths-') as directory:
        root = Path(directory)

        def write(name, text):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            return path

        def compile(source, *options, error=None):
            result = subprocess.run(
                [str(compiler), '--no-use-sys', '--diagnostic-format=jsonl',
                 str(source), *map(str, options)], cwd=root, capture_output=True,
                text=True, timeout=30)
            output = result.stdout + result.stderr
            if error:
                assert result.returncode and error in output, output
            else:
                assert result.returncode == 0, output
            return output

        nested = write('packages/lib/deep/module.dq',
                       'use ^/helper\nconst VALUE : int = ROOT_VALUE\n')
        write('packages/lib/helper.dq', 'const ROOT_VALUE : int = 7\n')
        paths = ['--pkg-path', root / 'packages']
        compile(nested, '-c', *paths)
        # Search-directory symlinks and package symlinks both preserve identity.
        (root / 'search-alias').symlink_to(root / 'packages', target_is_directory=True)
        compile(nested, '-c', '--pkg-path', root / 'search-alias')
        (root / 'links').mkdir()
        (root / 'links/lib').symlink_to(root / 'packages/lib', target_is_directory=True)
        compile(nested, '-c', '--pkg-path', root / 'links')
        (root / 'source-alias.dq').symlink_to(nested)
        compile(root / 'source-alias.dq', '-c', '--pkg-path', root / 'links')

        # Last search path wins; the shadowed source is not the active package.
        override = write('override/lib/deep/module.dq', nested.read_text())
        write('override/lib/helper.dq', 'const ROOT_VALUE : int = 19\n')
        overrides = [*paths, '--pkg-path', root / 'override']
        compile(override, '-c', *overrides)
        compile(nested, '-c', *overrides, error='ModuleRootPackageRequired')
        compile(nested, '-c', '--pkg-path', root / 'override', *paths)

        outside = write('packages/lib_extra/module.dq',
                        'use ^/helper\n')
        # lib_extra is outside the symlinked lib package, despite its prefix.
        compile(outside, '-c', '--pkg-path', root / 'links', error='ModuleRootPackageRequired')
        missing = write('standalone/deep/missing.dq',
                        'use ../../../../../../../../../../missing\n')
        compile(missing, '-c', error='ModuleNotFound')

        # External modules with identical basenames need distinct artifacts.
        # A transitive import also exercises cached source and link resolution.
        write('left/value.dq', 'func Value() -> int:\n    return 11\nendfunc\n')
        write('app/deep/shared.dq', 'func Shared() -> int:\n    return 0\nendfunc\n')
        write('left/deep/api.dq',
              'use ../value\nuse ../../app/deep/shared\n'
              'func Left() -> int:\n    return Value() + Shared()\nendfunc\n')
        write('right/value.dq', 'func Right() -> int:\n    return 23\nendfunc\n')
        main = write('app/deep/main.dq',
                     'use ../../left/deep/api\nuse ../../right/value\nuse ./shared\n'
                     'func printf(fmt : ^char, ...) -> int [[external]] ..\n'
                     'func *Main():\n    printf("%d\\n", Left() + Right() + Shared())\nendfunc\n')
        executable = root / 'app/test'
        compile(main, '-o', executable)
        assert subprocess.check_output([str(executable)], text=True).strip() == '34'
        artifacts = list((main.parent / '.dqbuild').rglob('*.dqm_if'))
        dependencies = {p: p.stat().st_mtime_ns for p in artifacts if p.stem != 'main'}
        assert len(dependencies) >= 3, dependencies
        compile(main, '-o', executable)
        assert all(p.stat().st_mtime_ns == stamp for p, stamp in dependencies.items()), dependencies
        assert subprocess.check_output([str(executable)], text=True).strip() == '34'
        write('left/value.dq', 'func Value() -> int:\n    return 12\nendfunc\n')
        compile(main, '-o', executable)
        assert subprocess.check_output([str(executable)], text=True).strip() == '35'
        print('PASS: package discovery, symlinks, overrides, parent traversal, external artifacts and rebuilds')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, default=Path(__file__).resolve().parents[1] / 'build/dq-comp')
    run(parser.parse_args().compiler.resolve())
