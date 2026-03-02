/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_float.h
 * authors: nvitya
 * created: 2026-03-01
 * brief:   float types implementation
 */

#pragma once

#include "symbols.h"

class OValueFloat : public OValue
{
private:
  using        super = OValue;

public:
  double       value;

  OValueFloat(OType * atype, double avalue)
  :
    super(atype)
  {
    value = avalue;
  }

  LlConst *  CreateLlConst() override;
  bool       CalculateConstant(OExpr * expr) override;
};

class OTypeFloat : public OType
{
private:
  using        super = OType;

public:
  uint8_t      bitlength;

  OTypeFloat(const string name, uint8_t abitlength)
  :
    bitlength(abitlength),
    super(name, TK_FLOAT)
  {
    bytesize = ((abitlength + 7) >> 3);
  }

  OValue * CreateValue() override
  {
    return new OValueFloat(this, 0.0);
  }

  OValSymConst * CreateConst(OScPosition & apos, const string aname, double avalue)
  {
    OValueFloat * v = new OValueFloat(this, avalue);
    return new OValSymConst(apos, aname, this, v);  // takes over the ownership of v
  }

  LlType * CreateLlType() override
  {
    if (bytesize > 4)
    {
      return LlType::getDoubleTy(ll_ctx);
    }
    else
    {
      return LlType::getFloatTy(ll_ctx);
    }
  }

  LlDiType * CreateDiType() override
  {
    return di_builder->createBasicType("float", 64, llvm::dwarf::DW_ATE_float);
  }

  LlValue *  GenerateConversion(OScope * scope, OExpr * src)  override;
};


