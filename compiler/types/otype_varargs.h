// SPDX-License-Identifier: MIT
#pragma once

#include "expressions.h"

class OTypeVarArgs : public OType
{
public:
  OTypeVarArgs();
  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  int AnalyzeConversion(OExpr ** rexpr, uint32_t flags, bool convert) override;
  static uint8_t Tag(OExpr * expr);
};

class OVarArgsPackExpr : public OExpr
{
  vector<OExpr *> elements;
  vector<OCachedValueExpr *> owners;
  LlValue * Descriptor(const vector<uint8_t> & tags);

public:
  OVarArgsPackExpr(OType * type, vector<OExpr *> && values);
  ~OVarArgsPackExpr() override;
  void PrepareOwners(vector<OExpr *> & cleanups);
  LlValue * Generate(OScope * scope) override;
  void FoldChildren() override;
};

class OVarArgsFieldExpr : public OExpr
{
  OExpr * source;
  unsigned field;

public:
  OVarArgsFieldExpr(OExpr * pack, const string & name);
  ~OVarArgsFieldExpr() override { delete source; }
  LlValue * Generate(OScope * scope) override;
};
