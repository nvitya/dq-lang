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

#include "ll_defs.h"
#include "named_scopes.h"
#include "scope_builtins.h"
#include "scope_defines.h"

#include "dqc.h"
#include "dq_module.h"

ODqCompiler *  g_compiler = nullptr;

ODqCompiler::ODqCompiler()
{
}

ODqCompiler::~ODqCompiler()
{
}

void ODqCompiler::Run(int argc, char ** argv)
{
  errorcnt = 0;
  OScPosition scpos;

  ParseCmdLineArgs(argc, argv);
  if (errorcnt)
  {
    return;
  }

  if (g_opt.print_version)
  {
    return;
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
    print("Compile error.\n");
    return;
  }

  GenerateIr();
  if (errorcnt)
  {
    print("Code generation error.\n");
    return;
  }

  if (g_opt.ir_print)
  {
    PrintIr();
  }

  EmitObject(out_filename);
  if (errorcnt)
  {
    return;
  }

  // linking decision
  if (!g_opt.compile_only)
  {
    OValSym * main_sym = nullptr;
    bool has_main = g_module->ValSymDeclared("main", &main_sym);

    if (has_main)
    {
      string link_cmd = format("gcc {} -o {}", out_filename, link_output);
      if (g_opt.verbose)  print("Linking: {}\n", link_cmd);
      print("Linking: \"{}\"...\n", link_output);

      int rc = system(link_cmd.c_str());
      if (rc != 0)
      {
        ++errorcnt;
        print("Link error.\n");
      }
    }
    else if (has_dash_o)
    {
      // no main(), no linking — rename .o to -o target if different
      if (out_filename != link_output)
      {
        rename(out_filename.c_str(), link_output.c_str());
      }
    }
  }

  if (0 == errorcnt)
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
