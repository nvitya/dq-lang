/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_base.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include <print>
#include <format>
#include "dqc_base.h"

using namespace std;

ODqCompBase::ODqCompBase()
{
  scf = new OScFeederDq();
}

ODqCompBase::~ODqCompBase()
{
  delete scf;
}

const string dq_reserved_words =
   "|var|for|while|if|else|return|break|continue"
   "|and|not|or"
   "|NOT|AND|OR|XOR|IDIV|IMOD"
   "|null"
   "|function|use|implementation|initialization|finalization"
   "|struct|endstruct"
   "|"
;

bool ODqCompBase::ReservedWord(const string aname)
{
  string search_target = "|" + aname + "|";
  if (dq_reserved_words.find(search_target) != string::npos)
  {
    return true;
  }
  else
  {
    return false;
  }
}

string ODqCompBase::FormatDiagMsg(string_view atext, string_view par1, string_view par2, string_view par3)
{
  string msg(atext);
  size_t pos;

  if (!par1.empty())
  {
    pos = 0;
    while ((pos = msg.find("$1", pos)) != string::npos)
    {
      msg.replace(pos, 2, par1);
      pos += par1.size();
    }
  }

  if (!par2.empty())
  {
    pos = 0;
    while ((pos = msg.find("$2", pos)) != string::npos)
    {
      msg.replace(pos, 2, par2);
      pos += par2.size();
    }
  }

  if (!par3.empty())
  {
    pos = 0;
    while ((pos = msg.find("$3", pos)) != string::npos)
    {
      msg.replace(pos, 2, par3);
      pos += par3.size();
    }
  }

  return msg;
}

void ODqCompBase::Error(const string amsg, OScPosition * ascpos)
{
  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos) epos = &scpos_statement_start;

  print("{} ERROR: {}\n", epos->Format(), amsg);

  ++errorcnt;
}

void ODqCompBase::ErrorTxt(const TDiagDefErr & adiag, string_view atext, string_view par1, string_view par2, string_view par3, OScPosition *ascpos)
{
 OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos) epos = &scpos_statement_start;

  print("{} ERROR({}): {}\n", epos->Format(), adiag.strid, FormatDiagMsg(atext, par1, par2, par3));

  ++errorcnt;
}

void ODqCompBase::ErrorTxt(const TDiagDefErr & adiag, string_view atext, string_view par1, string_view par2, OScPosition * ascpos)
{
  ErrorTxt(adiag, atext, par1, par2, "", ascpos);
}

void ODqCompBase::ErrorTxt(const TDiagDefErr & adiag, string_view atext, string_view par1, OScPosition * ascpos)
{
  ErrorTxt(adiag, atext, par1, "", "", ascpos);
}

void ODqCompBase::ErrorTxt(const TDiagDefErr & adiag, string_view atext, OScPosition * ascpos)
{
  ErrorTxt(adiag, atext, "", "", "", ascpos);
}

void ODqCompBase::Error2(const TDiagDefErr & adiag, string_view par1, string_view par2, string_view par3, OScPosition * ascpos)
{
  OScPosition log_scpos;

  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos)
  {
    // use the current parser position by default
    scf->SaveCurPos(log_scpos);
    epos = &log_scpos;

    // old behaviour: statement_start
    //epos = &scpos_statement_start;
  }

  print("{} ERROR({}): {}\n", epos->Format(), adiag.strid, FormatDiagMsg(adiag.text, par1, par2, par3));

  ++errorcnt;
}

void ODqCompBase::Error2(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * ascpos)
{
  Error2(adiag, par1, par2, "", ascpos);
}

void ODqCompBase::Error2(const TDiagDefErr & adiag, string_view par1, OScPosition * ascpos)
{
  Error2(adiag, par1, "", "", ascpos);
}

void ODqCompBase::Error2(const TDiagDefErr & adiag, OScPosition * ascpos)
{
  Error2(adiag, "", "", "", ascpos);
}


void ODqCompBase::SkipToStatementEnd()
{
  if (!scf->SearchPattern(";", true))  // TODO: improve to handle #{} and strings
  {
  }
}

void ODqCompBase::SkipCurStatement()
{
  // usually called for error recovery, to find the next statement
  if (!scf->SearchPattern(";", true))  // TODO: improve to handle #{} and strings
  {
    return;
  }

  scf->CheckSymbol(";"); // consume the ";"
}

void ODqCompBase::SkipToSymbol(const char * asym)
{
  // for error recovery only

  if (scf->SearchPattern(asym))
  {
    scf->CheckSymbol(asym); // consume the symbol
  }
}

void ODqCompBase::StatementError2(const TDiagDefErr & adiag, string_view par1, string_view par2, string_view par3, OScPosition * scpos, bool atryrecover)
{
  Error2(adiag, par1, par2, par3, scpos);
  SkipCurStatement();
}

void ODqCompBase::StatementError2(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * scpos, bool atryrecover)
{
  StatementError2(adiag, par1, par2, "", scpos, atryrecover);
}

void ODqCompBase::StatementError2(const TDiagDefErr & adiag, string_view par1, OScPosition * scpos, bool atryrecover)
{
  StatementError2(adiag, par1, "", "", scpos, atryrecover);
}

void ODqCompBase::StatementError2(const TDiagDefErr & adiag, OScPosition * scpos, bool atryrecover)
{
  StatementError2(adiag, "", "", "", scpos, atryrecover);
}

void ODqCompBase::Warning(const string amsg, OScPosition * ascpos)
{
  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos) epos = &scpos_statement_start;

  print("{} WARNING: {}\n", epos->Format(), amsg);

  ++warncnt;
}

void ODqCompBase::Hint(const string amsg, OScPosition * ascpos)
{
  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos) epos = &scpos_statement_start;

  print("{} HINT: {}\n", epos->Format(), amsg);

  ++hintcnt;
}
