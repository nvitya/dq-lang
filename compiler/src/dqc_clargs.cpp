/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_clargs.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include <filesystem>
#include "dqc_clargs.h"
#include "module_path.h"
#include "artifact_lock.h"
#include "dq_utils.h"

using namespace std;

ODqCompClargs::ODqCompClargs()
{
}

ODqCompClargs::~ODqCompClargs()
{
}

void ODqCompClargs::PrepareOutputPaths()
{
  in_filename = g_opt.input_filename;
  has_dash_o = g_opt.has_dash_o;
  has_output = g_opt.has_output;
  const string & explicit_output = g_opt.explicit_output;

  if (g_opt.print_version || g_opt.ifdump) return;

  if (g_opt.build_root_dir.empty())
  {
    // A project owns its complete build tree, even when its main source is in
    // a subdirectory.
    const string & build_owner = g_opt.project_filename.empty() ? in_filename : g_opt.project_filename;
    g_opt.build_root_dir = AbsNormPath(build_owner).parent_path().string();
  }
  else
  {
    g_opt.build_root_dir = AbsNormPath(g_opt.build_root_dir).string();
  }

  if (!g_opt.package_build_root_dir.empty())
  {
    g_opt.package_build_root_dir = AbsNormPath(g_opt.package_build_root_dir).string();
  }

  // A project file names the application; otherwise the main source does.
  const string & output_owner = g_opt.project_filename.empty() ? in_filename : g_opt.project_filename;
  if (!g_opt.project_filename.empty())
  {
    base_name = filesystem::path(output_owner).replace_extension().string();
  }
  // derive base_name by stripping .dq extension
  else if (output_owner.size() > 3 && output_owner.substr(output_owner.size() - 3) == ".dq")
  {
    base_name = output_owner.substr(0, output_owner.size() - 3);
  }
  else
  {
    base_name = output_owner;
  }

  OModulePath current_module;
  string module_error;
  filesystem::path default_artifact_path;
  filesystem::path default_interface_path;
  if (current_module.InitCurrent(in_filename, module_error))
  {
    default_artifact_path = current_module.artifact_path;
    default_interface_path = current_module.interface_artifact_path;
  }
  else
  {
    default_artifact_path = OModulePath::BuildArtifactPath(in_filename);
    default_interface_path = OModulePath::BuildInterfaceArtifactPath(in_filename);
  }

  if (g_opt.ifgen)
  {
    out_filename = has_output ? explicit_output : default_interface_path.string();
    interface_out_filename = out_filename;
  }
  else if ((DQC_LINK_COMPILE_ONLY == g_opt.link_mode)
           || ((DQC_LINK_AUTO == g_opt.link_mode) && g_opt.target.IsBare()))
  {
    // Explicit compile-only and automatic bare builds produce an object directly.
    out_filename = has_output ? explicit_output : default_artifact_path.string();
    interface_out_filename = ArtifactInterfacePathForObject(out_filename).string();
  }
  else
  {
    // Full compilation produces a DQ module object and its interface.
    out_filename = default_artifact_path.string();
    interface_out_filename = default_interface_path.string();
    link_output = has_output ? explicit_output : base_name;
    if (!has_output && g_opt.target.IsWasi()) link_output += ".wasm";
    if (!has_output && g_opt.target.IsBare()) link_output += ".elf";
  }
}
