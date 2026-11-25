/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_constexpr.h
 * authors: nvitya
 * created: 2026-11-25
 * brief:   Constexpr AST-template symbol and compile-time/module-interface support
 */

#pragma once

#include "otype_func.h"

class OValSymConstExpr : public OValSym
{
private:
  using super = OValSym;

public:
  vector<OValSym *>  param_syms;
  OExpr *            body_expr = nullptr;

  OValSymConstExpr(OScPosition & apos, const string & aname, OTypeFunc * asigtype);
  ~OValSymConstExpr() override;

  inline OTypeFunc * GetTypeFunc() const { return static_cast<OTypeFunc *>(ptype); }
  void BuildParamSyms();
  int FindParamIndex(const OValSym * avsym) const;
  OExpr * Expand(vector<OExpr *> & args, const OScPosition & callpos) const;
  bool WriteDqmIfDecl(ODqmIfWriter & writer) override;
};
