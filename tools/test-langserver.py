#!/usr/bin/env python3
"""Compiler/LSP regressions for import diagnostics and unsaved source aliases.

Run: python3 tools/test-langserver.py --compiler build/dq-comp
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def run(compiler, mode='roots'):
    with tempfile.TemporaryDirectory(prefix='dq-lsp-symlink-') as directory:
        root = Path(directory)
        for name in ['workspace', 'physical', 'packages']:
            (root / name).mkdir()
        workspace = root / 'workspace'
        (workspace / 'lib').symlink_to(root / 'physical', target_is_directory=True)
        (root / 'packages/lib').symlink_to(root / 'physical', target_is_directory=True)
        (root / 'physical/nested').mkdir()
        (root / 'physical/helper.dq').write_text('const ROOT_VALUE : int = 7\n')
        original = 'use ^/helper\nconst VALUE : int = ROOT_VALUE\nimplementation\n'
        edited = 'use ^/helper\nconst UNSAVED : int = ROOT_VALUE + 2\nimplementation\n'
        (root / 'physical/nested/editor.dq').write_text(original)
        (root / 'physical/broken.dq').write_text('use missing_board_package\n')
        broken = (workspace / 'broken.dq').as_uri()
        (workspace / 'main.dq').write_text('use lib/nested/editor\nvar result : int = UNSAVED\n')
        project = workspace / 'app.dqproj'
        project_text = "main = 'main.dq'\ntarget = 'x86_64-linux'\nlink = false\n"
        arguments = []
        if mode == 'roots':
            project_text += "pkg_path = '../packages'\n"
        else:
            (root / 'wrong').mkdir()
            (root / 'physical/settings.dqproj').write_text('define MAPPED_LIBRARY\n')
            if mode == 'project-mapping':
                project_text += "pkg = 'lib:../physical'\n"
            else:
                project_text += "pkg = 'lib:../wrong'\n"
                arguments = ['--pkg=lib:../physical']
            project_text += "var LIB=PackagePath('lib')\ninclude '${LIB}/settings.dqproj'\n"
        project.write_text(project_text)
        main = (workspace / 'main.dq').as_uri()
        alias = (workspace / 'lib/nested/editor.dq').as_uri()
        package = (root / 'packages/lib/nested/editor.dq').as_uri()
        messages = []
        def send(method, params=None, request=None):
            message = {'jsonrpc': '2.0', 'method': method, 'params': params}
            if request is not None:
                message['id'] = request
            messages.append(message)
        def open_document(uri, text):
            send('textDocument/didOpen', {'textDocument': {'uri': uri, 'languageId': 'dq', 'version': 1, 'text': text}})
        def change_document(uri, text, version):
            send('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version}, 'contentChanges': [{'text': text}]})
        send('initialize', {'processId': None, 'capabilities': {}}, 1)
        send('initialized', {})
        open_document(broken, 'use lib/broken\n')
        send('textDocument/documentSymbol', {'textDocument': {'uri': broken}}, 12)
        send('textDocument/didClose', {'textDocument': {'uri': broken}})
        open_document(main, (workspace / 'main.dq').read_text())
        open_document(alias, edited)
        send('textDocument/documentSymbol', {'textDocument': {'uri': alias}}, 2)
        send('textDocument/definition', {'textDocument': {'uri': alias},
             'position': {'line': 1, 'character': edited.splitlines()[1].index('ROOT_VALUE')}}, 11)
        send('textDocument/definition', {'textDocument': {'uri': main}, 'position': {'line': 1, 'character': 21}}, 3)
        # A second URI for the same physical file must share the source identity.
        open_document(package, original)
        change_document(alias, edited, 2)
        send('textDocument/documentSymbol', {'textDocument': {'uri': package}}, 4)
        send('textDocument/completion', {'textDocument': {'uri': main}, 'position': {'line': 1, 'character': 21}}, 5)
        # Diagnostics must reach both spellings, even though workers report one.
        change_document(package, 'const UNSAVED : int =\nimplementation\n', 2)
        send('textDocument/documentSymbol', {'textDocument': {'uri': alias}}, 6)
        change_document(alias, edited, 3)
        send('textDocument/documentSymbol', {'textDocument': {'uri': main}}, 7)
        send('textDocument/definition', {'textDocument': {'uri': main}, 'position': {'line': 1, 'character': 21}}, 9)
        change_document(package, edited, 4)
        send('textDocument/definition', {'textDocument': {'uri': main}, 'position': {'line': 1, 'character': 21}}, 10)
        send('shutdown', None, 8)
        send('exit')
        wire = b''
        for message in messages:
            body = json.dumps(message).encode()
            wire += f'Content-Length: {len(body)}\r\n\r\n'.encode() + body
        process = subprocess.run([str(compiler), '--langserver', '--no-use-sys', str(project), *arguments],
                                 input=wire, capture_output=True, cwd=workspace, timeout=60)
        assert process.returncode == 0, process.stderr.decode()
        output = process.stdout
        replies = {}
        diagnostics = {}
        snapshots = {}
        while output:
            header, output = output.split(b'\r\n\r\n', 1)
            length = int(header.split(b':')[1].strip())
            message = json.loads(output[:length])
            output = output[length:]
            assert 'error' not in message, message
            if message.get('method') == 'textDocument/publishDiagnostics':
                diagnostics[message['params']['uri']] = message['params']['diagnostics']
            if 'id' in message:
                replies[message['id']] = message['result']
                snapshots[message['id']] = dict(diagnostics)
        [import_error] = snapshots[12][broken]
        assert import_error['code'] == 'UseRegenFailed', import_error
        assert 'Package "missing_board_package" was not found' in import_error['message'], import_error
        assert any(s['name'] == 'UNSAVED' for s in replies[2]), replies[2]
        assert replies[11] and replies[11][0]['uri'] == (root / 'physical/helper.dq').as_uri(), replies[11]
        assert replies[3] and replies[3][0]['uri'] == alias, replies[3]
        assert not snapshots[2][main] and not snapshots[2][alias], snapshots[2]
        assert any(s['name'] == 'UNSAVED' for s in replies[4]), replies[4]
        assert all(not errors for errors in snapshots[4].values()), snapshots[4]
        items = replies[5].get('items', []) if isinstance(replies[5], dict) else replies[5]
        assert any(item['label'] == 'UNSAVED' for item in items), items
        assert snapshots[6][alias] and snapshots[6][package], snapshots[6]
        assert all(not errors for errors in snapshots[7].values()), snapshots[7]
        assert replies[9] and replies[9][0]['uri'] == alias, replies[9]
        assert replies[10] and replies[10][0]['uri'] == package, replies[10]
        assert (root / 'physical/nested/editor.dq').read_text() == original
        print(f'PASS ({mode}): import diagnostics, symlink identity, unsaved imports, alias navigation, and latest edit wins')



def run_cache(compiler):
    with tempfile.TemporaryDirectory(prefix='dq-lsp-cache-') as directory:
        root = Path(directory)
        library = root / 'lib'
        library.mkdir()
        include = library / 'value.dqi'
        original = 'const VALUE : int = 7\n'
        include.write_text(original)
        (library / 'lib.dq').write_text('#include "./value.dqi"\n')
        main = root / 'main.dq'
        main_text = 'use lib\nvar result : int = VALUE\n'
        main.write_text(main_text)
        output = root / 'unwritten' / 'project.code-workspace'
        project = root / 'app.dqproj'
        project.write_text(f"main = 'main.dq'\npkg = 'lib:{library}'\noutput = '{output}'\nlink = true\n")
        temporary = root / 'temporary'
        temporary.mkdir()
        environment = dict(os.environ, TMPDIR=str(temporary), TMP=str(temporary), TEMP=str(temporary))
        with subprocess.Popen([str(compiler), '--langserver', '--no-use-sys', str(project)],
                              cwd=root, env=environment, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE) as process:
            request_id = 0
            diagnostics = {}

            def send(method, params=None, request=None):
                message = {'jsonrpc': '2.0', 'method': method, 'params': params}
                if request is not None:
                    message['id'] = request
                body = json.dumps(message).encode()
                process.stdin.write(f'Content-Length: {len(body)}\r\n\r\n'.encode() + body)
                process.stdin.flush()

            def request(method, params):
                nonlocal request_id
                request_id += 1
                send(method, params, request_id)
                while True:
                    headers = {}
                    while True:
                        line = process.stdout.readline()
                        assert line, process.stderr.read().decode()
                        if line == b'\r\n':
                            break
                        name, value = line.decode().split(':', 1)
                        headers[name.lower()] = value.strip()
                    message = json.loads(process.stdout.read(int(headers['content-length'])))
                    assert 'error' not in message, message
                    if message.get('method') == 'textDocument/publishDiagnostics':
                        diagnostics[message['params']['uri']] = message['params']['diagnostics']
                    if message.get('id') == request_id:
                        return message['result']

            def open_document(path, text):
                send('textDocument/didOpen', {'textDocument': {
                    'uri': path.as_uri(), 'languageId': 'dq', 'version': 1, 'text': text}})

            def change_document(path, text, version):
                send('textDocument/didChange', {'textDocument': {'uri': path.as_uri(), 'version': version},
                     'contentChanges': [{'text': text}]})

            def analyze():
                # A request is a barrier: preceding notifications have finished analysis.
                request('textDocument/documentSymbol', {'textDocument': {'uri': main.as_uri()}})
                return diagnostics[main.as_uri()]

            def artifacts():
                return {path: (path.stat().st_mtime_ns, path.read_bytes())
                        for path in temporary.rglob('*.dqm_if')}

            try:
                request('initialize', {'capabilities': {}})
                send('initialized', {})
                open_document(main, main_text)
                assert not analyze(), diagnostics
                cached = artifacts()
                assert cached, 'No cached module interfaces'
                assert not output.parent.exists(), 'Analysis created the project output lock directory'
                assert not (root / '.dqbuild').exists(), 'Analysis wrote into the project build tree'

                # Opening an include invalidates its metadata once. Subsequent main-file
                # edits must preserve both the overlay and the cached module interface.
                open_document(include, original)
                assert not analyze(), diagnostics
                cached = artifacts()
                change_document(main, main_text + '\n', 2)
                assert not analyze(), diagnostics
                assert artifacts() == cached, 'An unrelated edit rebuilt unchanged module interfaces'
                change_document(include, original, 2)
                assert not analyze(), diagnostics
                assert artifacts() == cached, 'Identical buffer text invalidated the cache'

                # Same-size unsaved changes to a transitive include must invalidate it.
                change_document(include, original.replace('VALUE', 'OTHER'), 3)
                assert analyze(), 'A changed include reused stale declarations'
                assert artifacts() != cached, 'A changed include did not rebuild its interface'
                send('textDocument/didClose', {'textDocument': {'uri': include.as_uri()}})
                assert not analyze(), 'Closing an unsaved include did not restore disk declarations'
                assert include.read_text() == original

                # Disk changes must invalidate the persistent cache too.
                include.write_text(original.replace('VALUE', 'OTHER'))
                change_document(main, main_text + '\n\n', 3)
                assert analyze(), 'A disk edit reused stale declarations'
                assert not output.parent.exists(), 'Analysis created the project output lock directory'
                request('shutdown', None)
                send('exit')
                process.wait(timeout=10)
                assert process.returncode == 0, process.stderr.read().decode()
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
        assert not list(temporary.iterdir()), 'Server did not clean up its private cache'
        print('PASS: persistent interfaces, stable overlays, include invalidation, close, disk edits, and no output locks')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, default=Path(__file__).resolve().parents[1] / 'build/dq-comp')
    compiler = parser.parse_args().compiler.resolve()
    for mode in ['roots', 'project-mapping', 'cli-mapping']:
        run(compiler, mode)
    run_cache(compiler)
