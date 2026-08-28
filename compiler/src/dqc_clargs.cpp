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
    error_code ec;
    filesystem::path input_path = filesystem::absolute(in_filename, ec);
    if (ec) input_path = in_filename;
    g_opt.build_root_dir = input_path.parent_path().lexically_normal().string();
  }
  else
  {
    error_code ec;
    filesystem::path build_root = filesystem::absolute(g_opt.build_root_dir, ec);
    g_opt.build_root_dir = (ec ? filesystem::path(g_opt.build_root_dir) : build_root).lexically_normal().string();
  }

  // derive base_name by stripping .dq extension
  if (in_filename.size() > 3 && in_filename.substr(in_filename.size() - 3) == ".dq")
  {
    base_name = in_filename.substr(0, in_filename.size() - 3);
  }
  else
  {
    base_name = in_filename;
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
    if (!has_output && g_opt.target.IsBare()) link_output += ".elf";
  }
}
