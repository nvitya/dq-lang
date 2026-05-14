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
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/DerivedTypes.h>

#include <print>
#include <format>
#include <filesystem>

#include "dqc_codegen.h"
#include "artifact_lock.h"

using namespace std;

void ODqCompCodegen::GenerateIr()
{
  if (g_opt.verblevel >= VERBLEVEL_INFO)
  {
    print("Generating IR...\n");
  }

  PrepareTarget();

  // predeclare functions first so later global initializers can reference them
  for (OModuleIntf * intf : g_module->loaded_modules)
  {
    for (OIntfDecl * decl : intf->declarations)
    {
      if (!decl || (IDK_VALSYM != decl->kind))
      {
        continue;
      }

      OValSym * vs = decl->pvalsym;
      if (auto * vsfunc = dynamic_cast<OValSymFunc *>(vs))
      {
        vsfunc->GenGlobalDecl(true, nullptr);
      }
      else if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
      {
        for (OValSymFunc * fn : ovset->funcs)
        {
          fn->GenGlobalDecl(true, nullptr);
        }
      }
      else
      {
        vs->GenGlobalImportDecl();
      }
    }
  }

  for (ODecl * decl : g_module->declarations)
  {
    if (DK_VALSYM == decl->kind)
    {
      OValSym * vs = decl->pvalsym;
      if (auto * vsfunc = dynamic_cast<OValSymFunc *>(vs))
      {
        vsfunc->GenGlobalDecl(decl->ispublic, decl->initvalue);
      }
      else if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
      {
        for (OValSymFunc * fn : ovset->funcs)
        {
          fn->GenGlobalDecl(decl->ispublic, nullptr);
        }
      }
    }
  }

  // generate other global declarations

  for (ODecl * decl : g_module->declarations)
  {
    if (DK_VALSYM == decl->kind)
    {
      OValSym * vs = decl->pvalsym;
      if (dynamic_cast<OValSymOverloadSet *>(vs))
      {
        continue;
      }

      if (!dynamic_cast<OValSymFunc *>(vs))
      {
        vs->GenGlobalDecl(decl->ispublic, decl->initvalue);
      }
    }
    else if (DK_TYPE == decl->kind)
    {
      // Type declarations are semantic interface data. They do not emit globals.
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
      else if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
      {
        for (OValSymFunc * fn : ovset->funcs)
        {
          fn->GenerateFuncBody();
        }
      }
    }
  }

  if (g_opt.dbg_info)
  {
    di_builder->finalize();
  }

  OptimizeIr(g_opt.optlevel);
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

void ODqCompCodegen::EmbedDqmIfSection(const vector<uint8_t> & adata)
{
  if (adata.empty())
  {
    return;
  }

  llvm::ArrayRef<uint8_t> bytes(adata.data(), adata.size());
  llvm::Constant * init = llvm::ConstantDataArray::get(ll_ctx, bytes);
  auto * gv = new llvm::GlobalVariable(
      *ll_module,
      init->getType(),
      true,
      llvm::GlobalValue::PrivateLinkage,
      init,
      "__dq_dqm_if");
  gv->setSection(".dqm_if");
  gv->setAlignment(llvm::Align(1));

  llvm::Type * ptr_type = llvm::PointerType::get(ll_ctx, 0);
  llvm::ArrayType * used_type = llvm::ArrayType::get(ptr_type, 1);
  llvm::Constant * used_init = llvm::ConstantArray::get(used_type, { gv });
  auto * used = new llvm::GlobalVariable(
      *ll_module,
      used_type,
      false,
      llvm::GlobalValue::AppendingLinkage,
      used_init,
      "llvm.compiler.used");
  used->setSection("llvm.metadata");
}

void ODqCompCodegen::EmitObject(const string afilename)
{
  if (g_opt.verblevel >= VERBLEVEL_STATUS)
  {
    print("Writing object file \"{}\"...\n", afilename);
  }

  string dir_error;
  if (!ArtifactEnsureParentDir(afilename, dir_error))
  {
    throw runtime_error(dir_error);
  }

  filesystem::path tmp_path = ArtifactTempPathFor(afilename);
  error_code ec;
  llvm::raw_fd_ostream out(tmp_path.string(), ec, llvm::sys::fs::OF_None);
  if (ec)
  {
    ArtifactRemoveNoError(tmp_path);
    throw runtime_error(ec.message());
  }

  llvm::legacy::PassManager pm;
  ll_machine->addPassesToEmitFile(pm, out, nullptr, llvm::CodeGenFileType::ObjectFile);
  pm.run(*ll_module);
  out.flush();
  out.close();

  string publish_error;
  if (!ArtifactAtomicReplace(tmp_path, afilename, publish_error))
  {
    throw runtime_error(publish_error);
  }
}

void ODqCompCodegen::PrintIr()
{
  print("=== LLVM IR ===\n");
  ll_module->print(llvm::outs(), nullptr);
  print("===============\n\n");
}
