/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
  bool       CalculateConstant(OExpr * expr, bool emit_errors = true) override;
  bool       WriteDqmIfValue(ODqmIfWriter & writer) override;
};

class OTypeInt : public OType
{
private:
  using        super = OType;

public:
  uint8_t      bitlength;
  bool         issigned;

  OTypeInt(const string name, uint8_t abitlength, bool asigned, ETypeKind akind = TK_INT)
  :
    bitlength(abitlength),
    issigned(asigned),
    super(name, akind)
  {
    bytesize = ((abitlength + 7) >> 3);
    alignsize = bytesize;
  }

  OValue * CreateValue() override
  {
    return new OValueInt(this, 0);
  }

  int64_t NormalizeConstant(uint64_t rawbits) const;
  int64_t ConvertConstant(int64_t value) const;

  LlType * CreateLlType() override
  {
    return LlType::getIntNTy(ll_ctx, bitlength);  // no signed/unsigned difference here
  }

  LlDiType * CreateDiType() override
  {
    if (issigned)
    {
      return di_builder->createBasicType(name, bitlength, llvm::dwarf::DW_ATE_signed);
    }
    else
    {
      return di_builder->createBasicType(name, bitlength, llvm::dwarf::DW_ATE_unsigned);
    }
  }

  LlValue * GenerateConversion(OScope * scope, OExpr * src) override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;

  OValSymConst * CreateConst(OScPosition & apos, const string aname, const int64_t avalue)
  {
    OValueInt * v = new OValueInt(this, avalue);
    return new OValSymConst(apos, aname, this, v);  // takes over the ownership of v
  }
};

// later OTypeUint will be created for unsigned
