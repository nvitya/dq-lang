const path = require("path");
const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");

const languageClients = new Map();

function workspaceKey(workspaceFolder) {
  return workspaceFolder.uri.toString();
}

function configuredProjectFile(workspaceFolder) {
  const projects = vscode.workspace
    .getConfiguration("dq")
    .get("languageServerProjects", {});
  const projectFile = projects?.[workspaceFolder.name];
  if (typeof projectFile === "string" && projectFile.trim()) {
    return projectFile.trim();
  }

  return vscode.workspace
    .getConfiguration("dq", workspaceFolder.uri)
    .get("languageServerProject", "")
    .trim();
}

function languageServerInfo(workspaceFolder) {
  const configuration = vscode.workspace.getConfiguration("dq", workspaceFolder.uri);
  const compilerPath = configuration.get("languageServerPath", "dq-comp");
  const extraArgs = configuration.get("languageServerArgs", []);
  const configuredProject = configuredProjectFile(workspaceFolder);
  const projectFile = configuredProject
    ? path.resolve(workspaceFolder.uri.fsPath, configuredProject)
    : "";
  const args = ["--langserver", ...extraArgs];
  if (projectFile) {
    args.push(projectFile);
  }

  return {
    compilerPath,
    args,
    cwd: projectFile ? path.dirname(projectFile) : workspaceFolder.uri.fsPath,
    signature: JSON.stringify([compilerPath, args, projectFile ? path.dirname(projectFile) : workspaceFolder.uri.fsPath])
  };
}

function startLanguageServer(workspaceFolder) {
  const key = workspaceKey(workspaceFolder);
  if (languageClients.has(key)) return;

  const server = languageServerInfo(workspaceFolder);
  const client = new LanguageClient(
    `dqLanguageServer.${workspaceFolder.name}`,
    `DQ Language Server (${workspaceFolder.name})`,
    {
      command: server.compilerPath,
      args: server.args,
      transport: TransportKind.stdio,
      options: { cwd: server.cwd }
    },
    {
      documentSelector: [{
        scheme: "file",
        language: "dq",
        pattern: new vscode.RelativePattern(workspaceFolder.uri, "**/*")
      }],
      outputChannelName: `DQ Language Server (${workspaceFolder.name})`
    }
  );
  languageClients.set(key, { client, signature: server.signature });
  void client.start();
}

async function stopLanguageServer(workspaceFolder) {
  const key = workspaceKey(workspaceFolder);
  const entry = languageClients.get(key);
  if (!entry) return;
  languageClients.delete(key);
  await entry.client.stop();
}

async function restartChangedLanguageServers() {
  for (const workspaceFolder of vscode.workspace.workspaceFolders ?? []) {
    const entry = languageClients.get(workspaceKey(workspaceFolder));
    if (entry && entry.signature !== languageServerInfo(workspaceFolder).signature) {
      await stopLanguageServer(workspaceFolder);
      startLanguageServer(workspaceFolder);
    }
  }
}

async function selectLanguageServerProject() {
  let workspaceFolder = vscode.window.activeTextEditor
    ? vscode.workspace.getWorkspaceFolder(vscode.window.activeTextEditor.document.uri)
    : undefined;
  const workspaceFolders = vscode.workspace.workspaceFolders ?? [];
  if (!workspaceFolder && workspaceFolders.length === 1) {
    workspaceFolder = workspaceFolders[0];
  }
  if (!workspaceFolder) {
    const selectedFolder = await vscode.window.showQuickPick(
      workspaceFolders.map(folder => ({ label: folder.name, folder })),
      { placeHolder: "Select the DQ workspace folder" }
    );
    workspaceFolder = selectedFolder?.folder;
  }
  if (!workspaceFolder) return;

  const projectFiles = await vscode.workspace.findFiles(
    new vscode.RelativePattern(workspaceFolder.uri, "**/*.dqproj"),
    "**/{.git,.dqbuild,node_modules}/**"
  );
  const selectedProject = await vscode.window.showQuickPick(
    projectFiles.map(uri => ({
      label: path.relative(workspaceFolder.uri.fsPath, uri.fsPath),
      uri
    })),
    { placeHolder: `Select the DQ project for ${workspaceFolder.name}` }
  );
  if (!selectedProject) return;

  const configuration = vscode.workspace.getConfiguration("dq");
  const projects = { ...configuration.get("languageServerProjects", {}) };
  projects[workspaceFolder.name] = selectedProject.label;
  await configuration.update("languageServerProjects", projects, vscode.ConfigurationTarget.Workspace);
}

function activate(context) {
  for (const workspaceFolder of vscode.workspace.workspaceFolders ?? []) {
    startLanguageServer(workspaceFolder);
  }

  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration(() => restartChangedLanguageServers()),
    vscode.workspace.onDidChangeWorkspaceFolders(async event => {
      for (const workspaceFolder of event.removed) await stopLanguageServer(workspaceFolder);
      for (const workspaceFolder of event.added) startLanguageServer(workspaceFolder);
    }),
    vscode.commands.registerCommand("dq.selectLanguageServerProject", selectLanguageServerProject),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("dq.runCurrentFile", async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor || editor.document.languageId !== "dq") {
        vscode.window.showErrorMessage("Open a DQ file to run it.");
        return;
      }

      const document = editor.document;
      if (document.isUntitled) {
        vscode.window.showErrorMessage("Save the DQ file before running it.");
        return;
      }
      if (!(await document.save())) {
        vscode.window.showErrorMessage("Could not save the DQ file.");
        return;
      }

      const runPath = vscode.workspace
        .getConfiguration("dq", document.uri)
        .get("runPath", "dq-run");
      const scope =
        vscode.workspace.getWorkspaceFolder(document.uri) ??
        vscode.TaskScope.Workspace;
      const task = new vscode.Task(
        { type: "dq", command: "run" },
        scope,
        `Run ${path.basename(document.fileName)}`,
        "DQ",
        new vscode.ProcessExecution(runPath, ["-g", "-O0", document.fileName], {
          cwd: path.dirname(document.fileName)
        }),
        ["$dq"]
      );
      task.presentationOptions = {
        reveal: vscode.TaskRevealKind.Always,
        panel: vscode.TaskPanelKind.Dedicated,
        clear: true
      };

      await vscode.tasks.executeTask(task);
    })
  );
}

async function deactivate() {
  await Promise.all([...languageClients.values()].map(entry => entry.client.stop()));
  languageClients.clear();
}

module.exports = { activate, deactivate };
