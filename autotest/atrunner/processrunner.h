/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    processrunner.h
 * authors: Codex, nvitya
 * created: 2026-03-17
 * brief:   subprocess runner
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

using namespace std;

class OProcessRunner
{
public:
  string            workdir;
  vector<string>    args;

  int               exit_code = 0;
  int64_t           duration_us = 0;

  string            cmdline;
  string            stdout_text;
  string            stderr_text;

public:
  OProcessRunner();
  virtual ~OProcessRunner();

  bool Run();

protected:
  string BuildCmdLine();

};

