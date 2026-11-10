const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');

function fixture(options = {}) {
  const uri = filename => ({ fsPath: filename, scheme: 'file', toString() { return `file://${filename}`; } });
  const folders = (options.folders ?? ['/ws/blink', '/ws/uart', '/ws/vihal'])
    .map(filename => ({ name: path.basename(filename), uri: uri(filename) }));
  const events = new Map();
  const on = name => callback => { events.set(name, callback); return { dispose() {} }; };
  const state = options.state ?? new Map();
  const global = options.global ?? { languageServerProjects: { blink: 'blink.dqproj', uart: 'uart.dqproj' } };
  const configs = options.configs ?? {};
  const clients = [];
  const errors = [];
  const updates = [];
  const status = { show() { this.visible = true; }, hide() { this.visible = false; }, dispose() {} };
  const vscode = {
    Uri: { file: uri }, StatusBarAlignment: { Left: 1 }, ConfigurationTarget: { Workspace: 2 },
    RelativePattern: class { constructor(baseUri, pattern) { this.baseUri = baseUri; this.pattern = pattern; } },
    workspace: {
      workspaceFolders: folders, textDocuments: [],
      getWorkspaceFolder(uri) {
        return folders.filter(folder => uri.fsPath === folder.uri.fsPath || uri.fsPath.startsWith(folder.uri.fsPath + '/'))
          .sort((a, b) => b.uri.fsPath.length - a.uri.fsPath.length)[0];
      },
      getConfiguration(_, resource) {
        const scoped = configs[resource?.fsPath] ?? {};
        return { get: (key, fallback) => scoped[key] ?? global[key] ?? fallback,
          async update(key, value) { global[key] = value; updates.push({ key, value }); events.get('config')?.({ affectsConfiguration: () => true }); } };
      },
      findFiles: async () => (options.files ?? []).map(uri),
      createFileSystemWatcher: () => ({ dispose() {}, onDidCreate: on('create'), onDidDelete: on('delete'), onDidChange: on('project') }),
      onDidChangeConfiguration: on('config'), onDidChangeWorkspaceFolders: on('folders'),
      onDidOpenTextDocument: on('open'), onDidCloseTextDocument: on('close')
    },
    window: { createStatusBarItem: () => status, onDidChangeActiveTextEditor: on('focus'),
      showQuickPick: async choices => { vscode.choices = choices; return options.pick?.(choices); },
      showInformationMessage: message => errors.push(message), showErrorMessage: message => errors.push(message) },
    commands: { registerCommand: () => ({ dispose() {} }) }
  };
  class LanguageClient {
    constructor(id, name, server, clientOptions) { Object.assign(this, { id, name, server, clientOptions, running: false }); clients.push(this); }
    async start() { assert.ok(!clients.some(client => client !== this && client.running && client.id === this.id)); this.running = true; }
    async stop() { await new Promise(resolve => setImmediate(resolve)); this.running = false; this.stops = (this.stops ?? 0) + 1; }
    async dispose() { this.running = false; }
  }
  const context = { subscriptions: [], workspaceState: { get: key => state.get(key), update: async (key, value) => state.set(key, value) } };
  const sandbox = { module: { exports: {} }, require: name => name === 'vscode' ? vscode
    : name === 'vscode-languageclient/node' ? { LanguageClient, TransportKind: { stdio: 0 } } : require(name) };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../language-servers.js'), 'utf8'), sandbox);
  const manager = new sandbox.module.exports.LanguageServers(context);
  const document = filename => ({ languageId: 'dq', uri: uri(filename) });
  const focus = async filename => {
    vscode.window.activeTextEditor = { document: document(filename) };
    events.get('focus')?.(vscode.window.activeTextEditor);
    await manager.pending;
  };
  const running = () => clients.filter(client => client.running);
  const clientFor = filename => running().find(client => client.clientOptions.documentSelector.some(selector => {
    const base = selector.pattern.baseUri.fsPath;
    return filename.startsWith(base + '/') && (selector.pattern.pattern === '**/*' || path.dirname(filename) === base);
  }));
  return { vscode, manager, state, clients, errors, updates, status, document, focus, running, clientFor, events };
}

test('application and libraries share a client and inherit all compiler options', async t => {
  const f = fixture({ configs: { '/ws/blink': { languageServerPath: '/custom/dq-comp', languageServerArgs: ['--pkg-path', 'boardpkg'] } } });
  t.after(() => f.manager.stop());
  await f.focus('/ws/blink/main.dq'); await f.manager.start();
  const app = f.clientFor('/ws/blink/main.dq');
  assert.equal(f.clientFor('/ws/vihal/pins.dq'), app);
  assert.equal(app.server.command, '/custom/dq-comp');
  assert.equal(app.server.options.cwd, '/ws/blink');
  assert.ok(app.server.args.includes('boardpkg'));
  assert.ok(app.server.args.includes('/ws/blink/blink.dqproj'));
  assert.notEqual(f.clientFor('/ws/uart/main.dq'), app);
  assert.equal(f.updates.length, 0);
  assert.deepEqual(f.errors, []);
});

test('library context follows the last application and survives library or non-DQ focus', async t => {
  const f = fixture(); t.after(() => f.manager.stop());
  await f.focus('/ws/blink/main.dq'); await f.manager.start();
  await f.focus('/ws/uart/main.dq');
  assert.equal(f.clientFor('/ws/vihal/pins.dq'), f.clientFor('/ws/uart/main.dq'));
  await f.focus('/ws/vihal/pins.dq');
  assert.ok(f.status.text.includes('uart.dqproj'));
  f.events.get('focus')({ document: { languageId: 'markdown', uri: f.document('/ws/blink/README.md').uri } });
  await f.manager.pending;
  assert.ok(f.status.text.includes('uart.dqproj'));
  assert.deepEqual(f.errors, []);
});

test('single-project folders are detected automatically without settings writes', async t => {
  const f = fixture({ global: {}, files: ['/ws/blink/only.dqproj'] }); t.after(() => f.manager.stop());
  await f.manager.start();
  assert.equal(f.clientFor('/ws/vihal/pins.dq'), f.clientFor('/ws/blink/main.dq'));
  assert.ok(f.clientFor('/ws/blink/main.dq').server.args.includes('/ws/blink/only.dqproj'));
  assert.equal(f.updates.length, 0);
});

test('picker works from a library and persists the selected application', async t => {
  const state = new Map();
  const f = fixture({ state, files: ['/ws/blink/a.dqproj', '/ws/blink/b.dqproj', '/ws/uart/uart.dqproj'],
    pick: choices => choices.find(choice => choice.label === 'b.dqproj') });
  t.after(() => f.manager.stop());
  await f.focus('/ws/vihal/pins.dq'); await f.manager.start(); await f.manager.selectProject();
  assert.equal(f.vscode.choices.length, 4); // Includes the explicitly configured blink.dqproj.
  assert.equal(f.updates[0].value.blink, 'b.dqproj');
  assert.equal(f.updates[0].value.vihal, undefined);
  assert.ok(f.clientFor('/ws/vihal/pins.dq').server.args.includes('/ws/blink/b.dqproj'));
  const restored = fixture({ state, global: { languageServerProjects: f.updates[0].value } });
  t.after(() => restored.manager.stop());
  await restored.focus('/ws/vihal/pins.dq'); await restored.manager.start();
  assert.ok(restored.status.text.includes('b.dqproj'));
});

test('external package files join the active project client and stop matching when closed', async t => {
  const f = fixture(); t.after(() => f.manager.stop());
  f.vscode.workspace.textDocuments = [f.document('/packages/vihal/pins.dq')];
  await f.focus('/ws/uart/main.dq'); await f.manager.start();
  assert.equal(f.clientFor('/packages/vihal/pins.dq'), f.clientFor('/ws/uart/main.dq'));
  f.vscode.workspace.textDocuments = []; f.events.get('close')(); await f.manager.pending;
  assert.equal(f.clientFor('/packages/vihal/pins.dq'), undefined);
});

test('symlink aliases of a pinned library use the same client and project', async t => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'dq-routing-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  for (const name of ['app', 'other', 'physical', 'packages']) fs.mkdirSync(path.join(root, name));
  fs.symlinkSync(path.join(root, 'physical'), path.join(root, 'vihal'), 'dir');
  fs.symlinkSync(path.join(root, 'physical'), path.join(root, 'packages/vihal'), 'dir');
  const f = fixture({ folders: [path.join(root, 'app'), path.join(root, 'other'), path.join(root, 'vihal')],
    global: { languageServerProjects: { app: 'app.dqproj', other: 'other.dqproj', vihal: '../app/app.dqproj' } } });
  t.after(() => f.manager.stop());
  f.vscode.workspace.textDocuments = [f.document(path.join(root, 'packages/vihal/pins.dq'))];
  await f.focus(path.join(root, 'other/main.dq')); await f.manager.start();
  assert.equal(f.clientFor(path.join(root, 'packages/vihal/pins.dq')), f.clientFor(path.join(root, 'vihal/pins.dq')));
  assert.equal(f.clientFor(path.join(root, 'vihal/pins.dq')), f.clientFor(path.join(root, 'app/main.dq')));
  assert.deepEqual(f.errors, []);
});

test('rapid editor and configuration events serialize client replacement', async t => {
  const f = fixture(); t.after(() => f.manager.stop()); await f.manager.start();
  const changes = ['/ws/uart/main.dq', '/ws/blink/main.dq', '/ws/uart/main.dq'].map(filename => f.focus(filename));
  f.events.get('config')({ affectsConfiguration: () => true });
  await Promise.all(changes); await f.manager.pending;
  assert.equal(f.clientFor('/ws/vihal/pins.dq'), f.clientFor('/ws/uart/main.dq'));
  assert.deepEqual(f.errors, []);
});

test('saving a project reloads its application and library context only', async t => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'dq-project-reload-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  for (const name of ['app', 'other', 'library']) fs.mkdirSync(path.join(root, name));
  const project = path.join(root, 'app/app.dqproj');
  fs.writeFileSync(project, "main = 'main.dq'\n");
  const f = fixture({ folders: ['app', 'other', 'library'].map(name => path.join(root, name)),
    global: { languageServerProjects: { app: 'app.dqproj', other: 'other.dqproj' } } });
  t.after(() => f.manager.stop()); await f.manager.start();
  const oldApp = f.clientFor(path.join(root, 'app/main.dq'));
  const other = f.clientFor(path.join(root, 'other/main.dq'));
  fs.writeFileSync(project, "main = 'main.dq'\ntarget = 'armhf-linux'\n");
  f.events.get('project')(); await f.manager.pending;
  assert.equal(oldApp.running, false);
  assert.notEqual(f.clientFor(path.join(root, 'app/main.dq')), oldApp);
  assert.equal(f.clientFor(path.join(root, 'library/pins.dq')), f.clientFor(path.join(root, 'app/main.dq')));
  assert.equal(f.clientFor(path.join(root, 'other/main.dq')), other);
  assert.deepEqual(f.errors, []);
});
