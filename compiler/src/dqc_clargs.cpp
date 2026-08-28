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
#include <print>

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

void ODqCompClargs::ParseCmdLineArgs(int argc, char ** argv)
{
  string explicit_output;
  has_dash_o = false;
  has_output = false;
  bool project_argument_seen = false;

  if (!g_opt.project_main_filename.empty())
  {
    in_filename = g_opt.project_main_filename;
    if (g_opt.project_has_output)
    {
      explicit_output = g_opt.project_output_filename;
      has_output = true;
    }
  }

  for (int i = 1; i < argc; ++i)
  {
    string v(argv[i]);
    if (!v.empty() && ('-' == v[0]))
    {
      if ("-o" == v)
      {
        ++i;  // ProcessCommandLineOpts already validated the argument.
        explicit_output = argv[i];
        has_dash_o = true;
        has_output = true;
      }
      else if (OCompOptions::CommandLineOptionHasValue(v))
      {
        ++i;  // ProcessCommandLineOpts already validated the argument.
      }
      continue;
    }

    if (!g_opt.project_filename.empty() && !project_argument_seen && v.ends_with(".dqproj"))
    {
      project_argument_seen = true;
    }
    else if (in_filename.empty())
    {
      in_filename = v;
    }
    else if (!has_dash_o)
    {
      // backward compatibility: second positional arg = output name
      explicit_output = v;
      has_dash_o = true;
      has_output = true;
    }
    else
    {
      ++errorcnt;
      print("Unexpected argument: {}\n", v);
      OCompOptions::PrintUsage();
      return;
    }
  }

  if (g_opt.print_version) return;

  if (in_filename.empty())
  {
    ++errorcnt;
    print("Input file name is missing.\n");
    OCompOptions::PrintUsage();
    return;
  }

  if (g_opt.ifdump)
  {
    if (has_output)
    {
      ++errorcnt;
      print("--ifdump expects only one input .dqm_if file.\n");
      OCompOptions::PrintUsage();
    }
    return;
  }

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
