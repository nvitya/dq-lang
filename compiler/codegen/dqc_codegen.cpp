/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_codegen.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

// these include also provide llvm::format() so the std::format() must be fully specified
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/TargetParser/Host.h>

#include <llvm/Analysis/TargetLibraryInfo.h>

#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/PassManager.h>

#include <print>
#include <format>

#include "dqc_codegen.h"

using namespace std;

void ODqCompCodegen::GenerateIr()
{
  print("Generating IR...\n");

  PrepareTarget();

  // generate declarations

  for (ODecl * decl : g_module->declarations)
  {
    if (DK_VALSYM == decl->kind)
    {
      OValSym * vs = decl->pvalsym;
      vs->GenDeclaration(decl->ispublic, decl->initvalue);
    }
    else if (DK_TYPE == decl->kind)
    {
      OType * pt = decl->ptype;
      print("Unhandled type declaration \"{}\"\n", pt->name);
    }
  }

  // generate function bodies

  for (ODecl * decl : g_module->declarations)
  {
    if (DK_VALSYM == decl->kind)
    {
      OValSym * vs = decl->pvalsym;
      OValSymFunc * vsfunc = dynamic_cast<OValSymFunc *>(vs);
      if (vsfunc)
      {
        vsfunc->GenerateFuncBody();
      }
    }
  }

  if (g_opt.dbg_info)
  {
    di_builder->finalize();
  }

  OptimizeIr(0);
  //OptimizeIr(1);
}

void ODqCompCodegen::PrepareTarget()
{
  // Only initialize native target (not all targets)
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetAsmPrinter();

  auto triple = llvm::sys::getDefaultTargetTriple();
  ll_module->setTargetTriple(triple);

  string err;
  auto * target = llvm::TargetRegistry::lookupTarget(triple, err);
  if (!target) throw runtime_error(err);

  ll_machine = target->createTargetMachine(triple, "generic", "", llvm::TargetOptions(), llvm::Reloc::PIC_);

  ll_module->setDataLayout(ll_machine->createDataLayout());

}

void ODqCompCodegen::OptimizeIr(int aoptlevel)
{
  if (0 == aoptlevel)
  {
    return;
  }

  llvm::PassBuilder PB;

  llvm::LoopAnalysisManager     LAM;
  llvm::FunctionAnalysisManager FAM;
  llvm::CGSCCAnalysisManager    CGAM;
  llvm::ModuleAnalysisManager   MAM;

  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  llvm::OptimizationLevel ll_optlevel;

  if (2 == aoptlevel)
  {
    ll_optlevel = llvm::OptimizationLevel::O2;
  }
  else if (3 == aoptlevel)
  {
    ll_optlevel = llvm::OptimizationLevel::O3;
  }
  else
  {
    ll_optlevel = llvm::OptimizationLevel::O1;
  }

  llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(ll_optlevel);
  MPM.run(*ll_module, MAM);
}

void ODqCompCodegen::EmitObject(const string afilename)
{
  print("Writing object file \"{}\"...\n", afilename);

  error_code ec;
  llvm::raw_fd_ostream out(afilename, ec, llvm::sys::fs::OF_None);
  if (ec) throw runtime_error(ec.message());

  llvm::legacy::PassManager pm;
  ll_machine->addPassesToEmitFile(pm, out, nullptr, llvm::CodeGenFileType::ObjectFile);
  pm.run(*ll_module);
  out.flush();
}

void ODqCompCodegen::PrintIr()
{
  print("=== LLVM IR ===\n");
  ll_module->print(llvm::outs(), nullptr);
  print("===============\n\n");
}
