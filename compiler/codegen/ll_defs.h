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
 * brief:   LLVM defines, global objects
 */

#pragma once

#include <vector>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/Target/TargetMachine.h>

using namespace std;

// Short Aliases
using LlContext      = llvm::LLVMContext;
using LlModule       = llvm::Module;
using LlBuilder      = llvm::IRBuilder<>;
using LlMachine      = llvm::TargetMachine;

using LlValue        = llvm::Value;
using LlType         = llvm::Type;
using LlConst        = llvm::Constant;
using LlFunction     = llvm::Function;
using LlFuncType     = llvm::FunctionType;
using LlBasicBlock   = llvm::BasicBlock;

using LlLinkType     = llvm::GlobalValue::LinkageTypes;

using LlDiBuilder    = llvm::DIBuilder;
using LlDiUnit       = llvm::DICompileUnit;
using LlDiFile       = llvm::DIFile;
using LlDiScope      = llvm::DIScope;
using LlDiSubPrg     = llvm::DISubprogram;
using LlDiType       = llvm::DIType;

// global variables

extern LlContext     ll_ctx;
extern LlBuilder     ll_builder;
extern LlModule *    ll_module;

extern LlDiBuilder * di_builder;
extern LlDiUnit *    di_unit;
extern LlDiFile *    di_main_file;

// Loop context for break/continue
struct SLoopContext
{
  LlBasicBlock *  cond_bb;  // continue target
  LlBasicBlock *  end_bb;   // break target
};

extern vector<SLoopContext>  ll_loop_stack;

extern vector<LlDiScope *>   di_scope_stack;

void ll_defs_init();
void ll_init_debug_info();