/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    ll_defs.h
 * authors: nvitya
 * created: 2026-02-19
 * brief:   LLVM defines, object
 */

#include "ll_defs.h"
#include "dqc.h"
#include "scf_dq.h"
#include "comp_options.h"

LlContext      ll_ctx;
LlBuilder      ll_builder(ll_ctx);
LlModule *     ll_module;

vector<SLoopContext>   ll_loop_stack;

LlDiBuilder *          di_builder = nullptr;
LlDiUnit *             di_unit = nullptr;
LlDiFile *             di_main_file = nullptr;
//vector<LlDiScope *>    di_scope_stack;

void ll_defs_init()
{
  ll_module = new llvm::Module("dq", ll_ctx);
  if (g_opt.dbg_info)
  {
    di_builder = new LlDiBuilder(*ll_module);
  }

  //llvm::DICompileUnit* compile_unit;
}

void ll_init_debug_info()
{
  if (not g_opt.dbg_info)
  {
    return;
  }

  ll_module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
  ll_module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 4);
  di_builder = new LlDiBuilder(*ll_module);

  OScFile * pfile = g_compiler->scf->curfile;
  di_main_file = di_builder->createFile(pfile->name, ".");
  pfile->di_file = di_main_file;
  di_unit = di_builder->createCompileUnit(llvm::dwarf::DW_LANG_C, di_main_file, "dqc", false, "", 0);

  //debugScopes.push_back(dfile);
}
