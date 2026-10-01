/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    scf_base.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

// scf_base.cpp
// principles and some algorithms taken from strparseobj: github....

#include "string.h"
#include <cmath>

#include <fstream>
#include <print>
#include <format>
#include <filesystem>

#include "scf_base.h"
#include "comp_options.h"
#include "dq_utils.h"
#include "source_overlay.h"
#include "strparse.h"

//-------------------------------------------------------------------------

void OScPosition::RecalcLineCol()  // this is slow
{
  if (!scfile or (pos < scfile->pstart) or (pos > scfile->pend))
  {
    line = 0;
    col = 0;
    return;
  }

  char * line_start = scfile->pstart;
  line = 1;
  for (char * p = scfile->pstart; p < pos; ++p)
  {
    if (*p == '\n')
    {
      ++line;
      line_start = p + 1;
    }
  }
  col = int(pos - line_start) + 1;
}

string OScPosition::Format()
{
  string result = "";

  if (!scfile)
  {
    return result;
  }

  result = format("{}({},{})", scfile->name, line, col);
  return result;
}


OScFile::OScFile()
{
  //
}

OScFile::~OScFile()
{
  body = "";
}

bool OScFile::Load(const string aname, const string afullpath)
{
  name = aname;
  fullpath = afullpath;
  body = "";
  length = -1;
  filetime = 0;
  pstart = nullptr;
  pend = nullptr;

  filesystem::path physical_path = g_source_overlay.PhysicalPath(fullpath);
  ifstream f(physical_path, ios::binary | ios::ate);
  if (!f)
  {
    return false;
  }

  length = f.tellg();
  body.resize(length);
  if (length > 0)
  {
    f.seekg(0);
    f.read(body.data(), length);
    pstart = body.data();
    pend = pstart + length;
  }

  error_code ec;
  auto ftime = filesystem::last_write_time(physical_path, ec);
  if (ec)
  {
    return false;
  }
  filetime = FileTimeTicks(ftime);

  if (g_opt.dbg_info)
  {
    di_file = di_builder->createFile(name, ExtractFilePath(afullpath));
  }

  return true;
}

string ExtractFilePath(const string & full_path)
{
  filesystem::path path_obj(full_path);
  return path_obj.parent_path().string();
}

string ExtractFileName(const string & full_path)
{
  filesystem::path path_obj(full_path);
  return path_obj.filename().string();
}

//---------------------------------------------------------

OScFeederBase::OScFeederBase()
{
}

OScFeederBase::~OScFeederBase()
{
  Reset();
}

int OScFeederBase::Init(const string afilename)
{
  Reset();
  return 0;
}

void OScFeederBase::Reset()
{
  curfile = nullptr;
  curp = nullptr;
  bufend = nullptr;
  clstart = nullptr;
  curline = 1;
  curcol  = 1;
  last_token_end_line = 1;
}

string OScFeederBase::PrevStr()
{
  string result(prevp, prevlen);
  return result;
}

void OScFeederBase::SaveCurPos(OScPosition & rpos)
{
  rpos.scfile = curfile;
  rpos.pos    = curp;
  rpos.line   = curline;
  rpos.col    = curcol;
}

void OScFeederBase::SetCurPos(OScPosition & rpos)
{
  curfile = rpos.scfile;
  curp    = rpos.pos;
  curline = rpos.line;
  curcol  = rpos.col;
  last_token_end_line = curline;

  bufend  = curfile->pend;
  prevp   = curp;
  prevlen = 0;
  SearchClStart();
}

void OScFeederBase::SetCurPos(OScFile * afile, char * apos)
{
  curfile = afile;
  if (curfile)
  {
    bufend = curfile->pend;
    curp = apos;
    prevp = curp;
    prevlen = 0;

    RecalcCurLineCol();
    // A token from another file cannot share this file's line numbering.
    last_token_end_line = 0;
  }
  else
  {
    bufend = nullptr;
    curp = nullptr;
    prevp = nullptr;
    clstart = nullptr;
    prevlen = 0;
    curline = 0;
    curcol  = 0;
    last_token_end_line = 0;
  }
}

void OScFeederBase::SearchClStart()
{
  if (!curfile)
  {
    clstart = nullptr;
    return;
  }

  char * pstart = curfile->pstart;
  char * p = curp;
  clstart = pstart;
  if (curp > curfile->pstart)
  {
    while (p > pstart)
    {
      if ((*p == '\n') or (*p == '\r'))
      {
        clstart = p + 1;
        return;
      }
      --p;
    }
    // no previous linefeed was found, so point to the file start
  }
}

void OScFeederBase::RecalcCurLineCol()
{
  OScPosition scpos(curfile, curp); // calculates the line, col internally, this is slow
  curline = scpos.line;
  curcol  = scpos.col;
  SearchClStart();
}

void OScFeederBase::SkipSpaces(bool askiplineend)
{
  char * cp = curp;
  while ( (cp < bufend) && ( (*cp == 32) || (*cp == 9) || (askiplineend && ((*cp == 13) || (*cp == 10))) ) )
  {
    if (*cp == line_end_char)
    {
      ++curline;
      clstart = cp + 1;
    }
    ++cp;
  }
  curp = cp;
  curcol = (curp - clstart) + 1;
}

/*
  skips line end too, but LineLength does not contain the line end chars
  bufend shows the end of the buffer (one after the last character)
  so bufend-bufstart = buffer length
  returns false if end of buffer reached without line end
*/
bool OScFeederBase::ReadLine()
{
  char * p;

  prevp = curp;
  p = curp;

  while ((p < bufend) and (*p != 13) and (*p != 10))
  {
    ++p;
  }

  prevlen = p - curp;

  // skip the line end, but only one!
  if ((p < bufend) and (*p == 13))
  {
    if (*p == line_end_char)
    {
      ++curline;
    }
    ++p;
    clstart = p;
  }
  if ((p < bufend) and (*p == 10))
  {
    if (*p == line_end_char)
    {
      ++curline;
    }
    ++p;
    clstart = p;
  }

  curp = p;
  curcol = (curp - clstart) + 1;
  last_token_end_line = curline;
  return (prevp < bufend);
}

bool OScFeederBase::ReadTo(const char * checkchars)
{
  char *  p = curp;
  char *  ccstart = (char *)checkchars;
  char *  ccend = ccstart + strlen(checkchars);
  char *  ccptr;

  while (p < bufend)
  {
    // check chars
    ccptr = ccstart;
    while (ccptr < ccend)
    {
      if (*ccptr == *p)
      {
        prevlen = p - curp;
        curp = p;
        curcol = (curp - clstart) + 1;
        last_token_end_line = curline;
        return true;
      }
      ++ccptr;
    }

    if (*p == line_end_char)
    {
      ++curline;
      clstart = p + 1;
    }

    ++p;
  }

  // end of buffer, store the remaining length:
  prevlen = p - curp;
  curp = p;
  curcol = (curp - clstart) + 1;
  return false;
}

bool OScFeederBase::ReadToChar(char achar)
{
  char * p = curp;
  bool result = false;

  prevp = curp;

  while (p < bufend)
  {
    if (*p == achar)
    {
      curcol = (curp - clstart) + 1;
      last_token_end_line = curline;
      result = true;
      break;
    }

    if (*p == line_end_char)
    {
      ++curline;
      clstart = p + 1;
    }
    ++p;
  }

  prevlen = p - curp;
  curp = p;
  curcol = (curp - clstart) + 1;
  return result;
}

bool OScFeederBase::SearchPattern(const char * checkstr, bool aconsume)  // reads until the checkstring is found, readptr points to matching start
{
  char *    p;
  char *    cps = curp;
  char *    csstart = (char *)checkstr;
  unsigned  csslen  = strlen(checkstr);
  char *    csend   = csstart + csslen;
  char *    ccptr;
  int       newcurline = curline;
  char *    newclstart = clstart;

  // check start pos cycle
  while (cps < bufend - csslen)
  {
    // check chars cycle
    p = cps;
    ccptr = csstart;
    char match = 1;
    while (ccptr < csend)
    {
      if (*p != *ccptr)
      {
        match = 0;
        break;
      }
      ++p;
      ++ccptr;
    }

    if (match)
    {
      // does not skip the matching pattern, readptr points to the matching pattern

      prevlen = cps - curp;
      if (aconsume)
      {
        curp = p;
      }
      else
      {
        curp = cps;
      }
      curline = newcurline;
      clstart = newclstart;
      curcol = (curp - clstart) + 1;
      if (aconsume) last_token_end_line = curline;
      return true;
    }

    if (*cps == line_end_char)
    {
      ++newcurline;
      newclstart = cps + 1;
    }
    ++cps;
  }

  return false;
}

bool OScFeederBase::CheckSymbol(const char * checkstring, bool aconsume)
{
  // the checkstring should not contain the line_end_char

  char *  p = curp;
  char *  csptr = (char *)checkstring;
  char *  csend = csptr + strlen(checkstring);

  if (prevpos.pos != p)
  {
    SaveCurPos(prevpos);  // for precise error position tracking
  }

  while ((csptr < csend) && (p < bufend) && (*csptr == *p))
  {
    ++csptr;
    ++p;
  }

  if (csptr != csend)
  {
    return false;
  }

  if (aconsume)
  {
    curp = p;
    curcol = (curp - clstart) + 1;
    last_token_end_line = curline;
  }

  return true;
}

bool OScFeederBase::CheckKeyword(const char * checkstring, bool aconsume)
{
  auto is_identifier_char = [](char c)
  {
    return ((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z'))
        or ((c >= '0') and (c <= '9')) or ('_' == c);
  };

  const size_t len = strlen(checkstring);
  if ((curp > curfile->pstart && is_identifier_char(curp[-1]))
      || (size_t(bufend - curp) > len && is_identifier_char(curp[len])))
  {
    return false;
  }

  return CheckSymbol(checkstring, aconsume);
}

bool OScFeederBase::ReadIdentifier(string & rvalue, bool aconsume)
{
  if (prevpos.pos != curp)
  {
    SaveCurPos(prevpos);  // for precise error position tracking
  }
  char *   p = curp;
  prevp = curp;
  while (p < bufend)
  {
    char c = *p;

    if (
        ((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z')) or (c == '_')  // allowed anywhere
        or ((p != curp) and (c >= '0') and (c <= '9'))  // numbers can not be the first one
       )
    {
      ++p;
    }
    else
    {
      break;
    }
  }

  prevlen = p - curp;
  if (prevlen > 0)
  {
    if (aconsume)
    {
      curp = p;
      curcol = (curp - clstart) + 1;
      last_token_end_line = curline;
    }
    rvalue.assign(prevp, prevlen);
    return true;
  }

  return false;
}

bool OScFeederBase::IsIntLiteral()
{
  if (curp >= bufend)  return false;
  if ((*curp >= '0') and (*curp <= '9'))   return true;
  if ( ((curp + 1) < bufend) and ((curp[0] == '-') or (curp[0] == '+'))
       and (curp[1] >= '0') and (curp[1] <= '9')
     )
  {
    return true;
  }
  return false;
}

bool OScFeederBase::IsNumChar()
{
  if (curp >= bufend)  return false;
  if ((*curp >= '0') and (*curp <= '9'))   return true;
  return false;
}

bool OScFeederBase::ReadInt64Value(int64_t & rvalue)
{
  char *   p = curp;
  int64_t  result = 0;
  int64_t  signmul = 1;
  bool     bok = false;

  SaveCurPos(prevpos);  // for precise error position tracking
  prevp = curp;
  while (p < bufend)
  {
    char c = *p;
    if ((c >= '0') and (c <= '9'))
    {
      result = result * 10 + (c - '0');
      bok = true;
    }
    else if ((c == '-') and (p == curp))
    {
      signmul = -1;
    }
    else if ((c == '+') and (p == curp))
    {
      signmul = 1; // not required
    }
    else
    {
      break;  // invalid char for the integer literal
    }

    ++p;
  }

  if (bok)
  {
    prevlen = p - curp;
    curp = p; // consume
    curcol = (curp - clstart) + 1;
    last_token_end_line = curline;
    rvalue = result;
  }

  return bok;
}

bool OScFeederBase::ReadHex64Value(uint64_t & rvalue)
{
  char *    p = curp;
  uint64_t  result = 0;

  SaveCurPos(prevpos);  // for precise error position tracking
  prevp = curp;
  while (p < bufend)
  {
    char c = *p;
    if ((c >= '0') and (c <= '9'))
    {
      result = (result << 4) + (c - '0');
    }
    else if ((c >= 'a') and (c <= 'f'))
    {
      result = (result << 4) + (c - 'a' + 10);
    }
    else if ((c >= 'A') and (c <= 'F'))
    {
      result = (result << 4) + (c - 'A' + 10);
    }
    else
    {
      break;  // invalid char for the integer literal
    }

    ++p;
  }

  prevlen = p - curp;
  if (prevlen > 0)
  {
    curp = p; // consume
    curcol = (curp - clstart) + 1;
    last_token_end_line = curline;
    rvalue = result;
    return true;
  }
  else
  {
    return false;
  }
}

bool OScFeederBase::ReadQuotedString(string & rvalue)
{
  SaveCurPos(prevpos);  // for precise error position tracking
  prevp = curp;

  if ( (curp >= bufend) or ((*curp != '"') and (*curp != '\'')) )
  {
    return false;
  }

  OScPosition savedpos = prevpos;
  char startquote = *curp;
  bool triple_quoted = (curp + 2 < bufend) && (curp[1] == startquote) && (curp[2] == startquote);
  char * content_start = curp + (triple_quoted ? 3 : 1);
  char * p = content_start;
  char * content_end = nullptr;

  while (p < bufend)
  {
    if (*p == '\\')
    {
      // A quote preceded by a backslash is part of the content.  Newlines
      // remain invalid in ordinary (non-triple-quoted) string literals.
      if ((p + 1 >= bufend) || ((!triple_quoted) && ((p[1] == '\n') || (p[1] == '\r'))))
      {
        break;
      }
      p += 2;
      continue;
    }

    if (!triple_quoted && ((*p == '\n') || (*p == '\r')))
    {
      break;
    }

    if (*p == startquote)
    {
      if (!triple_quoted)
      {
        content_end = p;
        ++p;
        break;
      }
      if ((p + 2 < bufend) && (p[1] == startquote) && (p[2] == startquote))
      {
        content_end = p;
        p += 3;
        break;
      }
    }
    ++p;
  }

  if (!content_end)
  {
    SetCurPos(savedpos);
    prevpos = savedpos;
    return false;
  }

  string raw_value(content_start, content_end);
  if (triple_quoted)
  {
    // The closing delimiter establishes the indentation removed from each
    // content line.  It must otherwise be alone on its line to trim that
    // line's preceding newline.
    char * closing_line_start = content_end;
    while ((closing_line_start > curfile->pstart)
           && (closing_line_start[-1] != '\n') && (closing_line_start[-1] != '\r'))
    {
      --closing_line_start;
    }
    bool closing_delimiter_on_own_line = true;
    for (char * q = closing_line_start; q < content_end; ++q)
    {
      if ((*q != ' ') && (*q != '\t'))
      {
        closing_delimiter_on_own_line = false;
        break;
      }
    }

    string indentation;
    if (closing_delimiter_on_own_line)
    {
      indentation.assign(closing_line_start, content_end);

      size_t content_before_indentation = raw_value.size() - indentation.size();
      if (content_before_indentation > 0)
      {
        size_t newline_start = content_before_indentation;
        if (raw_value[newline_start - 1] == '\n')
        {
          --newline_start;
          if ((newline_start > 0) && (raw_value[newline_start - 1] == '\r')) --newline_start;
          raw_value.erase(newline_start);
        }
        else if (raw_value[newline_start - 1] == '\r')
        {
          raw_value.erase(newline_start - 1);
        }
      }
    }

    if (raw_value.starts_with("\r\n")) raw_value.erase(0, 2);
    else if (!raw_value.empty() && ((raw_value[0] == '\n') || (raw_value[0] == '\r'))) raw_value.erase(0, 1);

    if (!indentation.empty())
    {
      string dedented;
      for (size_t line_start = 0; line_start < raw_value.size(); )
      {
        size_t line_end = raw_value.find_first_of("\r\n", line_start);
        if (line_end == string::npos) line_end = raw_value.size();
        size_t text_start = line_start;
        if (raw_value.compare(line_start, indentation.size(), indentation) == 0)
        {
          text_start += indentation.size();
        }
        dedented.append(raw_value, text_start, line_end - text_start);
        if (line_end == raw_value.size()) break;
        if ((raw_value[line_end] == '\r') && (line_end + 1 < raw_value.size()) && (raw_value[line_end + 1] == '\n'))
        {
          dedented += "\r\n";
          line_start = line_end + 2;
        }
        else
        {
          dedented += raw_value[line_end];
          line_start = line_end + 1;
        }
      }
      raw_value = dedented;
    }
  }

  string result;
  for (size_t i = 0; i < raw_value.size(); ++i)
  {
    if ((raw_value[i] != '\\') || (i + 1 >= raw_value.size()))
    {
      result += raw_value[i];
      continue;
    }

    char escaped = raw_value[++i];
    switch (escaped)
    {
      case '"': result += '"'; break;
      case '\'': result += '\''; break;
      case 'n': result += '\n'; break;
      case 'r': result += '\r'; break;
      case 't': result += '\t'; break;
      case '\\': result += '\\'; break;
      case 'x':
      {
        if ((i + 2 < raw_value.size()))
        {
          int high = HexDigitValue(raw_value[i + 1]);
          int low = HexDigitValue(raw_value[i + 2]);
          if ((high >= 0) && (low >= 0))
          {
            result += char((high << 4) | low);
            i += 2;
            break;
          }
        }
        result += '\\';
        result += escaped;
        break;
      }
      default:
        result += '\\';
        result += escaped;
        break;
    }
  }

  curp = p;
  RecalcCurLineCol();
  last_token_end_line = curline;
  rvalue = result;
  return true;
}

bool OScFeederBase::ReadDecimalNumbers()
{
  char * cp = curp;
  bool result = false;

  prevp = curp;

  while (cp < bufend)
  {
    char c = *cp;

    if (
        ((c >= '0') && (c <= '9'))
       )
    {
      result = true;
      ++cp;
    }
    else
    {
      break;
    }
  }

  prevlen = cp - curp;
  curp = cp;
  curcol = (curp - clstart) + 1;
  if (result) last_token_end_line = curline;

  return result;
}

bool OScFeederBase::ReadHexNumbers()
{
  char * cp = curp;
  bool result = false;

  prevp = curp;

  while (cp < bufend)
  {
    char c = *cp;

    if (
        ((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'F')) || ((c >= 'a') && (c <= 'f'))
       )
    {
      result = true;
      ++cp;
    }
    else
    {
      break;
    }
  }

  prevlen = cp - curp;
  curp = cp;
  curcol = (curp - clstart) + 1;
  if (result) last_token_end_line = curline;

  return result;
}

bool OScFeederBase::ReadFloatNum()
{
  char * cp = curp;
  bool result = false;

  prevp = curp;

  while (cp < bufend)
  {
    char c = *cp;

    if (
        ((c >= '0') && (c <= '9'))
        || (c == '.') || (c == '-') || ('+' == c) || ('e' == c) || ('E' == c)
       )
    {
      result = true;
      ++cp;
    }
    else
    {
      break;
    }
  }

  prevlen = cp - curp;
  curp = cp;
  curcol = (curp - clstart) + 1;
  if (result) last_token_end_line = curline;

  return result;
}

bool OScFeederBase::ReadFloatFracExp(double & rvalue)
{
  // examples: 0.123, 2.1e-5, 1.234E6, 0.

  double fpval = rvalue;

  if ('.' == *curp)
  {
    // parse fractional part
    ++curp;
    double  fpdigmul = 0.1;
    while (curp < bufend)
    {
      char c = *curp;
      if (c >= '0' and c <= '9')
      {
        fpval = fpval + (c - '0') * fpdigmul;
        fpdigmul = fpdigmul * 0.1;
      }
      else
      {
        break;
      }
      ++curp;
    }
  }

  if (('e' == *curp) || ('E' == *curp))
  {
    // parse exponential part
    ++curp;
    double fpexp = 0.0;
    double fpexpsign = 1;
    bool expok = false;
    while (curp < bufend)
    {
      char c = *curp;
      if ('-' == c)
      {
        fpexpsign = -1;
      }
      else if (c >= '0' and c <= '9')
      {
        fpexp = fpexp * 10 + (c - '0');
        expok = true;
      }
      else
      {
        break;
      }
      ++curp;
    }

    if (!expok)
    {
      return false;
    }

    fpval = fpval * std::pow(10.0, fpexpsign * fpexp);
  }

  rvalue = fpval;
  curcol = (curp - clstart) + 1;
  last_token_end_line = curline;
  return true;
}
