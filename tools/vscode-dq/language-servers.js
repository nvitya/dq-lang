const path = require("path");
const fs = require("fs");
const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");

const projectExcludes = "**/{.git,.dqbuild,node_modules}/**";
const activeProjectKey = "dq.activeProjectFolder";

class LanguageServers {
  constructor(context) {
    this.context = context;
    this.clients = new Map();
    this.projects = new Map();
    this.projectFiles = [];
    this.activeFolder = context.workspaceState.get(activeProjectKey);
    this.pending = Promise.resolve();
    this.initialized = false;
    this.stopped = false;
    this.status = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left);
    this.status.command = "dq.selectLanguageServerProject";
    this.status.name = "DQ Active Project";
    context.subscriptions.push(this.status);
  }

  start() {
    const watcher = vscode.workspace.createFileSystemWatcher("**/*.dqproj");
    this.context.subscriptions.push(
      watcher,
      watcher.onDidCreate(() => this.scheduleRefresh(true)),
      watcher.onDidDelete(() => this.scheduleRefresh(true)),
      watcher.onDidChange(() => this.scheduleRefresh(true)),
      vscode.workspace.onDidChangeConfiguration(event => {
        if (event.affectsConfiguration("dq")) this.scheduleRefresh(true);
      }),
      vscode.workspace.onDidChangeWorkspaceFolders(() => this.scheduleRefresh(true)),
      vscode.workspace.onDidOpenTextDocument(() => this.scheduleRefresh()),
      vscode.workspace.onDidCloseTextDocument(() => this.scheduleRefresh()),
      vscode.window.onDidChangeActiveTextEditor(editor => {
        this.activateEditorProject(editor);
        this.updateStatus();
        this.scheduleRefresh();
      }),
      vscode.commands.registerCommand("dq.selectLanguageServerProject", () => this.selectProject())
    );
    return this.scheduleRefresh(true);
  }

  scheduleRefresh(discover = false) {
    // Configuration and editor events can arrive while a client is stopping.
    // Serialize restarts so each project has only one client.
    this.pending = this.pending.then(() => {
      if (!this.stopped) return this.refresh(discover);
    }).catch(error => {
      void vscode.window.showErrorMessage(`DQ language server: ${error.message}`);
    });
    return this.pending;
  }

  async discoverProjects() {
    this.projectFiles = await vscode.workspace.findFiles("**/*.dqproj", projectExcludes);
    this.projects.clear();
    const configured = vscode.workspace.getConfiguration("dq").get("languageServerProjects", {});
    for (const folder of vscode.workspace.workspaceFolders ?? []) {
      const key = folder.uri.toString();
      const configuration = vscode.workspace.getConfiguration("dq", folder.uri);
      const mapped = configured[folder.name];
      let filename = typeof mapped === "string" && mapped.trim()
        ? mapped.trim() : configuration.get("languageServerProject", "").trim();
      if (!filename) {
        const files = this.projectFiles.filter(uri =>
          vscode.workspace.getWorkspaceFolder(uri)?.uri.toString() === key);
        if (files.length === 1) filename = files[0].fsPath;
      }
      if (filename) {
        this.projects.set(key, { folder, filename: path.resolve(folder.uri.fsPath, filename) });
      }
    }
  }

  activateEditorProject(editor) {
    if (!editor || (editor.document.languageId !== "dq"
        && !editor.document.uri.fsPath.endsWith(".dqproj"))) return;
    const folder = this.workspaceFolder(editor.document.uri);
    const key = folder?.uri.toString();
    if (this.projects.has(key) && key !== this.activeFolder) {
      this.activeFolder = key;
      void this.context.workspaceState.update(activeProjectKey, key);
    }
  }

  updateStatus() {
    const project = this.projects.get(this.activeFolder);
    this.status.text = project ? `DQ: ${path.basename(project.filename)}` : "DQ: Select Project";
    this.status.tooltip = project
      ? `Libraries use ${project.filename}\nClick to select the active DQ project.`
      : "Select the application project to use when editing DQ libraries.";
    const document = vscode.window.activeTextEditor?.document;
    if (document && (document.languageId === "dq" || document.uri.fsPath.endsWith(".dqproj"))) {
      this.status.show();
    } else {
      this.status.hide();
    }
  }

  serverInfo(folder, project) {
    // Inherit the application's compiler and arguments as well as its project.
    const configuration = vscode.workspace.getConfiguration("dq", project?.folder.uri ?? folder.uri);
    const compilerPath = configuration.get("languageServerPath", "dq-comp");
    const args = ["--langserver", ...configuration.get("languageServerArgs", [])];
    if (project) args.push(project.filename);
    const cwd = project ? path.dirname(project.filename) : folder.uri.fsPath;
    let projectVersion;
    if (project) {
      try {
        const stat = fs.statSync(project.filename);
        projectVersion = `${stat.mtimeMs}:${stat.size}`;
      } catch {} // Missing projects are reported by the compiler.
    }
    return { compilerPath, args, cwd, projectVersion };
  }

  workspaceFolder(uri) {
    const direct = vscode.workspace.getWorkspaceFolder(uri);
    if (direct) return direct;
    // Only routing needs physical paths here. The compiler handles source
    // identity, overlays and locations without changing the editor's URI.
    try {
      const filename = fs.realpathSync(uri.fsPath);
      return [...vscode.workspace.workspaceFolders ?? []]
        .flatMap(folder => {
          try { return [{ folder, root: fs.realpathSync(folder.uri.fsPath) }]; }
          catch { return []; }
        })
        .sort((a, b) => b.root.length - a.root.length)
        .find(({ root }) => {
          const relative = path.relative(root, filename);
          return !path.isAbsolute(relative) && relative !== ".." && !relative.startsWith(`..${path.sep}`);
        })?.folder;
    } catch {
      return undefined;
    }
  }

  async refresh(discover) {
    if (discover) await this.discoverProjects();
    if (!this.initialized) {
      this.activateEditorProject(vscode.window.activeTextEditor);
      this.initialized = true;
    }
    if (!this.projects.has(this.activeFolder)) {
      this.activeFolder = this.projects.keys().next().value;
    }
    this.updateStatus();
    const activeProject = this.projects.get(this.activeFolder);
    const servers = new Map();
    const addServer = (folder, selector) => {
      const project = this.projects.get(folder.uri.toString()) ?? activeProject;
      const info = this.serverInfo(folder, project);
      const key = JSON.stringify(info);
      if (!servers.has(key)) {
        servers.set(key, { name: project?.folder.name ?? folder.name ?? "Libraries",
          ...info, documentSelector: [] });
      }
      servers.get(key).documentSelector.push(selector);
    };
    for (const folder of vscode.workspace.workspaceFolders ?? []) {
      addServer(folder, { scheme: "file", language: "dq",
        pattern: new vscode.RelativePattern(folder.uri, "**/*") });
    }

    // Imported packages need not be workspace folders. Match only the direct
    // parent directories of open external files, avoiding workspace documents.
    const externalDirectories = [...new Set(vscode.workspace.textDocuments
      .filter(document => document.languageId === "dq" && document.uri.scheme === "file"
        && !vscode.workspace.getWorkspaceFolder(document.uri))
      .map(document => path.dirname(document.uri.fsPath)))].sort();
    for (const directory of externalDirectories) {
      const uri = vscode.Uri.file(directory);
      const folder = this.workspaceFolder(uri) ?? { uri };
      addServer(folder, { scheme: "file", language: "dq",
        pattern: new vscode.RelativePattern(uri, "*") });
    }

    for (const [key, entry] of this.clients) {
      if (entry.signature !== JSON.stringify(servers.get(key))) {
        await entry.client.stop();
        this.clients.delete(key);
      }
    }
    for (const [key, server] of servers) {
      if (this.clients.has(key) || this.stopped) continue;
      const client = new LanguageClient(
        `dqLanguageServer.${server.name}`, `DQ Language Server (${server.name})`,
        { command: server.compilerPath, args: server.args, transport: TransportKind.stdio,
          options: { cwd: server.cwd } },
        { documentSelector: server.documentSelector,
          outputChannelName: `DQ Language Server (${server.name})` }
      );
      try {
        await client.start();
        this.clients.set(key, { client, signature: JSON.stringify(server) });
      } catch (error) {
        await client.dispose();
        void vscode.window.showErrorMessage(`DQ language server (${server.name}): ${error.message}`);
      }
    }
  }

  async selectProject() {
    await this.scheduleRefresh(true);
    const choices = new Map();
    const addChoice = (uri, folder) => {
      if (!folder || choices.has(uri.fsPath)) return;
      choices.set(uri.fsPath, {
        label: path.relative(folder.uri.fsPath, uri.fsPath),
        description: folder.name,
        picked: uri.fsPath === this.projects.get(this.activeFolder)?.filename,
        uri, folder
      });
    };
    for (const uri of this.projectFiles) addChoice(uri, vscode.workspace.getWorkspaceFolder(uri));
    for (const project of this.projects.values()) addChoice(vscode.Uri.file(project.filename), project.folder);
    if (!choices.size) {
      void vscode.window.showInformationMessage("No DQ projects found. Open a folder containing a .dqproj file.");
      return;
    }
    const selected = await vscode.window.showQuickPick([...choices.values()], {
      placeHolder: "Select the active DQ project for library editing", matchOnDescription: true
    });
    if (!selected) return;
    this.activeFolder = selected.folder.uri.toString();
    await this.context.workspaceState.update(activeProjectKey, this.activeFolder);
    const configuration = vscode.workspace.getConfiguration("dq");
    const projects = { ...configuration.get("languageServerProjects", {}) };
    projects[selected.folder.name] = path.relative(selected.folder.uri.fsPath, selected.uri.fsPath);
    await configuration.update("languageServerProjects", projects, vscode.ConfigurationTarget.Workspace);
    await this.scheduleRefresh(true);
  }

  async stop() {
    this.stopped = true;
    await this.pending;
    await Promise.all([...this.clients.values()].map(entry => entry.client.stop()));
    this.clients.clear();
  }
}

module.exports = { LanguageServers };
