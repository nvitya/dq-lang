#!/usr/bin/env python3
"""Package mappings: precedence, project paths, child compiles and cached rebuilds.

Run: python3 tools/test-package-mappings.py --compiler build/dq-comp
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def run(compiler):
    with tempfile.TemporaryDirectory(prefix='dq-package-mappings-') as directory:
        root = Path(directory)

        def write(name, text):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            return path

        def compile(source, *options, error=None):
            result = subprocess.run([str(compiler), '--no-use-sys', str(source), *map(str, options)],
                                    cwd=root, capture_output=True, text=True, timeout=30)
            output = result.stdout + result.stderr
            if error:
                assert result.returncode and error in output, output
            else:
                assert result.returncode == 0, output
            return output

        def board(name, value):
            folder = root / name
            write(f'{name}/board.dq',
                  'use ^/deep/value\n#include "^/config.dqi"\n'
                  'func BoardValue() -> int:\n    return VALUE + EXTRA\nendfunc\n')
            write(f'{name}/deep/value.dq',
                  'use ^/helper\nconst VALUE : int = ROOT_VALUE\n')
            write(f'{name}/helper.dq', f'const ROOT_VALUE : int = {value}\n')
            write(f'{name}/config.dqi', 'const EXTRA : int = 1\n')
            return folder

        first = board('boards/adcif', 10)
        second = board('boards/alternate', 20)
        board('project-packages/board', 30)
        board('cli-packages/board', 40)
        board('last-packages/board', 50)
        main = write('app/main.dq',
                     'use board\n#include "board/public.dqi"\n'
                     'func printf(fmt : ^char, ...) -> int [[external]] ..\n'
                     'func *Main():\n    printf("%d\\n", BoardValue() + PUBLIC)\nendfunc\n')
        # Package-qualified includes must use the mapping as well.
        for folder in [first, second, root / 'project-packages/board',
                       root / 'cli-packages/board', root / 'last-packages/board']:
            (folder / 'public.dqi').write_text('const PUBLIC : int = 2\n')
        project = write('app/app.dqproj',
                        "main = 'main.dq'\npkg_path = '../project-packages'\n"
                        "pkg = 'board:../boards/adcif'\n")
        executable = root / 'result'

        def check(expected, *options, source=project):
            compile(source, '-o', executable, *options)
            actual = subprocess.check_output([str(executable)], text=True).strip()
            assert actual == str(expected), (expected, actual, options)

        check(13)
        artifacts = list((root / 'app/.dqbuild').rglob('*.dqm_if'))
        dependencies = {p: p.stat().st_mtime_ns for p in artifacts
                        if 'pkg' in p.parts and p.stem != 'main'}
        assert len(dependencies) >= 3, dependencies
        check(13)
        assert all(p.stat().st_mtime_ns == stamp for p, stamp in dependencies.items()), dependencies
        check(13, '--pkg_path=cli-packages')  # A CLI root cannot override a mapping.
        check(23, '--pkg=board:boards/alternate', '--pkg_path', 'last-packages')
        check(13, '--pkg=board:boards/alternate', '--pkg', 'board:boards/adcif')
        check(23, '--pkg', 'board:boards/adcif', '--pkg=board:boards/alternate')
        # Mapping changes and returning to a former mapping must invalidate cached imports.
        check(13)
        helper = first / 'helper.dq'
        old_time = helper.stat().st_mtime_ns
        helper.write_text('const ROOT_VALUE : int = 12\n')
        os.utime(helper, ns=(old_time + 1_000_000_000, old_time + 1_000_000_000))
        check(15)

        # Directly compiling a mapped source must retain its logical package identity.
        compile(first / 'deep/value.dq', '-c', '--pkg=board:boards/adcif')
        compile(root / 'project-packages/board/deep/value.dq', '-c',
                '--pkg_path=project-packages', '--pkg=board:boards/adcif',
                error='ModuleRootPackageRequired')
        (root / 'alias').symlink_to(first, target_is_directory=True)
        compile(root / 'alias/deep/value.dq', '-c', '--pkg=board:boards/adcif')
        compile(first / 'deep/value.dq', '-c', '--pkg=board:alias')

        # CLI-only mappings also reach child compilers when there is no project file.
        check(15, '--pkg=board:boards/adcif', source=main)
        # Both new CLI forms and compatibility aliases remain usable.
        check(43, '--pkg_path=cli-packages', source=main)
        check(53, '--pkg_path', 'cli-packages', '--pkg_path=last-packages', source=main)
        check(43, '--pkg-path', 'cli-packages', source=main)
        plain = write('app/plain.dqproj', "main='main.dq'\npackagepath='../project-packages'\n")
        check(33, source=plain)
        check(43, '--pkg_path=cli-packages', source=plain)

        # Includes anchor only the directory part of a mapping to the declaring file.
        fragment = write('config/board.dqproj',
                         "var BOARD_DIR = '../boards/adcif'\npkg = 'board:${BOARD_DIR}'\n")
        write('boards/adcif/settings.dqproj', 'define SELECTED_BOARD = 10\n')
        write('boards/alternate/settings.dqproj', 'define SELECTED_BOARD = 20\n')
        lookup = write('app/lookup.dqproj',
                       "include '../config/board.dqproj'\nvar BOARD = PackagePath('board')\n"
                       "include '${BOARD}/settings.dqproj'\nmain = '${BOARD}/selected.dq'\nlink=false\n")
        write('boards/adcif/selected.dq',
              '#if SELECTED_BOARD <> 10\n#error "wrong project mapping"\n#endif\n')
        write('boards/alternate/selected.dq',
              '#if SELECTED_BOARD <> 20\n#error "wrong CLI mapping"\n#endif\n')
        compile(lookup)
        compile(lookup, '--pkg=board:boards/alternate')
        # Project mappings are textual and repeatable: the last declaration wins.
        fragment.write_text("pkg='board:../boards/adcif'\npkg='board:../boards/alternate'\n")
        compile(lookup)
        # CLI mappings must already be visible before evaluating any project statements.
        cli_lookup = write('app/cli-lookup.dqproj',
                           "var BOARD=PackagePath('board')\ninclude '${BOARD}/settings.dqproj'\n"
                           "main='${BOARD}/selected.dq'\nlink=false\n")
        compile(cli_lookup, '--pkg', 'board:boards/adcif')

        for mapping, error in [('board', 'Expected package mapping'),
                               ('board:', 'Expected package mapping'),
                               (':boards/adcif', 'Invalid package name'),
                               ('bad/name:boards/adcif', 'Invalid package name'),
                               ('1board:boards/adcif', 'Invalid package name'),
                               ('board:absent', 'Mapped package'),
                               ('board:boards/adcif/board.dq', 'Mapped package')]:
            compile(main, '--pkg=' + mapping, '--pkg_path=cli-packages', error=error)
            invalid = write('app/invalid.dqproj', f"main='main.dq'\npkg='{mapping}'\n")
            compile(invalid, '--pkg_path=cli-packages', error=error)
        compile(main, '--pkg', error='Missing value')
        compile(main, '--pkg_path=', error='Missing value')
        # A selected mapping may not fall back to a root for missing modules or includes.
        empty = root / 'empty'
        empty.mkdir()
        compile(main, '--pkg=board:empty', '--pkg_path=cli-packages', error='ModuleNotFound')
        (empty / 'board.dq').write_text('func BoardValue() -> int:\n    return 0\nendfunc\n')
        compile(main, '--pkg=board:empty', '--pkg_path=cli-packages', error='CDirIncFileLoad')
        # Split only the first colon; spaces and additional colons belong to the directory.
        colon_dir = 'boards/with space' if os.name == 'nt' else 'boards/with space:colon'
        folder = board(colon_dir, 60)
        (folder / 'public.dqi').write_text('const PUBLIC : int = 2\n')
        check(63, '--pkg=board:' + str(folder), source=main)
        write('includes/one/value.dqi', 'const INCLUDED : int = 70\n')
        write('includes/two/value.dqi', 'const INCLUDED : int = 80\n')
        write('includes/consumer.dq', '#include "config/value.dqi"\n'
              'func IncludedValue() -> int:\n    return INCLUDED\nendfunc\n')
        included_main = write('app/included.dq',
                              'use ../includes/consumer\n'
                              'func printf(fmt : ^char, ...) -> int [[external]] ..\n'
                              'func *Main():\n    printf("%d\\n", IncludedValue())\nendfunc\n')
        check(70, '--pkg=config:includes/one', source=included_main)
        check(80, '--pkg=config:includes/two', source=included_main)
        check(70, '--pkg=config:includes/one', source=included_main)
        # The root module is not required to be imported before a nested module.
        leaf_main = write('app/leaf.dq',
                          'use board/deep/value\n'
                          'func printf(fmt : ^char, ...) -> int [[external]] ..\n'
                          'func *Main():\n    printf("%d\\n", VALUE)\nendfunc\n')
        check(12, '--pkg=board:boards/adcif', source=leaf_main)
        check(20, '--pkg=board:boards/alternate', source=leaf_main)
        # A package symlink may change without changing any command-line option.
        (root / 'alias').unlink()
        (root / 'alias').symlink_to(first, target_is_directory=True)
        check(12, '--pkg=board:alias', source=leaf_main)
        (root / 'alias').unlink()
        (root / 'alias').symlink_to(second, target_is_directory=True)
        check(20, '--pkg=board:alias', source=leaf_main)
        # Changing an application mapping must not invalidate unrelated runtime packages.
        hosted = write('app/hosted.dq', 'use rtl/sys\n' + main.read_text())
        check(15, '--pkg=board:boards/adcif', source=hosted)
        runtime = {p: p.stat().st_mtime_ns for p in (root / 'app/.dqbuild').rglob('*.dqm_if')
                   if 'rtl' in p.parts}
        assert runtime, 'expected runtime artifacts'
        check(23, '--pkg=board:boards/alternate', source=hosted)
        assert all(p.stat().st_mtime_ns == stamp for p, stamp in runtime.items()), runtime
        print('PASS: package mappings, precedence, relative paths, includes, aliases, diagnostics and rebuilds')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, default=Path(__file__).resolve().parents[1] / 'build/dq-comp')
    run(parser.parse_args().compiler.resolve())
