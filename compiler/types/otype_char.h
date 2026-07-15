/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_char.h
 * authors: nvitya
 * created: 2026-06-07
 * brief:   Character type
 */

#pragma once

#include "otype_int.h"

bool IsCharacterType(OType * type);
bool IsValidWCharValue(int64_t value);
bool TryGetDirectWCharLiteralValue(OExpr * expr, int64_t & rvalue);

class OTypeChar : public OTypeInt
{
private:
  using super = OTypeInt;

public:
  OTypeChar()
  :
    super("char", 8, false, TK_CHAR)
  {
  }

  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
};

class OTypeChar16 : public OTypeInt
{
private:
  using super = OTypeInt;

public:
  OTypeChar16()
  :
    super("char16", 16, false, TK_CHAR)
  {
  }

  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
};

class OTypeWchar : public OTypeInt
{
private:
  using super = OTypeInt;

public:
  OTypeWchar()
  :
    super("wchar", 32, false, TK_CHAR)
  {
  }

  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
};
