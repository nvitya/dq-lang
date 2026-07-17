/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
#include <algorithm>
#include <chrono>

#include "scf_dq.h"
#include "scope_defines.h"
#include "scope_builtins.h"

#include "dqc.h"
#include "module_path.h"

//---------------------------------------------------------

static bool IsValidLinkLibName(const string & aname)
{
  if (aname.empty())
  {
    return false;
  }

  for (char c : aname)
  {
    if (    ((c >= 'A') && (c <= 'Z'))
         || ((c >= 'a') && (c <= 'z'))
         || ((c >= '0') && (c <= '9'))
         || ('_' == c)
         || ('-' == c)
         || ('.' == c)
         || ('+' == c) )
    {
      continue;
    }

    return false;
  }

  return true;
}

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

  error_code ec;
  main_source_path = filesystem::absolute(afilename, ec);
  if (ec) main_source_path = afilename;
  main_source_path = main_source_path.lexically_normal();
  header_source_path = main_source_path;
  header_source_path.replace_extension(".dqh");
  curfile = LoadFile(main_source_path);
  if (!curfile)
  {
    return 1;
  }
  if (!AddSourceDependency(curfile))
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
  source_dependencies.clear();
  main_source_path.clear();
  header_source_path.clear();
  implementation_section = false;
}

OScFile * OScFeederDq::LoadFile(const filesystem::path & filename)
{
  error_code ec;
  filesystem::path fullpath = filesystem::absolute(filename, ec);
  if (ec) fullpath = filename;
  fullpath = fullpath.lexically_normal();

  // check if already loaded
  for (OScFile * fs : scfiles)
  {
    if (filesystem::path(fs->fullpath) == fullpath)
    {
      ++fs->usagecount;  // increment usage count for #{include_once}
      return fs;
    }
  }

  OScFile * f = new OScFile();
  if (!f->Load(fullpath.filename().string(), fullpath.string()))
  {
    print("Error loading file: \"{}\"!\n", fullpath.string());

    delete f;
    return nullptr;
  }

  if (g_opt.verblevel >= VERBLEVEL_INFO)
  {
    print("File \"{}\" loaded: {} bytes\n", fullpath.string(), f->length);
  }

  scfiles.push_back(f);
  f->index = scfiles.size() - 1;
  return f;
}

bool OScFeederDq::ResolveSourcePath(const string & source_text, filesystem::path & rpath) const
{
  filesystem::path raw_path(source_text);
  filesystem::path candidate;
  if (source_text.starts_with("./") || source_text.starts_with("../"))
  {
    candidate = filesystem::path(curfile->fullpath).parent_path() / raw_path;
  }
  else if (source_text.starts_with("^/"))
  {
    OModulePath current_module;
    string path_error;
    if (!current_module.InitCurrent(main_source_path, path_error))
    {
      return false;
    }
    candidate = current_module.root_dir / source_text.substr(2);
  }
  else
  {
    candidate = filesystem::path(curfile->fullpath).parent_path() / raw_path;
    error_code ec;
    if (!filesystem::exists(candidate, ec) || ec)
    {
      for (auto it = g_opt.package_paths.rbegin(); it != g_opt.package_paths.rend(); ++it)
      {
        filesystem::path package_candidate = filesystem::path(*it) / raw_path;
        ec.clear();
        if (filesystem::exists(package_candidate, ec) && !ec)
        {
          candidate = package_candidate;
          break;
        }
      }
    }
  }

  error_code ec;
  rpath = filesystem::absolute(candidate, ec);
  if (ec) rpath = candidate;
  rpath = rpath.lexically_normal();
  return true;
}

bool OScFeederDq::AddSourceDependency(const filesystem::path & path)
{
  error_code ec;
  filesystem::path fullpath = filesystem::absolute(path, ec);
  if (ec) fullpath = path;
  fullpath = fullpath.lexically_normal();
  string filename = fullpath.string();
  for (const SSourceDependency & dep : source_dependencies)
  {
    if (dep.filename == filename) return true;
  }

  uintmax_t filesize = filesystem::file_size(fullpath, ec);
  if (ec) return false;
  auto filetime = filesystem::last_write_time(fullpath, ec);
  if (ec) return false;

  SSourceDependency dep;
  dep.filename = filename;
  dep.filesize = int64_t(filesize);
  dep.filetime = int64_t(chrono::duration_cast<chrono::nanoseconds>(filetime.time_since_epoch()).count());
  source_dependencies.push_back(dep);
  return true;
}

bool OScFeederDq::AddSourceDependency(const OScFile * file)
{
  if (!file) return false;
  filesystem::path fullpath(file->fullpath);
  string filename = fullpath.lexically_normal().string();
  for (const SSourceDependency & dep : source_dependencies)
  {
    if (dep.filename == filename) return true;
  }

  SSourceDependency dep;
  dep.filename = filename;
  dep.filesize = file->length;
  dep.filetime = file->filetime;
  source_dependencies.push_back(dep);
  return true;
}

bool OScFeederDq::IsDirectIncludeSource() const
{
  if (!curfile) return false;
  filesystem::path current_path(curfile->fullpath);
  return (current_path == main_source_path) || (current_path == header_source_path);
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
  else if ("srcdep" == sid)
  {
    ParseDirectiveSourceDependency();
  }
  else if ("linklib" == sid)
  {
    ParseDirectiveLinkLib();
  }
  else if ("opt" == sid)
  {
    ParseDirectiveOpt();
  }
  else if ("include_once" == sid)
  {
    FindDirectiveEnd();
    if (curfile->usagecount > 1)
    {
      // skip to the end
      curp = bufend;

      if (g_opt.verblevel >= VERBLEVEL_DEBUG)
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

void OScFeederDq::ParseDirectiveOpt()
{
  string optname;

  SkipSpaces(false);
  if (!ReadIdentifier(optname))
  {
    PreprocError2(DQERR_CDIR_OPT_SYNTAX);
    return;
  }

  if ("module_root_depth" != optname)
  {
    PreprocError2(DQERR_CDIR_OPT_UNKNOWN, optname);
    return;
  }

  SkipSpaces(false);
  if (!CheckSymbol("="))
  {
    PreprocError2(DQERR_CDIR_OPT_SYNTAX);
    return;
  }

  int64_t value = 0;
  SkipSpaces(false);
  if (!ReadInt64Value(value) || (value < 0))
  {
    PreprocError2(DQERR_CDIR_OPT_SYNTAX);
    return;
  }

  if (!FindDirectiveEnd())
  {
    return;
  }

  g_opt.module_root_depth = int(value);
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

void OScFeederDq::ParseDirectiveLinkLib()
{
  // #linklib("name") or #linklib('name')
  // note: linklib already consumed

  string libname;

  SkipSpaces(false);
  if (not CheckSymbol("("))
  {
    PreprocError2(DQERR_CDIR_LINKLIB_SYNTAX);
    return;
  }

  SkipSpaces(false);
  if (not ReadQuotedString(libname))
  {
    PreprocError2(DQERR_CDIR_LINKLIB_SYNTAX);
    return;
  }

  SkipSpaces(false);
  if (not CheckSymbol(")"))
  {
    PreprocError2(DQERR_CDIR_LINKLIB_SYNTAX);
    return;
  }

  if (not IsValidLinkLibName(libname))
  {
    PreprocError2(DQERR_CDIR_LINKLIB_INVALID, libname);
    return;
  }

  if (not FindDirectiveEnd())
  {
    return;
  }

  if (g_opt.link_libraries.end() == find(g_opt.link_libraries.begin(), g_opt.link_libraries.end(), libname))
  {
    g_opt.link_libraries.push_back(libname);
  }

  if (g_opt.verblevel >= VERBLEVEL_INFO)
  {
    print("{}: ", scpos_start_directive.Format());
    print("Linking library ({})\n", libname);
  }
}

void OScFeederDq::ParseDirectiveInclude()
{
  // #include "filename.dqi" or #include header
  // note: include already consumed!

  string sfname;
  bool include_header = false;

  SkipSpaces(false);

  string include_kind;
  if (ReadIdentifier(include_kind, false))
  {
    if ("header" != include_kind)
    {
      PreprocError2(DQERR_CDIR_INC_FN_MISSING);
      return;
    }
    ReadIdentifier(include_kind);
    include_header = true;
  }
  else if (not ReadQuotedString(sfname))
  {
    PreprocError2(DQERR_CDIR_INC_FN_MISSING);
    return;
  }

  if (not FindDirectiveEnd())
  {
    return;
  }

  filesystem::path include_path;
  if (include_header)
  {
    if (implementation_section || (filesystem::path(curfile->fullpath).extension() == ".dqh"))
    {
      PreprocError2(DQERR_CDIR_INC_HEADER_INVALID, nullptr, false);
      return;
    }
    include_path = header_source_path;
    sfname = include_path.string();
  }
  else if (!ResolveSourcePath(sfname, include_path))
  {
    PreprocError2(DQERR_CDIR_INC_LOADING, sfname);
    return;
  }

  if (g_opt.verblevel >= VERBLEVEL_INFO)
  {
    print("{}: ", scpos_start_directive.Format());
    print("Including ({})\n", include_path.string());
  }

  bool track_dependency = !implementation_section && (include_header || IsDirectIncludeSource());
  OScFile * incfile = LoadFile(include_path);
  if (!incfile)
  {
    PreprocError2(DQERR_CDIR_INC_LOADING, sfname);
    return;
  }
  if (track_dependency && !AddSourceDependency(incfile))
  {
    PreprocError2(DQERR_CDIR_INC_LOADING, sfname);
    return;
  }

  // push return position

  OScPosition  retpos;
  SaveCurPos(retpos);            // save the current position (pointing after the directive)
  returnpos.push_back(retpos);   // add to the return stack

  // switch to the include
  SetCurPos(incfile, incfile->pstart);
}

void OScFeederDq::ParseDirectiveSourceDependency()
{
  string source_name;
  SkipSpaces(false);
  if (!ReadQuotedString(source_name))
  {
    PreprocError2(DQERR_CDIR_SRCDEP_SYNTAX);
    return;
  }
  if (!FindDirectiveEnd()) return;
  if (implementation_section)
  {
    PreprocError2(DQERR_CDIR_SRCDEP_PLACEMENT, nullptr, false);
    return;
  }

  filesystem::path source_path;
  if (!ResolveSourcePath(source_name, source_path) || !AddSourceDependency(source_path))
  {
    PreprocError2(DQERR_CDIR_SRCDEP_LOADING, source_name, nullptr, false);
    return;
  }

  if (g_opt.verblevel >= VERBLEVEL_INFO)
  {
    print("{}: Source dependency ({})\n", scpos_start_directive.Format(), source_path.string());
  }
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

void OScFeederDq::StartConditionalBranch()
{
  curcond = new OScfCondition(curcond, scpos_start_directive, inactive_code);
}

void OScFeederDq::ApplyConditionalResult(bool acondition_true)
{
  if (not inactive_code)
  {
    if (acondition_true)
    {
      curcond->branch_taken = true;
    }
    else
    {
      inactive_code = true;
    }
  }
}

bool OScFeederDq::ContinueConditionalBranch(const string & adirective)
{
  if (!curcond)
  {
    PreprocError2(DQERR_CDIR_ELCOND_WITHOUT_IF, adirective);
    inactive_code = true;
    return false;
  }

  if (FCOND_ELSE == curcond->state)
  {
    PreprocError2(DQERR_CDIR_ELCOND_AFTER_ELSE_PREVPOS, adirective, curcond->elsepos.Format()); // this message is not perfect
    inactive_code = true;
    return false;
  }

  // restore the inactive code state
  inactive_code = (curcond->parent_inactive or curcond->branch_taken);

  if (curcond->startpos.scfile != curfile)  // same include ?
  {
    PreprocError2(DQERR_CDIR_COND_WRONG_INC, adirective, "#if");
    curcond = new OScfCondition(curcond, scpos_start_directive, inactive_code);
    inactive_code = true;
  }

  curcond->state = FCOND_ELIF;
  return (not inactive_code);
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

    StartConditionalBranch();

    if (!ReadIdentifier(sid))
    {
      PreprocError2(DQERR_CDIR_COND_ID_MISSING, "#" + aid);
      inactive_code = true;
    }
    else if (not inactive_code)
    {
      bool isdefined = g_defines->Defined(sid);
      ApplyConditionalResult((("ifdef" == aid) and isdefined) or (("ifndef" == aid) and not isdefined));
    }

    if (g_opt.verblevel >= VERBLEVEL_DEBUG)
    {
      print("{}: #{} \"{}\" {}\n", scpos_start_directive.Format(), aid, sid, (inactive_code ? "(inactive)" : ""));
    }
  }
  else if ("if" == aid)
  {
    StartConditionalBranch();

    if (not inactive_code)
    {
      condval = g_compiler->ParseDefineCondition(scpos_start_directive, &condok);
      ApplyConditionalResult(condok and condval);
    }

    if (g_opt.verblevel >= VERBLEVEL_DEBUG)
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
      if (g_opt.verblevel >= VERBLEVEL_DEBUG)
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

      if (g_opt.verblevel >= VERBLEVEL_DEBUG)
      {
        print("{}: #else {}\n", scpos_start_directive.Format(), (inactive_code ? "(inactive)" : ""));
      }
    }
  }
  else if (("elifdef" == aid) || ("elifndef" == aid))
  {
    string directive = "#" + aid;

    SkipSpaces();
    if (not ReadIdentifier(sid))
    {
      PreprocError2(DQERR_CDIR_COND_ID_MISSING, directive);
      inactive_code = true;
    }

    if (ContinueConditionalBranch(directive))
    {
      bool isdefined = g_defines->Defined(sid);
      ApplyConditionalResult((("elifdef" == aid) and isdefined) or (("elifndef" == aid) and not isdefined));
    }

    if (g_opt.verblevel >= VERBLEVEL_DEBUG)
    {
      print("{}: #{} \"{}\" {}\n", scpos_start_directive.Format(), aid, sid, (inactive_code ? "(inactive)" : ""));
    }
  }
  else if ("elif" == aid)
  {
    if (ContinueConditionalBranch("#elif"))
    {
      condval = g_compiler->ParseDefineCondition(scpos_start_directive, &condok);
      ApplyConditionalResult(condok and condval);
    }

    if (g_opt.verblevel >= VERBLEVEL_DEBUG)
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
