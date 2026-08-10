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
#include <optional>
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
  filesystem::path filename;
  filesystem::path main_file;
  optional<filesystem::path> output_file;
  optional<string> target;
  optional<bool> link;
  optional<int> optlevel;
  optional<bool> debuginfo;

  vector<OCmdLineDefine> defines;
  vector<string> package_paths;
  vector<string> link_objects;
  vector<string> linker_args;
  vector<SDqProjectDiagnostic> diagnostics;

  bool Load(const filesystem::path & top_file,
            const vector<string> & default_package_paths,
            const vector<string> & command_line_package_paths);

  bool Loaded() const { return !filename.empty() && diagnostics.empty(); }
};

