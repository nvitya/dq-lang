/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_parser_stmt.h
 * authors: nvitya
 * created: 2026-06-13
 * brief:   Statement parsing layer
 */

#pragma once

#include "dqc_parser_expr.h"

class ODqCompParserStmt : public ODqCompParserExpr
{
public: // statement blocks
  void ReadStatementBlock(OStmtBlock * stblock, const string blockend, string * rendstr = nullptr);
  void ParseStmtReturn();
  void ParseStmtWhile();
  void ParseStmtFor();
  void ParseStmtIf();
  void ParseStmtTry();
  void ParseStmtRaise();
  void ParseStmtDelete();
  void ParseStmtInherited();
  void FinalizeStmtVoidCall(OExpr * callexpr);

  void ParseStmtVar(bool arootstmt);  // used for statement blocks too
  void ParseStmtConst(bool arootstmt);  // used for statement blocks too
  void ParseStmtRef();

  EBinOp ParseAssignOp();

protected:
  int except_depth = 0;
};
