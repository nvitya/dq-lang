/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_bool.h
 * authors: nvitya
 * created: 2026-02-02
 * brief:   "bool" (boolean) type implementation
 */

#pragma once

#include "symbols.h"

class OValueBool : public OValue
{
private:
  using        super = OValue;

public:
  bool         value;

  OValueBool(OType * atype, bool avalue)
  :
    super(atype)
  {
    value = avalue;
  }

  LlConst *  CreateLlConst() override;
  bool       CalculateConstant(OExpr * expr, bool emit_errors = true) override;
};

class OTypeBool : public OType
{
private:
  using        super = OType;

public:
  OTypeBool()
  :
    super("bool", TK_BOOL)
  {
    bytesize = 1;
  }

  OValue * CreateValue() override
  {
    return new OValueBool(this, false);
  }

  OValSymConst * CreateConst(OScPosition & apos, const string aname, bool avalue)
  {
    OValueBool * v = new OValueBool(this, avalue);
    return new OValSymConst(apos, aname, this, v);  // takes over the ownership of v
  }

  LlType * CreateLlType() override
  {
    return LlType::getInt1Ty(ll_ctx);
  }

  LlDiType * CreateDiType() override
  {
    return di_builder->createBasicType("bool", 1, llvm::dwarf::DW_ATE_boolean);
  }
};

