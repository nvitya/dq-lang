/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_clargs.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#pragma once

#include "stdint.h"
#include <string>
#include <vector>
#include "comp_options.h"

#include "dqc_codegen.h"

using namespace std;

class ODqCompClargs : public ODqCompCodegen
{
private:
  using            super = ODqCompCodegen;

public:
  string           in_filename = "";
  string           out_filename = "";  // output path for object or interface files
  string           interface_out_filename = ""; // interface paired with out_filename
  string           base_name = "";     // in_filename with .dq stripped
  string           link_output = "";   // final executable/output name
  bool             has_dash_o = false; // -o was specified
  bool             has_output = false; // output selected by project, -o, or legacy positional arg

public:
  ODqCompClargs();
  virtual ~ODqCompClargs();

  void PrepareOutputPaths();

};
