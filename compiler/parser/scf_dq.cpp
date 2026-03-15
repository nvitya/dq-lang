/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    scf_dq.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

// scf_dq.cpp
// principles and some algorithms taken from strparseobj: github....

#include "string.h"
#include "math.h"

#include <fstream>
#include <print>
#include <format>
#include <filesystem>

#include "scf_dq.h"
#include "scope_defines.h"
#include "scope_builtins.h"

#include "dqc.h"

//---------------------------------------------------------

OScFeederDq::OScFeederDq()
{
}

OScFeederDq::~OScFeederDq()
{
  // free the conditional chain
  while (curcond)
  {
    OScfCondition * p = curcond->parent;
    delete curcond;
    curcond = p;
  }

  Reset();
}

int OScFeederDq::Init(const string afilename)
{
  super::Init(afilename);

  filesystem::path  fpath(afilename);
  basedir = fpath.parent_path().string();
  if ("" == basedir)  basedir = ".";

  curfile = LoadFile(fpath.filename().string());
  if (!curfile)
  {
    return 1;
  }

  SetCurPos(curfile, curfile->pstart);

  return 0;
}

void OScFeederDq::Reset()
{
  super::Reset();

  for (OScFile * f : scfiles)
  {
    delete f;
  }

  scfiles.clear();
  returnpos.clear();
}

OScFile * OScFeederDq::LoadFile(const string afilename)
{
  // check if already loaded
  for (OScFile * fs : scfiles)
  {
    if (fs->name == afilename)
    {
      ++fs->usagecount;  // increment usage count for #{include_once}
      return fs;
    }
  }

  string fullname = basedir + "/" + afilename;

  OScFile * f = new OScFile();
  if (!f->Load(afilename, fullname))
  {
    print("Error loading file: \"{}\"!\n", fullname);

    delete f;
    return nullptr;
  }

  if (g_opt.verbose)
  {
    print("File \"{}\" loaded: {} bytes\n", fullname, f->length);
  }

  scfiles.push_back(f);
  f->index = scfiles.size() - 1;
  return f;
}

void OScFeederDq::SkipWhite()
{
  // jumps to the next normal token while:
  //  - processes the #{opt } directives
  //  - processes the #{include } directives
  //  - processes the #{if...} directives, skipping inactive branches

  // this is a pretty complex function

  if (directive_expr_mode)
  {
    SkipSpaces(false);
    return;
  }

repeat_skip:  // jumped here when returning from an include

  while (curp < bufend)
  {
    SkipSpaces();

    if (curp >= bufend)
    {
      break;  // end of current file
    }

    if (CheckSymbol("//")) // single line comment
    {
      ReadTo("\n\r");
    }
    else if (CheckSymbol("/*"))  // multi-line comment start
    {
      // search for the end
      if (SearchPattern("*/", true))
      {
        // closing marker found
      }
      else
      {
        // the closing was not found in this file, jump to the end
        curp = curfile->pend;
        RecalcCurLineCol();
      }
    }
    else if (CheckSymbol("#"))  // compiler directive
    {
      bool saved_ppc_brace = preproc_closer_brace;
      ParseDirective();
      preproc_closer_brace = saved_ppc_brace;
    }
    else // should be a normal token then
    {
      if (not inactive_code)
      {
        return; // active token found
      }
      else
      {
        SkipInactiveCode();  // might return early to let the SkipWhite() to process some parts
        RecalcCurCol();
      }
    }
  } // while

  if (returnpos.size() > 0) // is there any return position recorded ?
  {
    OScPosition rpos = returnpos.back();
    returnpos.pop_back();
    SetCurPos(rpos);

    goto repeat_skip;
  }

  return;
}

void OScFeederDq::SkipInactiveCode()  // for an inactive #{if...} branch, called only from SkipWhite()
{
  // #{if...} and #{endif} must be handled correctly
  // handle strings like var s : str = "My #{ifdef WINDOWS} value";

  while (curp < bufend)
  {
    if (not ReadTo("\"\'#/"))  // skip to: directive start, string start, comment start
    {
      // file/include end reached
      return; // return to the SkipWhite() to handle this
    }

    // / | " | ' | #  was found, not consumed

    if (('"' == *curp) or ('\'' == *curp))  // string start ?
    {
      string s;
      if (not ReadQuotedString(s)) // try to consume the string
      {
        ++curp;  // if string closing was not found skip the string starter
      }

      continue;
    }

    // Call CheckSymbol with consume=false, to not consume the symbols

    if (CheckSymbol("#", false))  // directive
    {
      return;  // return to the SkipWhite() to handle this
    }

    // ony the '/' remained

    if (CheckSymbol("//", false)) // single line comment
    {
      return;  // return to the SkipWhite() to handle this
    }

    if (CheckSymbol("/*", false)) // multi-line comment
    {
      return;  // return to the SkipWhite() to handle this
    }

    ++curp;  // otherwise: skip this char ('/')
  }
}

void OScFeederDq::ParseDirective()
{
  // "#" is already consumed
  // Examples:
  //   #include "filename.dq"
  //   #{opt ... }
  //   #{if ... }

  string sid;

  // the prevpos was already set pointint the beginning of the "# " symbol
  scpos_start_directive.Assign(prevpos);  // save the start of the directive. prevpos saved in the CheckSymbol()

  SkipSpaces(false); // do not use SkipWhite() here !

  if (CheckSymbol("{"))
  {
    preproc_closer_brace = "}";
    SkipSpaces(false);
  }
  else
  {
    preproc_closer_brace = false; // line end then
  }

  // some keyword must come here
  if (not ReadIdentifier(sid))
  {
    PreprocError2(DQERR_CDIR_KW_MISSING);
    return;
  }

  // process the directive...

  if (CheckConditionals(sid))  // ifdef, if, else, etc
  {

  }
  else if (inactive_code)
  {
    // ignore the directives in the inactive code
    if (not FindDirectiveEnd())
    {
      return;
    }
  }
  else if ("define" == sid)
  {
    ParseDirectiveDefine();
  }
  else if ("include" == sid)
  {
    ParseDirectiveInclude(); // already contains end
  }
  else if ("include_once" == sid)
  {
    FindDirectiveEnd();
    if (curfile->usagecount > 1)
    {
      // skip to the end
      curp = bufend;

      if (g_opt.verbose)
      {
        print("{}: ", scpos_start_directive.Format());
        print("include_once found, returning from the include now\n");
      }
    }
  }
  else  // unknown
  {
    PreprocError2(DQERR_CDIR_UNKNOWN, sid);
    return;
  }
}

void OScFeederDq::ParseDirectiveDefine()
{
  // #define DEFNAME
  // note: include already consumed!

  string sid;

  SkipSpaces();

  // identifier must come here
  if (not ReadIdentifier(sid))
  {
    PreprocError2(DQERR_CDIR_DEF_ID_MISSING);
    return;
  }

  // Override the source code position, to point to the #define ... statement start
  g_compiler->errorpos = &scpos_start_directive;
  SkipSpaces(false);
  if (CheckSymbol("="))
  {
    OValSymConst * defineval = g_compiler->ParseDefineConst(scpos_start_directive, sid);
    if (defineval)
    {
      g_defines->DefineValSym(defineval);
    }

    if (not FindDirectiveEnd())
    {
      g_compiler->errorpos = nullptr;  // return to the default error position (statement start)
      return;
    }
  }
  else
  {
    if (not FindDirectiveEnd())
    {
      g_compiler->errorpos = nullptr;  // return to the default error position (statement start)
      return;
    }

    g_defines->DefineValSym(g_builtins->type_bool->CreateConst(scpos_start_directive, sid, true));
  }
  g_compiler->errorpos = nullptr;  // return to the default error position (statement start)
}

void OScFeederDq::ParseDirectiveInclude()
{
  // #include "filename.dq"
  // note: include already consumed!

  string sfname;

  SkipSpaces();

  if (not ReadQuotedString(sfname))
  {
    PreprocError2(DQERR_CDIR_INC_FN_MISSING);
    return;
  }

  if (not FindDirectiveEnd())
  {
    return;
  }

  if (g_opt.verbose)
  {
    print("{}: ", scpos_start_directive.Format());
    print("Including ({})\n", sfname);
  }

  OScFile * incfile = LoadFile(sfname);
  if (!incfile)
  {
    // try with the current path
    filesystem::path  fpath(curfile->name);
    string parentpath = fpath.parent_path(); //.string();
    if (!parentpath.empty())
    {
      parentpath += filesystem::path::preferred_separator;
    }
    incfile = LoadFile(parentpath + sfname);
    if (!incfile)
    {
      PreprocError2(DQERR_CDIR_INC_LOADING, sfname);
      return;
    }
  }

  // push return position

  OScPosition  retpos;
  SaveCurPos(retpos);            // save the current position (pointing after the directive)
  returnpos.push_back(retpos);   // add to the return stack

  // switch to the include
  SetCurPos(incfile, incfile->pstart);
}

void OScFeederDq::PreprocError2(const TDiagDefErr & adiag, string_view par1, string_view par2, string_view par3, OScPosition *ascpos, bool atryrecover)
{
  OScPosition * epos = ascpos;
  if (!epos)  epos = &scpos_start_directive;

  g_compiler->Error(adiag, par1, par2, par3, epos);

  // try to recover
  if (atryrecover)
  {
    if (preproc_closer_brace)
    {
      if (ReadToChar('{')) // find the closing
      {
        CheckSymbol("}"); // consume
      }
    }
    else // line end
    {
      if (ReadTo("\r\n"))
      {
        SkipSpaces(true);
      }
    }
  }
}

void OScFeederDq::PreprocError2(const TDiagDefErr & adiag, string_view par1, string_view par2, OScPosition * ascpos, bool atryrecover)
{
  PreprocError2(adiag, par1, par2, "", ascpos, atryrecover);
}

void OScFeederDq::PreprocError2(const TDiagDefErr & adiag, string_view par1, OScPosition * ascpos, bool atryrecover)
{
  PreprocError2(adiag, par1, "", "", ascpos, atryrecover);
}

void OScFeederDq::PreprocError2(const TDiagDefErr & adiag, OScPosition * ascpos, bool atryrecover)
{
  PreprocError2(adiag, "", "", "", ascpos, atryrecover);
}


bool OScFeederDq::CheckConditionals(const string aid)  // returns true if a conditional processed
{
  string sid;
  bool condok = false;
  bool condval = false;

  // starters

  if (("ifdef" == aid) || ("ifndef" == aid))
  {
    SkipSpaces();

    curcond = new OScfCondition(curcond, scpos_start_directive, inactive_code);

    if (!ReadIdentifier(sid))
    {
      PreprocError2(DQERR_CDIR_COND_ID_MISSING, "#" + aid);
      inactive_code = true;
    }
    else
    {
      if (not inactive_code)
      {
        bool isdefined = g_defines->Defined(sid);
        if ((("ifdef" == aid) and isdefined) or (("ifndef" == aid) and not isdefined))
        {
          curcond->branch_taken = true;
        }
        else
        {
          inactive_code = true;
        }
      }
    }

    if (g_opt.verbose)
    {
      print("{}: #{} \"{}\" {}\n", scpos_start_directive.Format(), aid, sid, (inactive_code ? "(inactive)" : ""));
    }
  }
  else if ("if" == aid)
  {
    curcond = new OScfCondition(curcond, scpos_start_directive, inactive_code);

    if (not inactive_code)
    {
      condval = g_compiler->ParseDefineCondition(scpos_start_directive, &condok);
      if (condok and condval)
      {
        curcond->branch_taken = true;
      }
      else
      {
        inactive_code = true;
      }
    }

    if (g_opt.verbose)
    {
      print("{}: #if = {} {}\n", scpos_start_directive.Format(), (condval ? "true" : "false"), (inactive_code ? "(inactive)" : ""));
    }
  }

  // closer

  else if ("endif" == aid)
  {
    if (!curcond)
    {
      PreprocError2(DQERR_CDIR_ENDIF_WITHOUT_IF);
    }
    else
    {
      if (curcond->startpos.scfile != curfile)
      {
        PreprocError2(DQERR_CDIR_COND_WRONG_INC, "#endif", "#if");
      }
      inactive_code = curcond->parent_inactive;
      curcond = curcond->parent;
      if (g_opt.verbose)
      {
        print("{}: #endif {}\n", scpos_start_directive.Format(), (inactive_code ? "(inactive)" : ""));
      }
    }
  }

  // continuing

  else if ("else" == aid)
  {
    if (!curcond)
    {
      PreprocError2(DQERR_CDIR_ELSE_WITHOUT_IF);
    }
    else if (FCOND_ELSE == curcond->state)
    {
      PreprocError2(DQERR_CDIR_MULTIPLE_ELSE_PREVPOS, curcond->elsepos.Format());
    }
    else
    {
      inactive_code = curcond->parent_inactive;

      if (curcond->startpos.scfile != curfile)  // same include ?
      {
        PreprocError2(DQERR_CDIR_COND_WRONG_INC, "#else", "#if");
        curcond = new OScfCondition(curcond, scpos_start_directive, inactive_code);
        inactive_code = true;
      }

      curcond->state = FCOND_ELSE;
      curcond->elsepos.Assign(scpos_start_directive);

      if (not inactive_code and curcond->branch_taken)
      {
        inactive_code = true;
      }

      if (g_opt.verbose)
      {
        print("{}: #else {}\n", scpos_start_directive.Format(), (inactive_code ? "(inactive)" : ""));
      }
    }
  }
  else if (("elifdef" == aid) || ("elifndef" == aid))
  {
    SkipSpaces();
    if (not ReadIdentifier(sid))
    {
      PreprocError2(DQERR_CDIR_COND_ID_MISSING, "#"+aid);
      inactive_code = true;
    }

    if (!curcond)
    {
      PreprocError2(DQERR_CDIR_ELCOND_WITHOUT_IF, "#"+aid);
      inactive_code = true;
    }
    else if (FCOND_ELSE == curcond->state)
    {
      PreprocError2(DQERR_CDIR_ELCOND_AFTER_ELSE_PREVPOS, "#"+aid, curcond->elsepos.Format()); // this message is not perfect
      inactive_code = true;
    }
    else
    {
      // restore the inactive code state
      inactive_code = (curcond->parent_inactive or curcond->branch_taken);

      if (curcond->startpos.scfile != curfile)  // same include ?
      {
        PreprocError2(DQERR_CDIR_COND_WRONG_INC, "#elifdef", "#if");
        curcond = new OScfCondition(curcond, scpos_start_directive, inactive_code);
        inactive_code = true;
      }

      curcond->state = FCOND_ELIF;
      if (not inactive_code)
      {
        bool isdefined = g_defines->Defined(sid);
        if ((("elifdef" == aid) and isdefined) or (("elifndef" == aid) and not isdefined))
        {
          curcond->branch_taken = true;
        }
        else
        {
          inactive_code = true;
        }
      }
    }

    if (g_opt.verbose)
    {
      print("{}: #{} \"{}\" {}\n", scpos_start_directive.Format(), aid, sid, (inactive_code ? "(inactive)" : ""));
    }
  }
  else if ("elif" == aid)
  {
    if (!curcond)
    {
      PreprocError2(DQERR_CDIR_ELCOND_WITHOUT_IF, "#elif");
      inactive_code = true;
    }
    else if (FCOND_ELSE == curcond->state)
    {
      PreprocError2(DQERR_CDIR_ELCOND_AFTER_ELSE_PREVPOS, "#elif", curcond->elsepos.Format());
      inactive_code = true;
    }
    else
    {
      inactive_code = (curcond->parent_inactive or curcond->branch_taken);

      if (curcond->startpos.scfile != curfile)
      {
        PreprocError2(DQERR_CDIR_COND_WRONG_INC, "#elif", "#if");
        curcond = new OScfCondition(curcond, scpos_start_directive, inactive_code);
        inactive_code = true;
      }

      curcond->state = FCOND_ELIF;
      if (not inactive_code)
      {
        condval = g_compiler->ParseDefineCondition(scpos_start_directive, &condok);
        if (condok and condval)
        {
          curcond->branch_taken = true;
        }
        else
        {
          inactive_code = true;
        }
      }
    }

    if (g_opt.verbose)
    {
      print("{}: #elif = {} {}\n", scpos_start_directive.Format(), (condval ? "true" : "false"), (inactive_code ? "(inactive)" : ""));
    }
  }
  else
  {
    return false;
  }

  FindDirectiveEnd();
  return true;
}

bool OScFeederDq::FindDirectiveEnd()
{
  // the #[{]id is already consumed, find the end

  if (preproc_closer_brace)
  {
    SkipSpaces();
    if (not CheckSymbol("}"))
    {
      PreprocError2(DQERR_CDIR_CLOSER_MISSING);
      return false;
    }
  }
  else // line end
  {
    if (ReadTo("\r\n"))
    {
      SkipSpaces(true);
    }
  }
  return true;
}
