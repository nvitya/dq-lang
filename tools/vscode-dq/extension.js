const path = require("path");
const vscode = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");

let languageClient;

function startLanguageServer(context) {
  const configuration = vscode.workspace.getConfiguration("dq");
  const compilerPath = configuration.get("languageServerPath", "dq-comp");
  const extraArgs = configuration.get("languageServerArgs", []);
  const projectFile = configuration.get("languageServerProject", "");
  const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
  const args = ["--langserver", ...extraArgs];
  if (projectFile) {
    args.push(projectFile);
  }

  languageClient = new LanguageClient(
    "dqLanguageServer",
    "DQ Language Server",
    {
      command: compilerPath,
      args,
      transport: TransportKind.stdio,
      options: workspaceFolder ? { cwd: workspaceFolder.uri.fsPath } : undefined
    },
    {
      documentSelector: [{ scheme: "file", language: "dq" }],
      outputChannelName: "DQ Language Server"
    }
  );
  languageClient.start();
  context.subscriptions.push({ dispose: () => languageClient?.stop() });
}

function activate(context) {
  startLanguageServer(context);

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
  if (languageClient) {
    await languageClient.stop();
    languageClient = undefined;
  }
}

module.exports = { activate, deactivate };
