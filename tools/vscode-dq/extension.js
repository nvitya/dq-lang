const path = require("path");
const vscode = require("vscode");
const { LanguageServers } = require("./language-servers");
const { blockCloserForHeader, hasIndentedBodyLine } = require("./block-closer");

let languageServers;

function appendBlockCloser(event) {
  if (event.document.languageId !== "dq" || event.contentChanges.length !== 1) return;

  const change = event.contentChanges[0];
  if (change.rangeLength !== 0 || !/^\n[\t ]*$/.test(change.text)) return;

  const headerLineNumber = change.range.start.line;
  const blockCloser = blockCloserForHeader(event.document.lineAt(headerLineNumber).text);
  if (!blockCloser) return;

  const bodyLine = event.document.lineAt(headerLineNumber + 1);
  if (bodyLine.text.trim()) return;
  const nearbyLines = [];
  for (let offset = 1; offset <= 2 && headerLineNumber + offset < event.document.lineCount; ++offset) {
    nearbyLines.push(event.document.lineAt(headerLineNumber + offset).text);
  }
  if (hasIndentedBodyLine(nearbyLines, blockCloser.indentation)) return;

  const editor = vscode.window.activeTextEditor;
  if (!editor || editor.document !== event.document) return;

  const bodyIndentation = `${blockCloser.indentation}    `;
  const cursorPosition = new vscode.Position(headerLineNumber + 1, bodyIndentation.length);
  void editor.edit(edit => {
    edit.replace(
      bodyLine.range,
      `${bodyIndentation}\n${blockCloser.indentation}${blockCloser.text}`
    );
  }).then(applied => {
    if (applied) editor.selection = new vscode.Selection(cursorPosition, cursorPosition);
  });
}

async function activate(context) {
  languageServers = new LanguageServers(context);
  context.subscriptions.push(vscode.workspace.onDidChangeTextDocument(appendBlockCloser));
  const ready = languageServers.start();

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
  await ready;
}

async function deactivate() {
  await languageServers?.stop();
}

module.exports = { activate, deactivate };
