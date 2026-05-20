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
#include <algorithm>
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
#include "module_path.h"
#include "otype_func.h"

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

static bool ResolveModuleForMainSource(const string & module_name, const string & main_source_filename,
                                       OModulePath & rpath, string & rerror)
{
  OModulePath current_module;
  if (!current_module.InitCurrent(main_source_filename, rerror))
  {
    return false;
  }

  if (!rpath.ParseUsePath(module_name, rerror))
  {
    return false;
  }

  return rpath.ResolveFrom(current_module, rerror);
}

static void PrintModuleArtifactError(const OModulePath & module_path,
                                     const SModuleArtifactEnsureResult & result)
{
  if (EModuleArtifactEnsureError::SOURCE_MISSING == result.error)
  {
    print("Module \"{}\" source file \"{}\" was not found\n",
          module_path.module_id, module_path.source_path.string());
    return;
  }

  if (EModuleArtifactEnsureError::ARTIFACT_MISSING == result.error)
  {
    print("Module \"{}\" requires missing compiled artifact \"{}\"\n",
          module_path.module_id, module_path.artifact_path.string());
    return;
  }

  print("Can not regenerate module \"{}\" from \"{}\": {}\n",
        module_path.module_id, module_path.source_path.string(), result.reason);
}

static bool EnsureCompiledModuleArtifact(const OModulePath & module_path)
{
  OModuleIntf artifact_intf(g_builtins, module_path.module_id);
  SModuleArtifactEnsureResult result = artifact_intf.EnsureFreshCompiledArtifact(module_path);
  if (!result.Ok())
  {
    PrintModuleArtifactError(module_path, result);
    return false;
  }

  return true;
}

bool ODqCompiler::AddImplicitUse(const string & module_name, const string & namespace_name,
                                 OScope * merge_scope, bool is_private,
                                 EModuleUseMergeMode merge_mode)
{
  OModulePath module_path;
  string path_error;
  if (!ResolveModuleForMainSource(module_name, in_filename, module_path, path_error))
  {
    print("Can not resolve implicit module {}: {}\n", module_name, path_error);
    return false;
  }

  if (!EnsureCompiledModuleArtifact(module_path))
  {
    return false;
  }

  if (!g_module->UseCompiledModule(module_path.module_id, namespace_name,
                                   module_path.artifact_path.string(),
                                   module_path.artifact_path.string(), merge_scope,
                                   is_private, merge_mode, vector<string>(), false))
  {
    print("Can not load implicit module interface from \"{}\"\n", module_path.artifact_path.string());
    return false;
  }

  return true;
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

  if (!g_opt.no_use_sys)
  {
    if (!AddImplicitUse("sys", "sys", g_module->scope_pub, false, MUM_ALL))
    {
      ++errorcnt;
      return;
    }
  }

  ParseModule();
  if (errorcnt)
  {
    if (g_opt.verblevel >= VERBLEVEL_STATUS)
    {
      print("Compile error.\n");
    }
    return;
  }

  OValSym * main_sym = nullptr;
  OValSymFunc * main_func = nullptr;
  bool has_app_main = g_module->ValSymDeclared("main", &main_sym);
  if (has_app_main)
  {
    main_func = dynamic_cast<OValSymFunc *>(main_sym);
    has_app_main = (nullptr != main_func);
  }

  if (g_opt.ifgen)
  {
    if (!g_module->WriteInterface(out_filename, in_filename))
    {
      ++errorcnt;
    }
    return;
  }

  if (!g_opt.compile_only && has_app_main)
  {
    main_func->attr_has_linkage_name = true;
    main_func->attr_linkage_name = "dq_main";
    g_module->EnsureAppInitFunc(main_func->scpos);
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
    if (has_app_main)
    {
      if (!AddImplicitUse("rtl/rtl_linux", "__dq_rtl", nullptr, true, MUM_NONE))
      {
        ++errorcnt;
        return;
      }

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
