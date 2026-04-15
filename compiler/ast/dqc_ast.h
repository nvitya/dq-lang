/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_ast.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#pragma once

#include "stdint.h"
#include <string>
#include <vector>
#include "comp_options.h"
#include "expressions.h"
#include "symbols.h"
#include "dq_module.h"

#include "dqc_base.h"

using namespace std;

enum EExprConvFlags
{
  EXPCF_GENERATE_ERRORS    = 1,
  EXPCF_ALLOW_LAZY_CSTRING = 2,
  EXPCF_EXPLICIT_CAST      = 4
};

class ODqCompAst : public ODqCompBase
{
private:
  using            super = ODqCompBase;

public:
  bool             section_public = true;
  OScope *         cur_mod_scope = nullptr;

public:
  ODqCompAst();
  virtual ~ODqCompAst();

  ODecl * AddDeclVar(OScPosition & scpos, string aid, OType * atype);
  ODecl * AddDeclConst(OScPosition & scpos, string aid, OType * atype, OValue * avalue);
  ODecl * AddDeclFunc(OScPosition & scpos, OValSymFunc * avsfunc);
  bool    ResolveCompoundMemberBase(OLValueExpr * lval, OType * srctype, OLValueExpr *& memberbase, OCompoundType *& ctype);
  void    CollectIgnoredPlainAssignVars(OLValueExpr * leftexpr, vector<OLValueVar *> & ignored);
  OValSym * GetAssignRootValSym(OLValueExpr * leftexpr);
  OExpr * FreeLeftRight(OExpr * left, OExpr * right);
  OExpr * CreateBinExpr(EBinOp op, OExpr * left, OExpr * right);
  bool    ConvertExprToType(OType * dsttype, OExpr * src, OExpr ** rout, uint32_t aflags = 0);
  bool    ResolveIifType(OExpr ** rtrueexpr, OExpr ** rfalseexpr, OType ** rresulttype);
  bool    CheckAssignType(OType * dsttype, OExpr ** rexpr, const string astmt);

};
