/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_anyvalue.h
 * authors: nvitya
 * created: 2026-06-10
 * brief:   anyvalue builtin type and lowering helpers
 */

#pragma once

#include "symbols.h"

class OLValueExpr;

enum EAnyValueMethod
{
  AVM_IS_NULL,
  AVM_SET_NULL,
  AVM_IS_NUMBER,
  AVM_IS_INT,
  AVM_IS_SINT,
  AVM_IS_UINT,
  AVM_AS_INT,
  AVM_AS_UINT,
  AVM_SET_INT,
  AVM_SET_UINT,
  AVM_IS_BOOL,
  AVM_AS_BOOL,
  AVM_SET_BOOL,
  AVM_IS_POINTER,
  AVM_AS_POINTER,
  AVM_SET_POINTER,
  AVM_IS_FLOAT,
  AVM_IS_FLOAT32,
  AVM_IS_FLOAT64,
  AVM_AS_FLOAT,
  AVM_AS_FLOAT32,
  AVM_AS_FLOAT64,
  AVM_SET_FLOAT,
  AVM_SET_FLOAT32,
  AVM_SET_FLOAT64,
  AVM_IS_TEXT,
  AVM_AS_TEXT,
  AVM_SET_TEXT,
  AVM_SET_CSTRING,
  AVM_IS_STR,
  AVM_AS_STR,
  AVM_SET_STR
};

class OTypeAnyValue : public OType
{
private:
  using super = OType;

public:
  OTypeAnyValue()
  :
    super("anyvalue", TK_ANYVALUE)
  {
    bytesize = 16 + 3 + TARGET_PTRSIZE - 1;
    alignsize = 1;
  }

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
};

bool EnsureAnyValueRtlUse();
bool IsAnyValueSourceType(OType * type);

void GenerateAnyValueCreate(OScope * scope, LlValue * addr);
void GenerateAnyValueDestroy(OScope * scope, LlValue * addr);
void GenerateAnyValueCopy(OScope * scope, LlValue * dstaddr, LlValue * srcaddr);
void GenerateAnyValueMove(OScope * scope, LlValue * dstaddr, LlValue * srcaddr);
bool GenerateAnyValueAssignExpr(OScope * scope, LlValue * targetaddr, OExpr * value);
LlValue * GenerateAnyValueBoxExpr(OScope * scope, OType * anytype, OExpr * source);
LlValue * GenerateAnyValueMethodCall(OScope * scope, OLValueExpr * receiver, EAnyValueMethod method,
                                     const vector<OExpr *> & args);
