/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_base.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#pragma once

#include "stdint.h"
#include <string>

#include "comp_config.h"
#include "comp_options.h"
#include "symbols.h"
#include "otype_func.h"
#include "scf_dq.h"
#include "errorcodes.h"

using namespace std;

class ODqCompBase
{
public:
  OScFeederDq *    scf = nullptr;

  OScPosition       scpos_statement_start;
  OScPosition *     errorpos = nullptr;  // if nullptr then uses the scpos_statement_start
  OValSymFunc *     curvsfunc = nullptr;

  int              errorcnt = 0;
  int              warncnt = 0;
  int              hintcnt = 0;

public:
  ODqCompBase();
  virtual ~ODqCompBase();

  bool ReservedWord(const string aname);

  string FormatDiagMsg(string_view atext, string_view par1, string_view par2, string_view par3);

  //void Error(const string amsg, OScPosition * ascpos = nullptr);

  // new error definition until the old-style is eliminated

  // With internal error text
  void Error(const TDiagDefErr & adiag, string_view par1, string_view par2, string_view par3, OScPosition * ascpos = nullptr);  // with three str params
  void Error(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * ascpos = nullptr);  // with two str params
  void Error(const TDiagDefErr & adiag, string_view par1, OScPosition * ascpos = nullptr);  // with one str param
  void Error(const TDiagDefErr & adiag, OScPosition * ascpos = nullptr);  // with no str params

  // with custom error text:
  void ErrorTxt(const TDiagDefErr & adiag, string_view atext, string_view par1, string_view par2, string_view par3, OScPosition * ascpos = nullptr);
  void ErrorTxt(const TDiagDefErr & adiag, string_view atext, string_view par1, string_view par2, OScPosition * ascpos = nullptr);
  void ErrorTxt(const TDiagDefErr & adiag, string_view atext, string_view par1, OScPosition * ascpos = nullptr);
  void ErrorTxt(const TDiagDefErr & adiag, string_view atext, OScPosition * ascpos = nullptr);

  // same as Error2() + calls SkipCurStatement()
  void StatementError2(const TDiagDefErr & adiag, string_view par1, string_view par2, string_view par3, OScPosition * scpos = nullptr, bool atryrecover = true);
  void StatementError2(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * scpos = nullptr, bool atryrecover = true);
  void StatementError2(const TDiagDefErr & adiag, string_view par1, OScPosition * scpos = nullptr, bool atryrecover = true);
  void StatementError2(const TDiagDefErr & adiag, OScPosition * scpos = nullptr, bool atryrecover = true);

  // Error recovery utilities
  void SkipToStatementEnd();
  void SkipCurStatement();
  void SkipToSymbol(const char * asym);

  // Warnings

  void Warning(const TDiagDefWarn & adiag, string_view par1, string_view par2, string_view par3, OScPosition * ascpos = nullptr);
  void Warning(const TDiagDefWarn & adiag, string_view par1, string_view par2, OScPosition * ascpos = nullptr);
  void Warning(const TDiagDefWarn & adiag, string_view par1, OScPosition * ascpos = nullptr);
  void Warning(const TDiagDefWarn & adiag, OScPosition * ascpos = nullptr);

  // Hints

  void Hint(const TDiagDefHint & adiag, string_view par1, string_view par2, string_view par3, OScPosition * ascpos = nullptr);
  void Hint(const TDiagDefHint & adiag, string_view par1, string_view par2, OScPosition * ascpos = nullptr);
  void Hint(const TDiagDefHint & adiag, string_view par1, OScPosition * ascpos = nullptr);
  void Hint(const TDiagDefHint & adiag, OScPosition * ascpos = nullptr);

};