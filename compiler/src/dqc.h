/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:   DQ Compiler Object global instance
 */

#pragma once

#include "stdint.h"
#include <string>
#include "comp_options.h"

#include "dqc_clargs.h"
#include "scf_base.h"

using namespace std;

class ODqCompiler : public ODqCompClargs
{
private:
  using            super = ODqCompClargs;

  bool AddImplicitUse(const string & module_name, const string & namespace_name,
                      OScope * merge_scope, bool is_private,
                      EModuleUseMergeMode merge_mode);

public:
  ODqCompiler();
  virtual ~ODqCompiler();

  void Run(int argc, char ** argv);
};

extern ODqCompiler *  g_compiler;

void dqc_init();
