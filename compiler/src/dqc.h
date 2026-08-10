/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:   DQ Compiler Object global instance
 */

#pragma once

#include "stdint.h"
#include <string>
#include <vector>
#include "comp_options.h"

#include "dqc_clargs.h"
#include "scf_base.h"

using namespace std;

class OModulePath;
struct SModuleArtifactEnsureResult;

class ODqCompiler : public ODqCompClargs
{
private:
  using            super = ODqCompClargs;

public:
  string ModuleStackNameFromInput() const;
  bool ResolveModuleForMainSource(const string & module_name,
                                  OModulePath & rpath, string & rerror) const;
  void PrintModuleArtifactError(const OModulePath & module_path,
                                const SModuleArtifactEnsureResult & result) const;
  bool EnsureCompiledModuleArtifact(const OModulePath & module_path) const;
  bool AddImplicitUse(const string & module_name, const string & namespace_name,
                      OScope * merge_scope, bool is_private,
                      EModuleUseMergeMode merge_mode);
  string HostedRtlModuleName() const;
  string DefaultLinkDriver() const;
  bool LinkInputForArtifact(const string & artifact_filename, string & rinput_filename, string & rerror) const;
  bool BuildLinkArgs(const string & object_filename, const string & executable_filename,
                     vector<string> & rargs, string & rerror) const;
  string FormatLinkCommandForLog(const vector<string> & args) const;
  bool LinkExecutable(const string & object_filename, const string & executable_filename) const;

  ODqCompiler();
  virtual ~ODqCompiler();

  void Run(int argc, char ** argv);
};

extern ODqCompiler *  g_compiler;

void dqc_init();
