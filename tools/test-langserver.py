#!/usr/bin/env python3
"""Compiler/LSP regressions for import diagnostics and unsaved source aliases.

Run: python3 tools/test-langserver.py --compiler build/dq-comp
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def run(compiler):
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
        project.write_text("main = 'main.dq'\ntarget = 'x86_64-linux'\npackagepath = '../packages'\nlink = false\n")
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
        process = subprocess.run([str(compiler), '--langserver', '--no-use-sys', str(project)],
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
        print('PASS: import diagnostics, symlink identity, unsaved imports, alias navigation, and latest edit wins')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, default=Path(__file__).resolve().parents[1] / 'build/dq-comp')
    run(parser.parse_args().compiler.resolve())
