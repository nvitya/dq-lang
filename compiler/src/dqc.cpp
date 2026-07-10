/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
#include <system_error>

#include "ll_defs.h"
#include "named_scopes.h"
#include "scope_builtins.h"
#include "scope_defines.h"

#include "dqc.h"
#include "comp_config.h"
#include "dq_module.h"
#include "version.h"
#include "artifact_lock.h"
#include "module_path.h"
#include "otype_func.h"
#include "processrunner.h"

ODqCompiler *  g_compiler = nullptr;

ODqCompiler::ODqCompiler()
{
}

ODqCompiler::~ODqCompiler()
{
}

string ODqCompiler::ModuleStackNameFromInput() const
{
  filesystem::path p(base_name);
  return p.filename().string();
}

bool ODqCompiler::ResolveModuleForMainSource(const string & module_name,
                                             OModulePath & rpath, string & rerror) const
{
  OModulePath current_module;
  if (!current_module.InitCurrent(in_filename, rerror))
  {
    return false;
  }

  if (!rpath.ParseUsePath(module_name, rerror))
  {
    return false;
  }

  return rpath.ResolveFrom(current_module, rerror);
}

void ODqCompiler::PrintModuleArtifactError(const OModulePath & module_path,
                                           const SModuleArtifactEnsureResult & result) const
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

bool ODqCompiler::EnsureCompiledModuleArtifact(const OModulePath & module_path) const
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
  if (!ResolveModuleForMainSource(module_name, module_path, path_error))
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

string ODqCompiler::HostedRtlModuleName() const
{
#if defined(TARGET_WIN)
  return "rtl/rtl_windows";
#else
  return "rtl/rtl_linux";
#endif
}

string ODqCompiler::DefaultLinkDriver() const
{
  const char * env_driver = getenv("DQ_LINKER_DRIVER");
  if (env_driver && env_driver[0])
  {
    return env_driver;
  }

#if defined(TARGET_WIN)
  if (!g_opt.compiler_executable_dir.empty())
  {
    filesystem::path bin_dir = g_opt.compiler_executable_dir;
    filesystem::path root_dir = (bin_dir / "..").lexically_normal();

    vector<filesystem::path> candidates = {
      root_dir / "toolchain" / "llvm-mingw" / "bin" / "clang++.exe",
      root_dir / "toolchain" / "bin" / "clang++.exe",
      root_dir / "llvm-mingw" / "bin" / "clang++.exe",
      bin_dir / "clang++.exe"
    };

    error_code ec;
    auto add_toolchain_dir_candidates = [&](const filesystem::path & parent_dir)
    {
      ec.clear();
      if (!filesystem::is_directory(parent_dir, ec) || ec)
      {
        return;
      }

      ec.clear();
      for (const filesystem::directory_entry & entry : filesystem::directory_iterator(parent_dir, ec))
      {
        if (ec)
        {
          break;
        }
        ec.clear();
        if (entry.is_directory(ec) && !ec)
        {
          candidates.push_back(entry.path() / "bin" / "clang++.exe");
        }
      }
    };
    add_toolchain_dir_candidates(root_dir);
    add_toolchain_dir_candidates(root_dir / "toolchain");

    for (const filesystem::path & path : candidates)
    {
      ec.clear();
      if (filesystem::is_regular_file(path, ec) && !ec)
      {
        return path.lexically_normal().string();
      }
    }
  }

  return "clang++.exe";
#else
  return "clang++";
#endif
}

bool ODqCompiler::LinkInputForArtifact(const string & artifact_filename, string & rinput_filename,
                                       string & rerror) const
{
  if (ELtoMode::FULL != g_opt.lto_mode)
  {
    rinput_filename = artifact_filename;
    return true;
  }

  filesystem::path bitcode_path = ArtifactBitcodeSidecarPathForObject(artifact_filename);
  error_code ec;
  if (!filesystem::exists(bitcode_path, ec) || ec)
  {
    rerror = format("LTO bitcode sidecar is missing for \"{}\": \"{}\"",
                    artifact_filename, bitcode_path.string());
    return false;
  }

  rinput_filename = bitcode_path.string();
  return true;
}

bool ODqCompiler::BuildLinkArgs(const string & object_filename, const string & executable_filename,
                                vector<string> & rargs, string & rerror) const
{
  rargs.clear();
  rargs.push_back(DefaultLinkDriver());

#if defined(TARGET_WIN)
  #if defined(TARGET_64BIT)
    rargs.push_back("--target=x86_64-w64-windows-gnu");
  #else
    rargs.push_back("--target=i686-w64-windows-gnu");
  #endif
#elif defined(TARGET_LINUX)
  #if defined(HOST_X86)
    #if defined(TARGET_64BIT)
      rargs.push_back("--target=x86_64-unknown-linux-gnu");
    #else
      rargs.push_back("--target=i386-unknown-linux-gnu");
    #endif
  #elif defined(HOST_ARM)
    #if defined(TARGET_64BIT)
      rargs.push_back("--target=aarch64-unknown-linux-gnu");
    #else
      rargs.push_back("--target=arm-unknown-linux-gnueabihf");
    #endif
  #elif defined(HOST_RISCV)
    #if defined(TARGET_64BIT)
      rargs.push_back("--target=riscv64-unknown-linux-gnu");
    #else
      rargs.push_back("--target=riscv32-unknown-linux-gnu");
    #endif
  #endif
#endif

  rargs.push_back("-fuse-ld=lld");
  if (ELtoMode::FULL == g_opt.lto_mode)
  {
    rargs.push_back("-flto=full");
    rargs.push_back(format("-O{}", g_opt.optlevel));
  }

  string link_input;
  if (!LinkInputForArtifact(object_filename, link_input, rerror))
  {
    return false;
  }
  rargs.push_back(link_input);
  for (const string & artifact_path : g_module->link_module_artifacts)
  {
    if (!LinkInputForArtifact(artifact_path, link_input, rerror))
    {
      return false;
    }
    rargs.push_back(link_input);
  }
  rargs.push_back("-o");
  rargs.push_back(executable_filename);
  for (const string & libname : g_opt.link_libraries)
  {
    rargs.push_back("-l" + libname);
  }

  return true;
}

string ODqCompiler::FormatLinkCommandForLog(const vector<string> & args) const
{
  string result;
  for (const string & arg : args)
  {
    if (!result.empty())
    {
      result += " ";
    }

    if (arg.empty() || (arg.find_first_of(" \t\n\"") != string::npos))
    {
      result += "\"";
      for (char c : arg)
      {
        if ('"' == c)
        {
          result += "\\\"";
        }
        else
        {
          result.push_back(c);
        }
      }
      result += "\"";
    }
    else
    {
      result += arg;
    }
  }
  return result;
}

bool ODqCompiler::LinkExecutable(const string & object_filename, const string & executable_filename) const
{
  vector<string> args;
  string args_error;
  if (!BuildLinkArgs(object_filename, executable_filename, args, args_error))
  {
    print("{}\n", args_error);
    return false;
  }

  if (g_opt.verblevel >= VERBLEVEL_STATUS)
  {
    print("Linking: \"{}\"...\n", executable_filename);
  }
  if (g_opt.verblevel >= VERBLEVEL_INFO)
  {
    print("Link cmd: {}\n", FormatLinkCommandForLog(args));
  }

  int rc = -1;
  string errtxt;
  bool exec_ok = RunInteractiveProcess(args, rc, &errtxt);
  if (!exec_ok && !errtxt.empty())
  {
    print("{}\n", errtxt);
  }
  if (!exec_ok || (0 != rc))
  {
    print("Link error.\n");
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
    g_opt.module_use_stack.push_back(ModuleStackNameFromInput());
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

  bool use_sys = !g_opt.no_use_sys;
  OModulePath current_module;
  string module_error;
  if (use_sys && current_module.InitCurrent(in_filename, module_error))
  {
    use_sys = !OModulePath::SuppressesImplicitSys(current_module.module_id, current_module.root_dir);
  }

  if (use_sys)
  {
    if (!AddImplicitUse("rtl/sys", "sys", g_module->scope_pub, false, MUM_ALL))
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

  OValSymFunc * main_func = g_module->app_main_func;
  bool has_app_main = (nullptr != main_func);

  if (g_opt.ifgen)
  {
    // Release the exclusive file lock on the output artifact before replacing it.
    output_artifact_lock.Unlock();

    if (!g_module->WriteInterface(out_filename, in_filename))
    {
      ++errorcnt;
    }
    return;
  }

  if (!g_opt.compile_only && has_app_main)
  {
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

  if (ELtoMode::FULL == g_opt.lto_mode)
  {
    EmitBitcode(ArtifactBitcodeSidecarPathForObject(out_filename).string());
  }

  if (g_opt.ir_print)
  {
    PrintIr();
  }

  // Release the exclusive file lock on the output artifact before replacing it.
  // MoveFileExW can fail with ERROR_ACCESS_DENIED if the destination file is still open by this process.
  output_artifact_lock.Unlock();

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
      if (!AddImplicitUse(HostedRtlModuleName(), "__dq_rtl", nullptr, true, MUM_NONE))
      {
        ++errorcnt;
        return;
      }

      if (!LinkExecutable(out_filename, link_output))
      {
        ++errorcnt;
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
