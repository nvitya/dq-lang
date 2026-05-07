/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
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

  void PrepareTarget();
  void GenerateIr();
  void OptimizeIr(int aoptlevel);
  void PrintIr();

  void EmbedDqmIfSection(const vector<uint8_t> & adata);
  void EmitObject(const string afilename);

};
