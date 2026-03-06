/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    statements.h
 * authors: nvitya
 * created: 2026-02-07
 * brief:   statements, statement blocks
 */

#pragma once

#include <format>
#include <vector>
#include "symbols.h"
#include "ll_defs.h"
#include "expressions.h"

using namespace std;

class OStmt
{
public:
  OScPosition   scpos;

  OStmt(OScPosition & ascpos)
  :
    scpos(ascpos)
  {
  }

  virtual ~OStmt()  { }

  virtual void Generate(OScope * scope)
  {
    throw logic_error(std::format("Unhandled OStmt::Generate for \"{}\"", typeid(this).name()));
  }

  void EmitDebugLocation(OScope * scope, OScPosition * ascpos = nullptr);
};

class OValSymFunc;

struct OStmtReturn : public OStmt
{
private:
  using        super = OStmt;
public:
  OExpr *       value;
  OValSymFunc * vsfunc;

  OStmtReturn(OScPosition & ascpos, OExpr * v, OValSymFunc * avsfunc)
  :
    super(ascpos),
    value(v),
    vsfunc(avsfunc)
  {}

  void Generate(OScope * scope) override;
};

class OStmtBlock
{
private:
  using        super = OStmt;
public:
  OScope *         scope; // owned
  vector<OStmt *>  stlist;

  OStmtBlock(OScope * aparentscope, const string adebugname)
  {
    scope = new OScope(aparentscope, adebugname);
  }

  virtual ~OStmtBlock()
  {
    for (OStmt * st : stlist)
    {
      delete st;
    }
    stlist.clear();

    delete scope;
  }

  OStmt * AddStatement(OStmt * astmt)
  {
    stlist.push_back(astmt);
    return astmt;
  }

  void Generate();
};

class OStmtVarDecl : public OStmt
{
private:
  using        super = OStmt;
public:
  OValSym *  variable;
  OExpr *    initvalue;

  OStmtVarDecl(OScPosition & ascpos, OValSym * avariable, OExpr * ainitvalue)
  :
    super(ascpos),
    variable(avariable),
    initvalue(ainitvalue)
  {
    if (ainitvalue)
    {
      variable->initialized = true;
    }
  }

  void Generate(OScope * scope) override;
};

class OStmtAssign : public OStmt
{
private:
  using        super = OStmt;
public:
  OValSym *   variable;
  OExpr *     value;
  OStmtAssign(OScPosition & ascpos, OValSym * avariable, OExpr * avalue)
  :
    super(ascpos),
    variable(avariable),
    value(avalue)
  {}

  void Generate(OScope * scope) override;
};

class OStmtModifyAssign : public OStmt
{
private:
  using        super = OStmt;
public:
  OValSym *   variable;
  EBinOp      op;
  OExpr *     value;
  OStmtModifyAssign(OScPosition & ascpos, OValSym * avariable, EBinOp aop, OExpr * avalue)
  :
    super(ascpos),
    variable(avariable),
    op(aop),
    value(avalue)
  {}

  void Generate(OScope * scope) override;
};

class OStmtDerefAssign : public OStmt
{
private:
  using        super = OStmt;
public:
  OValSym *   ptrvariable;
  OExpr *     value;
  OStmtDerefAssign(OScPosition & ascpos, OValSym * aptrvariable, OExpr * avalue)
  :
    super(ascpos),
    ptrvariable(aptrvariable),
    value(avalue)
  {}

  void Generate(OScope * scope) override;
};

class OStmtVoidCall : public OStmt
{
private:
  using        super = OStmt;
public:
  OExpr *      callexpr;
  OStmtVoidCall(OScPosition & ascpos, OExpr * acallexpr)
  :
    super(ascpos),
    callexpr(acallexpr)
  {}

  void Generate(OScope * scope) override;
};

class OStmtWhile : public OStmt
{
private:
  using        super = OStmt;
public:
  OExpr *       condition;
  OStmtBlock *  body;
  OStmtWhile(OScPosition & ascpos, OExpr * acondition, OScope * ascope)
  :
    super(ascpos),
    condition(acondition)
  {
    body = new OStmtBlock(ascope, "while");
  }

  ~OStmtWhile()
  {
    delete body;
  }

  void Generate(OScope * scope) override;
};

class OIfBranch
{
public:
  OExpr *       condition; // nullptr for else branch
  OStmtBlock *  body;
  OIfBranch(OExpr * acondition, OScope * aparentscope)
  :
    condition(acondition)
  {
    body = new OStmtBlock(aparentscope, "ifbranch");
  }

  ~OIfBranch()
  {
    delete body;
  }
};

class OStmtIf : public OStmt
{
private:
  using        super = OStmt;
public:
  OScope *             parentscope;
  bool                 else_present = false;
  vector<OIfBranch *>  branches; // if, elif..., else
  OStmtIf(OScPosition & ascpos, OScope * aparentscope)
  :
    super(ascpos),
    parentscope(aparentscope)
  {
  }

  ~OStmtIf()
  {
    for (OIfBranch * b : branches)
    {
      delete b;
    }
  }

  OIfBranch * AddBranch(OExpr * acondition)
  {
    OIfBranch * result = new OIfBranch(acondition, parentscope);
    branches.push_back(result);
    return result;
  }

  void Generate(OScope * scope) override;
};

class OBreakStmt : public OStmt
{
public:
  void Generate(OScope * scope) override;
};

class OContinueStmt : public OStmt
{
public:
  void Generate(OScope * scope) override;
};
