/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    atr_options.h
 * authors: Codex
 * created: 2026-03-17
 * brief:   dqatrun options
 */

#pragma once

#include <string>

using namespace std;

enum EAtrunMode
{
  ATRMODE_SINGLE = 0,
  ATRMODE_BATCH  = 1
};

class OAtrOptions
{
public:
  EAtrunMode        run_mode = ATRMODE_SINGLE;

  string            compiler_filename = "dq-comp";
  string            test_root = ".";
  string            single_test_filename;

  int               worker_count = 16;
  bool              verbose = false;

  int               arg_error_count = 0;

public:
  OAtrOptions();
  virtual ~OAtrOptions();

  void ParseCmdLineArgs(int argc, char ** argv);
  void PrintUsage();
};

extern OAtrOptions *  g_atropt;

void init_atr_options(int argc, char ** argv);

