/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:   DQ Compiler Object global instance
 */

#include <print>
#include <format>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <filesystem>

#include "ll_defs.h"
#include "named_scopes.h"
#include "scope_builtins.h"
#include "scope_defines.h"

#include "dqc.h"
#include "dq_module.h"
#include "version.h"
#include "artifact_lock.h"

ODqCompiler *  g_compiler = nullptr;

ODqCompiler::ODqCompiler()
{
}

ODqCompiler::~ODqCompiler()
{
}

static string ModuleStackNameFromInput(const string & base_name)
{
  filesystem::path p(base_name);
  return p.filename().string();
}

void ODqCompiler::Run(int argc, char ** argv)
{
  errorcnt = 0;
  OScPosition scpos;

  ParseCmdLineArgsVerblevel(argc, argv);
  if (g_opt.verblevel >= VERBLEVEL_STATUS)
  {
    print("DQ Compiler - v{}\n", DQ_COMPILER_VERSION);
  }

  ParseCmdLineArgs(argc, argv);
  if (errorcnt)
  {
    return;
  }

  if (g_opt.print_version) // print only the version
  {
    print("{}\n", DQ_COMPILER_VERSION);
    return;
  }

  if (g_opt.ifdump)
  {
    if (!DumpModuleInterface(in_filename))
    {
      ++errorcnt;
    }
    return;
  }

  OArtifactLock output_artifact_lock;
  if (!out_filename.empty())
  {
    if (!output_artifact_lock.Lock(out_filename, EArtifactLockMode::EXCLUSIVE))
    {
      ++errorcnt;
      print("{}\n", output_artifact_lock.error);
      return;
    }

    if (g_opt.regen_if_stale)
    {
      OModuleIntf artifact_intf(g_builtins, "regen_check");
      string stale_reason;
      if (artifact_intf.CompiledArtifactIsFresh(out_filename, in_filename, stale_reason, false))
      {
        if (!g_opt.ifgen)
        {
          ArtifactCleanupInterfaceSidecarForObject(out_filename);
        }
        return;
      }
    }
  }

  if (g_opt.module_use_stack.empty())
  {
    g_opt.module_use_stack.push_back(ModuleStackNameFromInput(base_name));
  }
  g_module->name = g_opt.module_use_stack.back();

  if ((g_opt.verblevel >= VERBLEVEL_STATUS) and not in_filename.empty())
  {
    print("Compiling: \"{}\"...\n", in_filename);
  }

  for (const OCmdLineDefine & def : g_opt.cmdline_defines)
  {
    if (def.has_bool_value)
    {
      g_defines->DefineValSym(g_builtins->type_bool->CreateConst(scpos, def.name, def.bool_value));
    }
    else if (def.has_int_value)
    {
      g_defines->DefineValSym(g_builtins->native_int->CreateConst(scpos, def.name, def.int_value));
    }
    else
    {
      g_defines->DefineValSym(g_builtins->type_bool->CreateConst(scpos, def.name, true));
    }
  }
  if (errorcnt)
  {
    return;
  }

  // initialize the source code feeder:
  if (scf->Init(in_filename) != 0)
  {
    ++errorcnt;
    return;
  }

  ll_init_debug_info();

  ParseModule();
  if (errorcnt)
  {
    if (g_opt.verblevel >= VERBLEVEL_STATUS)
    {
      print("Compile error.\n");
    }
    return;
  }

  if (g_opt.ifgen)
  {
    if (!g_module->WriteInterface(out_filename, in_filename))
    {
      ++errorcnt;
    }
    return;
  }

  GenerateIr();
  if (errorcnt)
  {
    print("Code generation error.\n");
    return;
  }

  vector<uint8_t> dqm_if_data;
  if (!g_module->BuildInterfaceBytes(dqm_if_data, in_filename))
  {
    ++errorcnt;
    return;
  }
  EmbedDqmIfSection(dqm_if_data);

  if (g_opt.ir_print)
  {
    PrintIr();
  }

  EmitObject(out_filename);
  if (errorcnt)
  {
    return;
  }
  ArtifactCleanupInterfaceSidecarForObject(out_filename);

  // linking decision
  if (!g_opt.compile_only)
  {
    OValSym * main_sym = nullptr;
    bool has_main = g_module->ValSymDeclared("main", &main_sym);

    if (has_main)
    {
      string link_cmd = format("gcc {}", out_filename);
      for (const string & artifact_path : g_module->link_module_artifacts)
      {
        link_cmd += format(" {}", artifact_path);
      }
      link_cmd += format(" -o {} -lm", link_output);
      for (const string & libname : g_opt.link_libraries)
      {
        link_cmd += format(" -l{}", libname);
      }
      if (g_opt.verblevel >= VERBLEVEL_STATUS)
      {
        print("Linking: \"{}\"...\n", link_output);
      }
      if (g_opt.verblevel >= VERBLEVEL_INFO)
      {
        print("Link cmd: {}\n", link_cmd);
      }


      int rc = system(link_cmd.c_str());
      if (rc != 0)
      {
        ++errorcnt;
        print("Link error.\n");
      }
    }
    else if (has_dash_o)
    {
      // no main(), no linking: rename the module object to the requested target
      if (out_filename != link_output)
      {
        rename(out_filename.c_str(), link_output.c_str());
      }
    }
  }

  if ((0 == errorcnt) and (g_opt.verblevel >= VERBLEVEL_STATUS))
  {
    print("OK.\n");
  }

  return;
}

void dqc_init()
{
  ll_defs_init();
  init_scope_builtins();
  init_scope_defines();

  init_dq_module();
  init_named_scopes();

  g_compiler = new ODqCompiler();
}
