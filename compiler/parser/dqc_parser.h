/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_parser.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#pragma once

#include "stdint.h"
#include <string>
#include "comp_options.h"
#include "expressions.h"
#include "statements.h"
#include "dqc_ast.h"

using namespace std;

class ODqCompParser : public ODqCompAst
{
private:
  using             super = ODqCompAst;

public:
  OScPosition       scpos_statement_start;
  OScPosition *     errorpos = nullptr;  // if nullptr then uses the scpos_statement_start
  OValSymFunc *     curvsfunc = nullptr;

public:
  ODqCompParser();
  virtual ~ODqCompParser();

  void ParseModule();

public: // top level items
  void ParseVarDecl();
  void ParseConstDecl();
  void ParseFunction();

public: // statement blocks
  void ReadStatementBlock(OStmtBlock * stblock, const string blockend, string * rendstr = nullptr);

  void ParseStmtVar();
  bool ParseStmtAssign(OValSym * pvalsym);
  void ParseStmtReturn();
  void ParseStmtWhile();
  void ParseStmtIf();
  void ParseStmtVoidCall(OValSymFunc * vsfunc);
  void ParseStmtDerefAssign(OValSym * ptrvalsym);
  void ParseStmtArrayAssign(OValSym * arrayvalsym);

public: // type parsing
  OType * ParseTypeSpec();  // parses type after ":" — handles ^, [N], []

public: // utility
  bool CheckStatementClose();

  void StatementError(const string amsg, OScPosition * scpos = nullptr, bool atryrecover = true);
  void ExpressionError(const string amsg, OScPosition * scpos = nullptr);

  void Error(const string amsg, OScPosition * ascpos = nullptr);
  void Warning(const string amsg, OScPosition * ascpos = nullptr);
  void Hint(const string amsg, OScPosition * ascpos = nullptr);

public: // expressions
  OStmtBlock *  curblock = nullptr;
  OScope *      curscope = nullptr;

  OExpr * ParseExpression(); // calls ParseExprOr()

  // if a == 1 or 1 SHL 5 AND 0xFFFF != 0
  // if a == 1 or regval >> 5 AND 0xF != 0

  //   5 + -x << 2

  // Expression parsing in increasing operator priority:
  OExpr * ParseExprOr();
  OExpr * ParseExprAnd();
  OExpr * ParseExprNot();
  OExpr * ParseComparison();
  OExpr * ParseExprAdd();
  OExpr * ParseExprMul();
  OExpr * ParseExprDiv();
  OExpr * ParseExprBinOr();
  OExpr * ParseExprBinAnd();
  OExpr * ParseExprShift();
  OExpr * ParseExprNeg();
  OExpr * ParseExprPrimary();

  OExpr * ParseExprFuncCall(OValSymFunc * vsfunc);
  OExpr * ParseBuiltinLen();

protected:
  OExpr * CreateBinExpr(EBinOp op, OExpr * left, OExpr * right);  // handles implicit conversions
  bool    CheckAssignType(OType * dsttype, OExpr ** rexpr,
                          const string astmt);                    // returns false when the assignment is not possible
                                                                  // adds implicit conversion if necessary
};