/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
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
  string           base_name = "";     // in_filename with .dq stripped
  string           link_output = "";   // final executable/output name
  bool             has_dash_o = false; // -o was specified

public:
  ODqCompClargs();
  virtual ~ODqCompClargs();

  bool IsValidDefineName(const string & name);
  string ResolveCompilerExecutable(const string & argv0);
  string CompilerExecutableDir(const string & compiler_executable);
  string DefaultTargetArch();
  string DefaultTargetRtl();
  string DefaultBuildTag();
  void AddDefaultPackagePaths();
  string NormalizeCompilerExecutable(const string & argv0);
  void ParseModuleUseStack(const string & text, vector<string> & rstack);
  bool ParseDefineIntValue(const string & text, int64_t & rvalue);
  bool ParseDefineBoolValue(const string & text, bool & rvalue);
  bool VerblevelSwitch(const string & aswitch);

  void ParseCmdLineArgs(int argc, char ** argv);
  void ParseCmdLineArgsVerblevel(int argc, char ** argv);

  void PrintUsage();

};
