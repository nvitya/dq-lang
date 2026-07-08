/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 */

#pragma once

#include <string>
#include <vector>

#include "symbols.h"
#include "otype_int.h"

class OLValueExpr;

struct SEnumItem
{
  std::string name;
  uint64_t value;
};

class OValueEnum : public OValue
{
public:
  uint64_t value = 0;

  OValueEnum(OType * atype, uint64_t avalue = 0) : OValue(atype), value(avalue) {}

  LlConst * CreateLlConst() override;
  bool CalculateConstant(OExpr * expr, bool emit_errors = true) override;
  bool WriteDqmIfValue(ODqmIfWriter & writer) override;
};

class OTypeEnum : public OType
{
public:
  OTypeInt * storage_type;
  std::vector<SEnumItem> items;

  OTypeEnum(const std::string & aname, OTypeInt * astorage_type)
  :
    OType(aname, TK_ENUM),
    storage_type(astorage_type)
  {
    bytesize = storage_type->bytesize;
    alignsize = storage_type->alignsize;
  }

  const SEnumItem * FindItem(const std::string & aname) const;
  bool HasValue(uint64_t avalue) const;

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  OValue * CreateValue() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
  bool WriteDqmIfDecl(ODqmIfWriter & writer) override;
};

class OEnumValueExpr : public OExpr
{
public:
  uint64_t value;

  OEnumValueExpr(OTypeEnum * atype, uint64_t avalue);
  LlValue * Generate(OScope * scope) override;
};

class OUnresolvedEnumItemExpr : public OExpr
{
public:
  std::string item_name;

  explicit OUnresolvedEnumItemExpr(const std::string & aname);
};

class OEnumOrdExpr : public OExpr
{
public:
  OExpr * source;

  explicit OEnumOrdExpr(OExpr * asource);
  LlValue * Generate(OScope * scope) override;
  void FoldChildren() override;
  void DeleteChildTree() override;
};

enum EEnumFromOrdKind
{
  EFOK_THROW,
  EFOK_DEFAULT,
  EFOK_TRY
};

class OEnumFromOrdExpr : public OExpr
{
public:
  EEnumFromOrdKind kind;
  OTypeEnum * enum_type;
  OExpr * value_expr;
  OExpr * default_expr = nullptr;
  OLValueExpr * output_expr = nullptr;

  OEnumFromOrdExpr(EEnumFromOrdKind akind, OTypeEnum * atype, OExpr * avalue);
  LlValue * Generate(OScope * scope) override;
  void FoldChildren() override;
  void DeleteChildTree() override;
};

bool EnsureEnumRtlUse();
