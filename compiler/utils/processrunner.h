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

enum
{
  PROCRUNERR_INVALID_ARGS = -1000,
  PROCRUNERR_PIPE_CREATE  = -1001,
  PROCRUNERR_FORK         = -1002,
  PROCRUNERR_SETUP        = -1003,
  PROCRUNERR_CHDIR        = -1004,
  PROCRUNERR_EXEC         = -1005,
  PROCRUNERR_TIMEOUT      = -1006,
  PROCRUNERR_WAIT         = -1007,
  PROCRUNERR_POLL         = -1008
};

class OProcessRunner
{
public:
  string            workdir;
  int               exec_timeout_ms = -1;
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

bool RunInteractiveProcess(const vector<string> & aargs, int & aexit_code, string * astderr_text = nullptr, const string & aworkdir = "");
