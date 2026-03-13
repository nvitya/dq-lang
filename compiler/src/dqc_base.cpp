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

string ODqCompBase::FormatDiagMsg(string_view atext, string_view par1, string_view par2)
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

void ODqCompBase::Error2(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * ascpos)
{
  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos) epos = &scpos_statement_start;

  print("{} ERROR({}): {}\n", epos->Format(), adiag.strid, FormatDiagMsg(adiag.text, par1, par2));

  ++errorcnt;
}

void ODqCompBase::Error2(const TDiagDefErr & adiag, OScPosition * ascpos)
{
  Error2(adiag, "", "", ascpos);
}

void ODqCompBase::Error2(const TDiagDefErr & adiag, string_view par1, OScPosition * ascpos)
{
  Error2(adiag, par1, "", ascpos);
}


void ODqCompBase::StatementError(const string amsg, OScPosition * scpos, bool atryrecover)
{
  OScPosition log_scpos(scf->curfile, scf->curp);

  if (scpos and scpos->scfile) // use the position provided
  {
    log_scpos.Assign(*scpos);
  }

  Error(amsg, &log_scpos);
  //print("{}: {}\n", log_scpos.Format(), amsg);

  // try to recover
  if (atryrecover)
  {
    if (!scf->SearchPattern(";", true))  // TODO: improve to handle #{} and strings
    {

    }
  }

  scf->SkipWhite();
}

void ODqCompBase::StatementError2(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * scpos, bool atryrecover)
{
  OScPosition log_scpos(scf->curfile, scf->curp);

  if (scpos and scpos->scfile) // use the position provided
  {
    log_scpos.Assign(*scpos);
  }

  Error2(adiag, par1, par2, &log_scpos);

  // try to recover
  if (atryrecover)
  {
    if (!scf->SearchPattern(";", true))  // TODO: improve to handle #{} and strings
    {

    }
  }

  scf->SkipWhite();
}

void ODqCompBase::StatementError2(const TDiagDefErr & adiag, OScPosition * scpos, bool atryrecover)
{
  StatementError2(adiag, "", "", scpos, atryrecover);
}

void ODqCompBase::StatementError2(const TDiagDefErr & adiag, string_view par1, OScPosition * scpos, bool atryrecover)
{
  StatementError2(adiag, par1, "", scpos, atryrecover);
}


void ODqCompBase::ExpressionError(const string amsg, OScPosition * scpos)
{
  OScPosition log_scpos(scpos_statement_start);

  if (scpos and scpos->scfile) // use the position provided
  {
    log_scpos.Assign(*scpos);
  }

  Error(amsg, &log_scpos);
}

void ODqCompBase::ExpressionError2(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * scpos)
{
  OScPosition log_scpos(scpos_statement_start);

  if (scpos and scpos->scfile) // use the position provided
  {
    log_scpos.Assign(*scpos);
  }

  Error2(adiag, par1, par2, &log_scpos);
}

void ODqCompBase::ExpressionError2(const TDiagDefErr & adiag, string_view par1, OScPosition * scpos)
{
  ExpressionError2(adiag, par1, "", scpos);
}

void ODqCompBase::ExpressionError2(const TDiagDefErr & adiag, OScPosition * scpos)
{
  ExpressionError2(adiag, "", "", scpos);
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
