/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_base.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include <print>
#include <format>
#include "dqc_base.h"
#include "dq_utils.h"

using namespace std;

static void PrintDiagnostic(string_view severity, string_view id, string_view message, OScPosition * position)
{
  if (!g_opt.diagnostic_json)
  {
    print("{} {}({}): {}\n", position->Format(), severity, id, message);
    return;
  }

  string path;
  int64_t offset = 0;
  int line = 1;
  int column = 1;
  if (position)
  {
    line = position->line;
    column = position->col;
    if (position->scfile)
    {
      path = position->scfile->fullpath;
      if (position->scfile->pstart && position->pos)
      {
        offset = position->pos - position->scfile->pstart;
      }
    }
  }
  print("{{\"kind\":\"diagnostic\",\"severity\":\"{}\",\"code\":\"{}\",\"message\":\"{}\",\"path\":\"{}\",\"line\":{},\"column\":{},\"offset\":{}}}\n",
        JsonEscape(severity), JsonEscape(id), JsonEscape(message), JsonEscape(path), line, column, offset);
}

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
   "|endfor|try|except|finally|endtry|raise"
   "|new|delete"
   "|and|not|or|is|as"
   "|xor|div|rem|mod"
   "|nil"
   "|func|use|implementation"
   "|struct|endstruct|union|endunion|object|endobject"
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
   "|func|use|implementation"
   "|struct|union|object"
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

const string dq_block_closer_words =
   "|endfor|endwhile|endif|endfunc|endtry|except|finally|else|elif"
   "|endstruct|endobject|endconst|endenum|endunion|endclass"
   "|"
;

bool ODqCompBase::IsBlockCloserWord(const string aname)
{
  string search_target = "|" + aname + "|";
  if (dq_block_closer_words.find(search_target) != string::npos)
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
  if (suppress_errors) return;

  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos) epos = &scpos_statement_start;

  PrintDiagnostic("ERROR", adiag.strid, FormatDiagMsg(atext, par1, par2, par3), epos);

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
  if (suppress_errors) return;

  OScPosition log_scpos;

  OScPosition * epos = ascpos;
  if (!epos) epos = errorpos;
  if (!epos)
  {
    scf->SaveCurPos(log_scpos);
    // A statement can only be known to be invalid after whitespace look-ahead
    // reaches the next line. Keep that diagnostic on the statement that
    // caused it, while retaining precise positions for errors found within a
    // multiline statement (notably inline assembly bodies).
    epos = (scf->curline > scf->last_token_end_line) ? &scpos_statement_start : &log_scpos;
  }

  PrintDiagnostic("ERROR", adiag.strid, FormatDiagMsg(adiag.text, par1, par2, par3), epos);

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
  SkipCurStatement();
}

void ODqCompBase::SkipCurStatement()
{
  scf->SkipSpaces(false); // skip spaces, but not line feeds
  if (scf->curline > scf->last_token_end_line)
  {
    // The failing parser already looked across the statement-ending newline.
    // Leave the next statement untouched so recovery can resume from it.
    return;
  }

  while (not scf->Eof())
  {
    scf->SkipSpaces(false); // skip spaces, but not line feeds
    if (scf->Eof()) return;

    char c = *scf->curp;
    if (c == '\r' || c == '\n')
    {
      scf->SkipWhite(); // consume the line break and any following whitespace
      return;
    }

    if (scf->CheckSymbol(";"))
    {
      return; // consumed ';'
    }

    if (scf->CheckSymbol("}", false))
    {
      return; // block close reached, don't consume it
    }

    string sid;
    if (scf->ReadIdentifier(sid, false)) // just peek the identifier
    {
      if (IsBlockCloserWord(sid))
      {
        return; // don't consume, it's the start of the next structural block
      }
      scf->ReadIdentifier(sid, true); // consume it
      continue;
    }

    if (scf->ReadQuotedString(sid))
    {
      continue;
    }

    if (scf->CheckSymbol("//"))
    {
      scf->ReadTo("\r\n");
      continue;
    }

    if (scf->CheckSymbol("/*"))
    {
      scf->SearchPattern("*/", true);
      continue;
    }

    if (scf->CheckSymbol("#", false)) // preprocessor
    {
      return; // let SkipWhite handle it in the next statement
    }

    // consume one char
    ++scf->curp;
    scf->RecalcCurCol();
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

void ODqCompBase::SkipToModuleStatementStart(bool astop_at_block_closer)
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
      if (RootStatementWord(sid) || (astop_at_block_closer && IsBlockCloserWord(sid)))
      {
        scf->SetCurPos(scpos);  // restore the statement/closer keyword position
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
  if (suppress_warnings) return;

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

  PrintDiagnostic("WARNING", adiag.strid, FormatDiagMsg(adiag.text, par1, par2, par3), epos);

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

  PrintDiagnostic("HINT", adiag.strid, FormatDiagMsg(adiag.text, par1, par2, par3), epos);

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
