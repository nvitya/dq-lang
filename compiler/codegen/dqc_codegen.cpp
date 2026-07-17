/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

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
#include "otype_compound.h"

#include "dqc.h"
#include "scope_builtins.h"
#include "named_scopes.h"

using namespace std;

OValSymFunc * ODqCompCodegen::DqExceptionFunc(const string & name)
{
  auto CheckScope = [&](OScope * scope) -> OValSymFunc * {
    if (scope) {
      if (OValSym * vs = scope->FindValSym(name, nullptr, false)) {
        return dynamic_cast<OValSymFunc *>(vs);
      }
    }
    return nullptr;
  };

  if (g_module)
  {
    if (OValSymFunc * fn = CheckScope(g_module->scope_pub)) return fn;
    if (OValSymFunc * fn = CheckScope(g_module->scope_priv)) return fn;
  }

  if (g_module)
  {
    for (OModuleIntf * intf : g_module->loaded_modules)
    {
      if (intf && intf->name == "rtl/exception")
      {
        if (OValSymFunc * fn = CheckScope(intf->scope_pub)) return fn;
      }
    }
  }

  return nullptr;
}

LlValue * ODqCompCodegen::DqExceptionActiveValue()
{
  return nullptr;
}

LlValue * ODqCompCodegen::DqCurrentExceptionValue()
{
  if (!ll_current_exc_storage)
  {
    return nullptr;
  }
  return ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_current_exc_storage, "exc.current");
}

void ODqCompCodegen::EnsurePersonalityFn(LlFunction * func)
{
  if (!func || func->hasPersonalityFn())
  {
    return;
  }

#if defined(TARGET_WIN)
  constexpr const char * personality_name = "__gxx_personality_seh0";
#else
  constexpr const char * personality_name = "__gxx_personality_v0";
#endif

  llvm::FunctionCallee pers_fn = ll_module->getOrInsertFunction(personality_name,
      LlFuncType::get(llvm::Type::getInt32Ty(ll_ctx), {}, true));
  func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));
}

void ODqCompCodegen::DqClearException()
{
  OValSymFunc * fn = DqExceptionFunc("DqExcClear");
  if (fn && fn->ll_func)
  {
    ll_builder.CreateCall(fn->ll_func, {});
  }
}

void ODqCompCodegen::DqBeginCatch()
{
  OValSymFunc * fn = DqExceptionFunc("DqExcBeginCatch");
  if (fn && fn->ll_func)
  {
    ll_builder.CreateCall(fn->ll_func, {});
  }
}

void ODqCompCodegen::DqEndCatch()
{
  OValSymFunc * fn = DqExceptionFunc("DqExcEndCatch");
  if (fn && fn->ll_func)
  {
    ll_builder.CreateCall(fn->ll_func, {});
  }
}

void ODqCompCodegen::EmitExceptionEscapeCheck(LlBasicBlock * active_bb, LlBasicBlock * normal_bb)
{
  LlValue * active = DqExceptionActiveValue();
  if (!active)
  {
    ll_builder.CreateBr(normal_bb);
    return;
  }
  ll_builder.CreateCondBr(active, active_bb, normal_bb);
}

void ODqCompCodegen::EmitActiveFinallyBlocks()
{
  if (ll_finally_stack.empty())
  {
    return;
  }

  vector<OStmtBlock *> saved_stack = ll_finally_stack;
  while (!ll_finally_stack.empty())
  {
    OStmtBlock * finally_body = ll_finally_stack.back();
    ll_finally_stack.pop_back();
    if (finally_body)
    {
      finally_body->Generate();
      if (ll_builder.GetInsertBlock()->getTerminator())
      {
        break;
      }
    }
  }
  ll_finally_stack = saved_stack;
}

void ODqCompCodegen::GenerateExceptBranchMatch(OExceptBranch * branch, LlBasicBlock * bb_match, LlBasicBlock * bb_next)
{
  if (!branch->exception_type)
  {
    ll_builder.CreateBr(bb_match);
    return;
  }

  LlValue * ll_exc = DqCurrentExceptionValue();
  if (!ll_exc)
  {
    ll_builder.CreateBr(bb_next);
    return;
  }
  LlValue * ll_null = llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
  LlValue * is_null = ll_builder.CreateICmpEQ(ll_exc, ll_null, "exc.isnull");

  LlFunction * ll_func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * bb_check = LlBasicBlock::Create(ll_ctx, "exc.check", ll_func);

  LlBasicBlock * bb_start = ll_builder.GetInsertBlock();
  ll_builder.CreateCondBr(is_null, bb_next, bb_check);

  ll_builder.SetInsertPoint(bb_check);

  // We can get the Exception type from DqExcRaise's first parameter
  OTypeObject * exc_type = nullptr;
  OValSymFunc * fn_raise = DqExceptionFunc("DqExcRaise");
  if (fn_raise && fn_raise->ptype)
  {
    OTypeFunc * fn_type = dynamic_cast<OTypeFunc *>(fn_raise->ptype);
    if (fn_type && fn_type->params.size() > 0)
    {
      exc_type = dynamic_cast<OTypeObject *>(fn_type->params[0]->ptype->ResolveAlias());
    }
  }

  if (!exc_type)
  {
    throw runtime_error("GenerateExceptBranchMatch: missing Exception type");
  }

  // Exception is polymorphic, so we can access its TypeInfo at vtable[0]
  // The first field of the Exception object is its vtable pointer
  LlValue * ll_vptr_addr = ll_builder.CreateStructGEP(exc_type->GetLlType(), ll_exc,
      exc_type->vtable_field_index, "exc.vtable.addr");
  LlValue * ll_vptr = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_vptr_addr, "exc.vtable");

  // TypeInfo is slot 0
  LlValue * ll_ti_slot = ll_builder.CreateGEP(llvm::PointerType::get(ll_ctx, 0), ll_vptr,
      {llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0)}, "exc.ti.slot.addr");
  LlValue * ll_obj_ti = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_ti_slot, "exc.ti");

  branch->exception_type->GenVTableGlobal(false);
  LlValue * target_ti = branch->exception_type->ll_typeinfo;

  if (!branch->exception_type->is_polymorphic) {
    g_compiler->ErrorTxt(DQERR_NOT_SUPPORTED, format("catching non-polymorphic object {}", branch->exception_type->name));
  }

  LlBasicBlock * bb_loop = LlBasicBlock::Create(ll_ctx, "exc.ti.loop", ll_func);
  LlBasicBlock * bb_next_ti = LlBasicBlock::Create(ll_ctx, "exc.ti.next", ll_func);

  ll_builder.CreateBr(bb_loop);
  ll_builder.SetInsertPoint(bb_loop);

  llvm::PHINode * phi_ti = ll_builder.CreatePHI(llvm::PointerType::get(ll_ctx, 0), 2, "exc.cur.ti");
  phi_ti->addIncoming(ll_obj_ti, bb_check);

  LlValue * is_match = ll_builder.CreateICmpEQ(phi_ti, target_ti, "exc.ti.match");
  ll_builder.CreateCondBr(is_match, bb_match, bb_next_ti);

  ll_builder.SetInsertPoint(bb_next_ti);
  LlValue * is_ti_null = ll_builder.CreateICmpEQ(phi_ti, ll_null, "exc.ti.isnull");

  LlBasicBlock * bb_adv = LlBasicBlock::Create(ll_ctx, "exc.ti.adv", ll_func);
  ll_builder.CreateCondBr(is_ti_null, bb_next, bb_adv);

  ll_builder.SetInsertPoint(bb_adv);
  LlValue * base_ti_ptr = ll_builder.CreateGEP(llvm::PointerType::get(ll_ctx, 0), phi_ti,
      {llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 1)}, "exc.base.ti.ptr");
  LlValue * next_ti = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), base_ti_ptr, "exc.next.ti");
  phi_ti->addIncoming(next_ti, bb_adv);
  ll_builder.CreateBr(bb_loop);
}

void ODqCompCodegen::GenerateDqExcNativeThrowBody()
{
  OValSymFunc * vs_throw = DqExceptionFunc("DqExcNativeThrow");
  if (!vs_throw || !vs_throw->ll_func)
  {
    return;
  }

  LlFunction * f = vs_throw->ll_func;
  // If it already has a body, do not generate again
  if (!f->empty())
  {
    return;
  }

  LlBasicBlock * bb = LlBasicBlock::Create(ll_ctx, "entry", f);
  ll_builder.SetInsertPoint(bb);

  LlValue * exc_arg = f->getArg(0);

  llvm::FunctionCallee fn_alloc = ll_module->getOrInsertFunction("__cxa_allocate_exception",
      LlFuncType::get(llvm::PointerType::get(ll_ctx, 0), {LlType::getInt64Ty(ll_ctx)}, false));

  LlValue * size_val = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 8);
  LlValue * ptr = ll_builder.CreateCall(fn_alloc, {size_val}, "exc.alloc");

  ll_builder.CreateStore(exc_arg, ptr);

  llvm::FunctionCallee fn_throw = ll_module->getOrInsertFunction("__cxa_throw",
      LlFuncType::get(llvm::Type::getVoidTy(ll_ctx), {llvm::PointerType::get(ll_ctx, 0), llvm::PointerType::get(ll_ctx, 0), llvm::PointerType::get(ll_ctx, 0)}, false));

  LlValue * null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
  LlValue * type_info = ll_module->getOrInsertGlobal("_ZTIPv", llvm::PointerType::get(ll_ctx, 0));

  ll_builder.CreateCall(fn_throw, {ptr, type_info, null_ptr});
  ll_builder.CreateUnreachable();
}

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
    if (intf && intf->module_init_func)
    {
      intf->module_init_func->GenGlobalDecl(true, nullptr);
    }

    for (OIntfDecl * decl : intf->declarations)
    {
      if (!decl)
      {
        continue;
      }

      auto gen_imported_function = [](OValSym * vs)
      {
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
      };

      if (IDK_VALSYM == decl->kind)
      {
        OValSym * vs = decl->pvalsym;
        gen_imported_function(vs);
        if (vs && (VSK_FUNCTION != vs->kind))
        {
          vs->GenGlobalImportDecl();
        }
      }
      else if (auto * compound_type = dynamic_cast<OCompoundType *>(decl->ptype))
      {
        for (auto & [name, vs] : compound_type->Members()->valsyms)
        {
          (void)name;
          gen_imported_function(vs);
        }
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

  for (ODecl * decl : g_module->declarations)
  {
    if (DK_TYPE == decl->kind)
    {
      if (auto * object_type = dynamic_cast<OTypeObject *>(decl->ptype))
      {
        object_type->GenVTableGlobal(decl->ispublic);
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
  GenerateDqExcNativeThrowBody();

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

  // OptimizeIr(g_opt.optlevel);
}

void ODqCompCodegen::PrepareTarget()
{
  // Only initialize native target (not all targets)
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetAsmPrinter();

  string triple = llvm::sys::getDefaultTargetTriple();
  if (!g_opt.build_tag.empty())
  {
    if (g_opt.build_tag.find("-linux") != string::npos)
    {
      triple = "x86_64-unknown-linux-gnu";
    }
    else if (g_opt.build_tag.find("-win") != string::npos)
    {
      triple = "x86_64-w64-windows-gnu";
    }
  }

#if LLVM_VERSION_MAJOR >= 21
  llvm::Triple ll_triple(triple);
  ll_module->setTargetTriple(ll_triple);
#else
  ll_module->setTargetTriple(triple);
#endif

  string err;
  auto * target = llvm::TargetRegistry::lookupTarget(triple, err);
  if (!target) throw runtime_error(err);

#if LLVM_VERSION_MAJOR >= 21
  ll_machine = target->createTargetMachine(ll_triple, "generic", "", llvm::TargetOptions(), llvm::Reloc::PIC_);
#else
  ll_machine = target->createTargetMachine(triple, "generic", "", llvm::TargetOptions(), llvm::Reloc::PIC_);
#endif

  ll_module->setDataLayout(ll_machine->createDataLayout());

}

void ODqCompCodegen::OptimizeIr(int aoptlevel)
{
  if (0 == aoptlevel)
  {
    return;
  }

  llvm::PassBuilder PB(ll_machine);

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

  llvm::ModulePassManager MPM = (ELtoMode::FULL == g_opt.lto_mode)
      ? PB.buildLTOPreLinkDefaultPipeline(ll_optlevel)
      : PB.buildPerModuleDefaultPipeline(ll_optlevel);
  MPM.run(*ll_module, MAM);
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

void ODqCompCodegen::EmitBitcode(const string afilename)
{
  if (g_opt.verblevel >= VERBLEVEL_STATUS)
  {
    print("Writing LLVM bitcode file \"{}\"...\n", afilename);
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

  llvm::WriteBitcodeToFile(*ll_module, out);
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
