/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    scf_dq.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#pragma once

#include <string>
#include <vector>
#include "stdint.h"
#include "scf_base.h"
#include "errorcodes.h"

using namespace std;

enum EScfCondState
{
  FCOND_IF    = 0,
  FCOND_ELIF  = 1,
  FCOND_ELSE  = 2
};

class OScfCondition
{
public:
  EScfCondState        state;
  bool                 parent_inactive = false;
  bool                 branch_taken = false;

  OScfCondition *      parent;
  OScPosition          startpos;
  OScPosition          elsepos;

  OScfCondition(OScfCondition * aparent, OScPosition astartpos, bool aparent_inactive)
  :
    parent(aparent),
    startpos(astartpos),
    parent_inactive(aparent_inactive),
    state(FCOND_IF)
  {
  }
};

class OScFeederDq : public OScFeederBase
{
private:
  using                super = OScFeederBase;

public:
  string               basedir = ".";

  vector<OScFile *>    scfiles;
  vector<OScPosition>  returnpos;

  OScPosition          scpos_start_directive;

  OScFeederDq();
  virtual ~OScFeederDq();

  int Init(const string afilename) override;
  void Reset() override;

  OScFile * LoadFile(const string afilename);

public: // parsing functions
  inline bool Eof() { return ((curp >= bufend) and (returnpos.size() == 0)); }
  inline void SetDirectiveExprMode(bool avalue) { directive_expr_mode = avalue; }

  void SkipWhite(); // jumps to the first normal token

protected:

  OScfCondition *      curcond = nullptr;
  bool                 inactive_code = false;
  bool                 preproc_closer_brace = true;
  bool                 directive_expr_mode = false;

  void ParseDirective();

  void PreprocError2(const TDiagDefErr & adiag, string_view par1, string_view par2, string_view par3, OScPosition * ascpos = nullptr, bool atryrecover = true);
  void PreprocError2(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * ascpos = nullptr, bool atryrecover = true);
  void PreprocError2(const TDiagDefErr & adiag, string_view par1, OScPosition * ascpos = nullptr, bool atryrecover = true);
  void PreprocError2(const TDiagDefErr & adiag, OScPosition * ascpos = nullptr, bool atryrecover = true);

  void ParseDirectiveInclude();
  void ParseDirectiveDefine();
  void ParseDirectiveLinkLib();
  void ParseDirectiveOpt();
  bool FindDirectiveEnd();

  void StartConditionalBranch();
  void ApplyConditionalResult(bool acondition_true);
  bool ContinueConditionalBranch(const string & adirective);

  bool CheckConditionals(const string aid);  // processes if, ifdef, else, elif, elifdef, endif. Returns true, when one of those found

  void SkipInactiveCode();
};
