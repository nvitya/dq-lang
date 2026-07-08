/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_string.h
 * authors: nvitya
 * created: 2026-06-09
 * brief:   Byte-only str and strview types
 */

#pragma once

#include "symbols.h"

class OExpr;
class OLValueExpr;

class OTypeDynString : public OType
{
private:
  using super = OType;

public:
  OTypeDynString()
  :
    super("str", TK_DYNSTR)
  {
    bytesize = TARGET_PTRSIZE;
    alignsize = TARGET_PTRSIZE;
  }

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
};

class OTypeStrView : public OType
{
private:
  using super = OType;

public:
  OTypeStrView()
  :
    super("strview", TK_STRVIEW)
  {
    bytesize = TARGET_PTRSIZE + 8;
    alignsize = TARGET_PTRSIZE;
  }

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
};

enum EStringMetaField
{
  SMF_LENGTH,
  SMF_CAPACITY,
  SMF_REFCOUNT
};

enum EStringMethod
{
  STRM_CLEAR,
  STRM_SET,
  STRM_RESERVE,
  STRM_COMPACT,
  STRM_SET_LENGTH,
  STRM_SET_CAPACITY,
  STRM_TRUNCATE,
  STRM_APPEND,
  STRM_PREPEND,
  STRM_INSERT,
  STRM_DELETE,
  STRM_CLONE,
  STRM_POP,
  STRM_POP_FIRST,
  STRM_POP_CHAR,
  STRM_POP_FIRST_CHAR,
  STRM_ADDFMT
};

bool IsTextSourceType(OType * type);
bool IsStringComparableTextType(OType * type);
bool IsStringFamilyTextType(OType * type);
bool EnsureDynStringRtlUseForStringTypes();
LlValue * GenerateTextInfoAddress(OScope * scope, OExpr * expr);
LlValue * GenerateTextInfoValue(OScope * scope, OExpr * expr);
LlValue * GenerateStringLength(OScope * scope, OType * strtype, LlValue * straddr);
LlValue * GenerateStringCapacity(OScope * scope, OType * strtype, LlValue * straddr);
LlValue * GenerateStringRefCount(OScope * scope, OType * strtype, LlValue * straddr);
LlValue * GenerateStringGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index);
LlValue * GenerateStringCharAddress(OScope * scope, OLValueExpr * receiver, LlValue * index);
void GenerateStringSetChar(OScope * scope, OLValueExpr * receiver, OExpr * index, OExpr * value);
LlValue * GenerateStringSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                              OExpr * end_expr, bool end_inclusive);
LlValue * GenerateStringEqual(OScope * scope, OExpr * left, OExpr * right);
LlValue * GenerateStringConcat(OScope * scope, OExpr * left, OExpr * right);
LlValue * GenerateStringConcatFromStringValue(OScope * scope, LlValue * leftvalue, OExpr * right);
void GenerateStringCreate(OScope * scope, LlValue * straddr);
void GenerateStringIncRef(OScope * scope, LlValue * straddr);
void GenerateStringDestroy(OScope * scope, LlValue * straddr);
bool GenerateStringAssignExpr(OScope * scope, LlValue * targetaddr, OExpr * value);
LlValue * GenerateStringMethodCall(OScope * scope, OLValueExpr * receiver, EStringMethod method,
                                   const vector<OExpr *> & args);
