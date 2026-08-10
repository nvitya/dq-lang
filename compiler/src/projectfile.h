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
#include "strparse.h"

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

class ODqProject;

class ODqProjectFile
{
public:
  ODqProjectFile(ODqProject * aproject, const filesystem::path & afilename);

  ODqProjectFile(const ODqProjectFile &) = delete;
  ODqProjectFile & operator=(const ODqProjectFile &) = delete;
  ODqProjectFile(ODqProjectFile &&) = delete;
  ODqProjectFile & operator=(ODqProjectFile &&) = delete;

  filesystem::path  filename;
  string            text;
  TStrParseObj      sp;
  ODqProject * const pproject;

  bool ParseFile();

  filesystem::path AbsNorm(const filesystem::path & path) const;
  filesystem::path Canonical(const filesystem::path & path) const;
  pair<int, int> LineCol(const char * pos) const;
  bool Fail(const string & id, const string & message, const char * pos = nullptr);
  bool SkipSpace(bool include_line_end, bool & rsaw_line_end);
  bool SkipInlineSpace();
  bool RequireInlineSpaceResult(const string & expected);
  bool FinishStatement();
  bool ReadIdentifier(string & rvalue, const string & what);
  bool ReadString(string & rvalue);
  bool ExpandVariables(const string & value, string & rvalue);
  bool ReadExpandedString(string & rvalue);
  bool ReadPath(filesystem::path & rpath);
  bool ReadBool(bool & rvalue);
  bool ReadInt(int64_t & rvalue);
  bool CheckDuplicate(const string & name);
  vector<string> EffectivePackagePaths() const;
  bool ReadPackagePathCall(string & rvalue);
  bool ParseVariable();
  bool ParseInclude();
  bool ParseDefine();
  bool ParseProperty(const string & name);
};

class ODqProject
{
public:

  bool Load(const filesystem::path & top_file,
            const vector<string> & command_line_package_paths);

public:
  vector<SDqProjectDiagnostic>  diagnostics;
  vector<string>                command_line_package_paths;
  filesystem::path              project_dir;
  map<string, string>           variables;
  set<string>                   single_properties;
  set<string>                   define_names;
  vector<filesystem::path>      include_stack;
};
