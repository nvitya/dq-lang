/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    projectfile.h
 * authors: Codex
 * created: 2026-08-10
 * brief:   DQ compiler project file
 */

#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "comp_options.h"

using namespace std;

struct SDqProjectDiagnostic
{
  filesystem::path filename;
  int line = 1;
  int col = 1;
  string id;
  string message;

  string Format() const;
};

class ODqProjectFile
{
public:
  vector<SDqProjectDiagnostic> diagnostics;

  bool Load(const filesystem::path & top_file,
            const vector<string> & command_line_package_paths);

private:
  struct SParseContext;

  filesystem::path filename;
  vector<string> command_line_package_paths;
  filesystem::path project_dir;
  map<string, string> variables;
  set<string> single_properties;
  set<string> define_names;
  vector<filesystem::path> include_stack;

  filesystem::path AbsNorm(const filesystem::path & path) const;
  filesystem::path Canonical(const filesystem::path & path) const;
  pair<int, int> LineCol(const SParseContext & ctx, const char * pos) const;
  bool Fail(const SParseContext & ctx, const string & id, const string & message,
            const char * pos = nullptr);
  bool SkipSpace(SParseContext & ctx, bool include_line_end, bool & rsaw_line_end);
  bool SkipInlineSpace(SParseContext & ctx);
  bool RequireInlineSpaceResult(SParseContext & ctx, const string & expected);
  bool FinishStatement(SParseContext & ctx);
  bool ReadIdentifier(SParseContext & ctx, string & rvalue, const string & what);
  bool ReadString(SParseContext & ctx, string & rvalue);
  bool ExpandVariables(SParseContext & ctx, const string & value, string & rvalue);
  bool ReadExpandedString(SParseContext & ctx, string & rvalue);
  bool ReadPath(SParseContext & ctx, filesystem::path & rpath);
  bool ReadBool(SParseContext & ctx, bool & rvalue);
  bool ReadInt(SParseContext & ctx, int64_t & rvalue);
  bool CheckDuplicate(SParseContext & ctx, const string & name);
  vector<string> EffectivePackagePaths() const;
  bool ReadPackagePathCall(SParseContext & ctx, string & rvalue);
  bool ParseVariable(SParseContext & ctx);
  bool ParseInclude(SParseContext & ctx);
  bool ParseDefine(SParseContext & ctx);
  bool ParseProperty(SParseContext & ctx, const string & name);
  bool ParseFile(const filesystem::path & input_file);
};
