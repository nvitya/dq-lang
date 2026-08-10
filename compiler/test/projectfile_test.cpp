/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    projectfile_test.cpp
 * authors: Codex
 * created: 2026-08-10
 * brief:   Focused DQ project file parser tests
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "projectfile.h"

using namespace std;
namespace fs = std::filesystem;

static int failures = 0;

static void Expect(bool condition, const string & message)
{
  if (!condition)
  {
    cerr << "projectfile test failed: " << message << "\n";
    ++failures;
  }
}

static void WriteFile(const fs::path & filename, const string & text)
{
  fs::create_directories(filename.parent_path());
  ofstream output(filename, ios::binary);
  output << text;
}

static bool HasDiagnostic(const ODqProjectFile & project, const string & id)
{
  return !project.diagnostics.empty() && (project.diagnostics.front().id == id);
}

int main()
{
  fs::path root = fs::temp_directory_path() / "dq-projectfile-test";
  error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "packages" / "sdk" / "project");
  fs::create_directories(root / "packages" / "sdk" / "ld");

  WriteFile(root / "packages" / "sdk" / "project" / "common.dqproj", R"(
/* include-local paths use the fragment directory */
var SDK_LD = '../ld'
define FROM_INCLUDE = 17
linkerpath = '${SDK_LD}'
linkscript = '${SDK_LD}/board.ld'
)");
  WriteFile(root / "main.dq", "// parser fixture\n");
  WriteFile(root / "full.dqproj", R"(
// comments, includes, variables, functions, and semicolon-separated statements
packagepath = 'packages'
var SDK = PackagePath('sdk')
include '${SDK}/project/common.dqproj'
main = '${PROJECT_DIR}/main.dq'; output = '${THIS_DIR}/out.elf'
target = 'arm_m7f-bare'
link = true
optlevel = 2
debuginfo = true
define FEATURE
define COUNT = -42
linkobject = '${SDK}/objects/startup.o'
linkoption = '--gc-sections'
)");

  ODqProjectFile project;
  Expect(project.Load(root / "full.dqproj", {}, {}), "full project should parse");
  Expect(project.main_file == fs::absolute(root / "main.dq").lexically_normal(), "main path");
  Expect(project.output_file && (*project.output_file == fs::absolute(root / "out.elf").lexically_normal()),
         "output path");
  Expect(project.target && (*project.target == "arm_m7f-bare"), "target value");
  Expect(project.link && *project.link, "link value");
  Expect(project.optlevel && (*project.optlevel == 2), "optimization value");
  Expect(project.debuginfo && *project.debuginfo, "debug value");
  Expect(project.defines.size() == 3, "included and local defines");
  Expect(project.package_paths.size() == 1, "project package path");
  Expect(project.link_objects.size() == 1, "link object");
  Expect(project.linker_args.size() == 3, "ordered linker arguments");
  if (project.linker_args.size() == 3)
  {
    Expect(project.linker_args[0].starts_with("--library-path="), "linker path translation");
    Expect(project.linker_args[1].starts_with("--script="), "linker script translation");
    Expect(project.linker_args[2] == "--gc-sections", "raw linker option");
  }

  fs::create_directories(root / "project-packages" / "selected");
  fs::create_directories(root / "cli-packages" / "selected");
  WriteFile(root / "package-precedence.dqproj", R"(
packagepath = 'project-packages'
var SELECTED = PackagePath('selected')
main = '${SELECTED}/main.dq'
)");
  Expect(project.Load(root / "package-precedence.dqproj", {}, {(root / "cli-packages").string()}),
         "package precedence project should parse");
  Expect(project.main_file == fs::absolute(root / "cli-packages" / "selected" / "main.dq").lexically_normal(),
         "command-line package root precedence");

  WriteFile(root / "duplicate.dqproj", "main='main.dq'\nmain='other.dq'\n");
  Expect(!project.Load(root / "duplicate.dqproj", {}, {}) && HasDiagnostic(project, "ProjectDuplicate"),
         "duplicate scalar diagnostic");

  WriteFile(root / "duplicate-variable.dqproj", "var ROOT='one'\nvar ROOT='two'\nmain='main.dq'\n");
  Expect(!project.Load(root / "duplicate-variable.dqproj", {}, {}) && HasDiagnostic(project, "ProjectDuplicate"),
         "duplicate variable diagnostic");

  WriteFile(root / "duplicate-define.dqproj", "main='main.dq'\ndefine SAME\ndefine SAME=2\n");
  Expect(!project.Load(root / "duplicate-define.dqproj", {}, {}) && HasDiagnostic(project, "ProjectDuplicate"),
         "duplicate define diagnostic");

  WriteFile(root / "unknown-variable.dqproj", "main='${UNKNOWN}/main.dq'\n");
  Expect(!project.Load(root / "unknown-variable.dqproj", {}, {}) && HasDiagnostic(project, "ProjectVariable"),
         "unknown variable diagnostic");

  WriteFile(root / "missing-package.dqproj", "var SDK=PackagePath('missing')\nmain='main.dq'\n");
  Expect(!project.Load(root / "missing-package.dqproj", {}, {}) && HasDiagnostic(project, "ProjectPackage"),
         "missing package diagnostic");

  WriteFile(root / "missing-main.dqproj", "define FEATURE\n");
  Expect(!project.Load(root / "missing-main.dqproj", {}, {}) && HasDiagnostic(project, "ProjectMain"),
         "missing main diagnostic");

  WriteFile(root / "unknown-property.dqproj", "main='main.dq'\nunknown='value'\n");
  Expect(!project.Load(root / "unknown-property.dqproj", {}, {}) && HasDiagnostic(project, "ProjectProperty"),
         "unknown property diagnostic");

  WriteFile(root / "bad-optlevel.dqproj", "main='main.dq'\noptlevel=4\n");
  Expect(!project.Load(root / "bad-optlevel.dqproj", {}, {}) && HasDiagnostic(project, "ProjectValue"),
         "invalid optimization diagnostic");

  WriteFile(root / "trailing.dqproj", "main='main.dq' extra\n");
  Expect(!project.Load(root / "trailing.dqproj", {}, {}) && HasDiagnostic(project, "ProjectTrailingToken"),
         "trailing token diagnostic");

  WriteFile(root / "bad-string.dqproj", "main='unterminated\n");
  Expect(!project.Load(root / "bad-string.dqproj", {}, {}) && HasDiagnostic(project, "ProjectString"),
         "malformed string diagnostic");

  WriteFile(root / "bad-comment.dqproj", "/* unterminated\nmain='main.dq'\n");
  Expect(!project.Load(root / "bad-comment.dqproj", {}, {}) && HasDiagnostic(project, "ProjectComment"),
         "malformed comment diagnostic");

  WriteFile(root / "cycle-a.dqproj", "include 'cycle-b.dqproj'\nmain='main.dq'\n");
  WriteFile(root / "cycle-b.dqproj", "include 'cycle-a.dqproj'\n");
  Expect(!project.Load(root / "cycle-a.dqproj", {}, {}) && HasDiagnostic(project, "ProjectIncludeCycle"),
         "include cycle diagnostic");

  WriteFile(root / "unsupported.dqproj", "main='main.dq'\ncpu='cortex-m7'\n");
  Expect(!project.Load(root / "unsupported.dqproj", {}, {}) && HasDiagnostic(project, "ProjectUnsupported"),
         "deferred target tuning diagnostic");

  WriteFile(root / "legacy.dqprj", "main='main.dq'\n");
  Expect(!project.Load(root / "legacy.dqprj", {}, {}) && HasDiagnostic(project, "ProjectExtension"),
         "legacy extension diagnostic");

  fs::remove_all(root, ec);
  if (failures)
  {
    cerr << failures << " projectfile test(s) failed\n";
    return 1;
  }
  cout << "Project file tests passed.\n";
  return 0;
}
