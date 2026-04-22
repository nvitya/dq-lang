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
   "|var|ref|refin|refout|refnull|for|while|if|else|return|break|continue"
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

const string dq_root_statement_words =
   "|var|const|type"
   "|function|use|implementation|initialization|finalization"
   "|struct|object"
   "|"
;

bool ODqCompBase::RootStatementWord(const string aname)
{
  string search_target = "|" + aname + "|";
  if (dq_root_statement_words.find(search_target) != string::npos)
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

void ODqCompBase::Error(const TDiagDefErr & adiag, string_view par1, string_view par2, string_view par3, OScPosition * ascpos)
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

void ODqCompBase::Error(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * ascpos)
{
  Error(adiag, par1, par2, "", ascpos);
}

void ODqCompBase::Error(const TDiagDefErr & adiag, string_view par1, OScPosition * ascpos)
{
  Error(adiag, par1, "", "", ascpos);
}

void ODqCompBase::Error(const TDiagDefErr & adiag, OScPosition * ascpos)
{
  Error(adiag, "", "", "", ascpos);
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
  if (scf->ReadTo("#;"))  // TODO: improve to handle #{} and strings
  {
    scf->CheckSymbol(";"); // consume the ";"
    return;
  }
}

void ODqCompBase::SkipToSymbol(const char * asym)
{
  // for error recovery only

  if (scf->SearchPattern(asym))
  {
    scf->CheckSymbol(asym); // consume the symbol
  }
}

void ODqCompBase::SkipToModuleStatementStart()
{
  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->Eof())
    {
      return;
    }

    if (scf->CheckSymbol(";"))
    {
      return;
    }

    OScPosition scpos;
    scf->SaveCurPos(scpos);

    string sid;
    if (scf->ReadIdentifier(sid))
    {
      if (RootStatementWord(sid))
      {
        scf->SetCurPos(scpos);  // restore the module keyword starting position
        return;
      }

      continue;
    }

    // if not an identifier
    if (scf->CheckSymbol("[[", false))
    {
      return;
    }

    // some other symbol

    if (scf->ReadQuotedString(sid))  // string ?
    {
      continue;
    }

    // skip this char
    ++scf->curp;
    scf->RecalcCurCol();
  }
}

void ODqCompBase::RootStatementError(const TDiagDefErr & adiag, string_view par1, string_view par2, string_view par3, OScPosition * scpos, bool atryrecover)
{
  Error(adiag, par1, par2, par3, scpos);
  SkipToModuleStatementStart();
}

void ODqCompBase::RootStatementError(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * scpos, bool atryrecover)
{
  RootStatementError(adiag, par1, par2, "", scpos, atryrecover);
}

void ODqCompBase::RootStatementError(const TDiagDefErr & adiag, string_view par1, OScPosition * scpos, bool atryrecover)
{
  RootStatementError(adiag, par1, "", "", scpos, atryrecover);
}

void ODqCompBase::RootStatementError(const TDiagDefErr & adiag, OScPosition * scpos, bool atryrecover)
{
  RootStatementError(adiag, "", "", "", scpos, atryrecover);
}


void ODqCompBase::StatementError(const TDiagDefErr & adiag, string_view par1, string_view par2, string_view par3, OScPosition * scpos, bool atryrecover)
{
  Error(adiag, par1, par2, par3, scpos);
  SkipCurStatement();
}

void ODqCompBase::StatementError(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * scpos, bool atryrecover)
{
  StatementError(adiag, par1, par2, "", scpos, atryrecover);
}

void ODqCompBase::StatementError(const TDiagDefErr & adiag, string_view par1, OScPosition * scpos, bool atryrecover)
{
  StatementError(adiag, par1, "", "", scpos, atryrecover);
}

void ODqCompBase::StatementError(const TDiagDefErr & adiag, OScPosition * scpos, bool atryrecover)
{
  StatementError(adiag, "", "", "", scpos, atryrecover);
}

void ODqCompBase::Warning(const TDiagDefWarn & adiag, string_view par1, string_view par2, string_view par3, OScPosition * ascpos)
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

  print("{} WARNING({}): {}\n", epos->Format(), adiag.strid, FormatDiagMsg(adiag.text, par1, par2, par3));

  ++warncnt;
}

void ODqCompBase::Warning(const TDiagDefWarn & adiag, string_view par1, string_view par2, OScPosition * ascpos)
{
  Warning(adiag, par1, par2, "", ascpos);
}

void ODqCompBase::Warning(const TDiagDefWarn & adiag, string_view par1, OScPosition * ascpos)
{
  Warning(adiag, par1, "", "", ascpos);
}

void ODqCompBase::Warning(const TDiagDefWarn & adiag, OScPosition * ascpos)
{
  Warning(adiag, "", "", "", ascpos);
}

void ODqCompBase::Hint(const TDiagDefHint & adiag, string_view par1, string_view par2, string_view par3, OScPosition * ascpos)
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

  print("{} HINT({}): {}\n", epos->Format(), adiag.strid, FormatDiagMsg(adiag.text, par1, par2, par3));

  ++hintcnt;
}

void ODqCompBase::Hint(const TDiagDefHint & adiag, string_view par1, string_view par2, OScPosition * ascpos)
{
  Hint(adiag, par1, par2, "", ascpos);
}

void ODqCompBase::Hint(const TDiagDefHint & adiag, string_view par1, OScPosition * ascpos)
{
  Hint(adiag, par1, "", "", ascpos);
}

void ODqCompBase::Hint(const TDiagDefHint & adiag, OScPosition * ascpos)
{
  Hint(adiag, "", "", "", ascpos);
}
