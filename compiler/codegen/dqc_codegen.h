/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_codegen.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#pragma once

#include "stdint.h"
#include <string>
#include <vector>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include "comp_options.h"
#include "dqc_parser.h"

#include "ll_defs.h"

using namespace std;

class ODqCompCodegen : public ODqCompParser
{
private:
  using                super = ODqCompParser;

public:
  LlMachine *          ll_machine = nullptr;

  ODqCompCodegen()
  {
  }

  virtual ~ODqCompCodegen() {}

  LlValue *     ll_exn_storage = nullptr;
  LlValue *     ll_current_exc_storage = nullptr;
  vector<OStmtBlock *> ll_finally_stack;

  OValSymFunc * DqExceptionFunc(const string & name);
  LlValue *     DqExceptionActiveValue();
  LlValue *     DqCurrentExceptionValue();
  void          DqClearException();
  void          DqBeginCatch();
  void          DqEndCatch();
  void          EmitExceptionEscapeCheck(LlBasicBlock * active_bb, LlBasicBlock * normal_bb);
  void          EmitActiveFinallyBlocks();
  void          GenerateExceptBranchMatch(OExceptBranch * branch, LlBasicBlock * bb_match, LlBasicBlock * bb_next);
  void          EnsurePersonalityFn(LlFunction * func);

  void PrepareTarget();
  void GenerateIr();
  void GenerateDqExcNativeThrowBody();
  void OptimizeIr(int aoptlevel);
  void PrintIr();

  void EmbedDqmIfSection(const vector<uint8_t> & adata);
  void EmitBitcode(const string afilename);
  void EmitObject(const string afilename);

};
