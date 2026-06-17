const path = require("path");
const vscode = require("vscode");

function activate(context) {
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

function deactivate() {}

module.exports = { activate, deactivate };
