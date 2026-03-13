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

void ODqCompBase::Error(const string amsg, OScPosition * ascpos)
{
  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos) epos = &scpos_statement_start;

  print("{} ERROR: {}\n", epos->Format(), amsg);

  ++errorcnt;
}

void ODqCompBase::Error2(const TDiagDefErr & adiag, OScPosition * ascpos)
{
  Error(format("{}: {}", adiag.strid, adiag.text), ascpos);
}

void ODqCompBase::Error2(const TDiagDefErr & adiag, string_view par1, OScPosition * ascpos)
{
  string msg = adiag.text;

  size_t pos = 0;
  while ((pos = msg.find("$1", pos)) != string::npos)
  {
    msg.replace(pos, 2, par1);
    pos += par1.size();
  }

  Error(format("{}: {}", adiag.strid, msg), ascpos);
}

void ODqCompBase::Error2(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * ascpos)
{
  string msg = adiag.text;
  size_t pos = 0;

  while ((pos = msg.find("$1", pos)) != string::npos)
  {
    msg.replace(pos, 2, par1);
    pos += par1.size();
  }

  pos = 0;
  while ((pos = msg.find("$2", pos)) != string::npos)
  {
    msg.replace(pos, 2, par2);
    pos += par2.size();
  }

  Error(format("{}: {}", adiag.strid, msg), ascpos);
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

void ODqCompBase::StatementError2(const TDiagDefErr & adiag, OScPosition * scpos, bool atryrecover)
{
  StatementError(format("{}: {}", adiag.strid, adiag.text), scpos, atryrecover);
}

void ODqCompBase::StatementError2(const TDiagDefErr & adiag, string_view par1, OScPosition * scpos, bool atryrecover)
{
  string msg = adiag.text;

  size_t pos = 0;
  while ((pos = msg.find("$1", pos)) != string::npos)
  {
    msg.replace(pos, 2, par1);
    pos += par1.size();
  }

  StatementError(format("{}: {}", adiag.strid, msg), scpos, atryrecover);
}

void ODqCompBase::StatementError2(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * scpos, bool atryrecover)
{
  string msg = adiag.text;

  size_t pos = 0;
  while ((pos = msg.find("$1", pos)) != string::npos)
  {
    msg.replace(pos, 2, par1);
    pos += par1.size();
  }

  pos = 0;
  while ((pos = msg.find("$2", pos)) != string::npos)
  {
    msg.replace(pos, 2, par2);
    pos += par2.size();
  }

  StatementError(format("{}: {}", adiag.strid, msg), scpos, atryrecover);
}

void ODqCompBase::ExpressionError(const string amsg, OScPosition *scpos)
{
  OScPosition log_scpos(scpos_statement_start);

  if (scpos and scpos->scfile) // use the position provided
  {
    log_scpos.Assign(*scpos);
  }

  Error(amsg, &log_scpos);
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
