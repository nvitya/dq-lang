/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_int.h
 * authors: nvitya
 * created: 2026-02-02
 * brief:
 */

#pragma once

#include "symbols.h"

class OValueInt : public OValue
{
private:
  using        super = OValue;

public:
  int64_t      value;

  OValueInt(OType * atype, int64_t avalue)
  :
    super(atype)
  {
    value = avalue;
  }

  LlConst *  CreateLlConst() override;
  bool       CalculateConstant(OExpr * expr) override;
};

class OTypeInt : public OType
{
private:
  using        super = OType;

public:
  uint8_t      bitlength;
  bool         issigned;

  OTypeInt(const string name, uint8_t abitlength, bool asigned)
  :
    bitlength(abitlength),
    issigned(asigned),
    super(name, TK_INT)
  {
    bytesize = ((abitlength + 7) >> 3);
  }

  OValue * CreateValue() override
  {
    return new OValueInt(this, 0);
  }

  LlType * CreateLlType() override
  {
    return LlType::getIntNTy(ll_ctx, bitlength);
  }

  LlDiType * CreateDiType() override
  {
    return di_builder->createBasicType("int", 64, llvm::dwarf::DW_ATE_signed);
  }

  OValSymConst * CreateConst(OScPosition & apos, const string aname, const int64_t avalue)
  {
    OValueInt * v = new OValueInt(this, avalue);
    return new OValSymConst(apos, aname, this, v);  // takes over the ownership of v
  }
};

// later OTypeUint will be created for unsigned

