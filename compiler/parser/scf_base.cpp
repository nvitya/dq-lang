/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    scf_base.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

// scf_base.cpp
// principles and some algorithms taken from strparseobj: github....

#include "string.h"
#include "math.h"

#include <fstream>
#include <print>
#include <format>
#include <filesystem>

#include "scf_base.h"
#include "comp_options.h"

bool PCharUCCompare(char * * ReadPtr, int len, const char * checkstring)
{
  char * cp = *ReadPtr;
  char * bufend = cp + len;
  char * csptr = (char *)checkstring;
  char * csend = csptr + strlen(checkstring);

  while ((csptr < csend) and (cp < bufend))
  {
    char c = *cp;
    if ((c >= 'a') && (c <= 'z'))  c &= 0xDF;

    if (c != *csptr)
    {
      break;
    }

    ++csptr;
    ++cp;
  }

  if (csptr != csend)
  {
    return false;
  }

  *ReadPtr = cp;
  return true;
}

int PCharToInt(char * ReadPtr, int len)
{
  char * cp = ReadPtr;
  char * endp = ReadPtr + len;
  int result = 0;

  while (cp < endp)
  {
    result = result * 10 + (*cp - '0');
    ++cp;
  }

  return result;
}

double PCharToFloat(char * ReadPtr, int len)
{
  if (len < 1)
  {
  	return 0.0;
  }

  char * pc = ReadPtr;
  char * pend = ReadPtr + len;

  double nsign = 1.0;
  double esign = 1.0;
  double fracmul = 1.0;
  //double digit = 0.0;
  double nv = 0.0;
  double ev = 0.0;
  double digit;

  int mode = 0; // 0 = integer part, 1 = fractional part, 2 = exponent
  char c;

  while (pc < pend)
  {
    c = *pc;
    if ('+' == c)
    {
      if (2 == mode)  esign = 1.0;
      else            nsign = 1.0;
    }
    else if ('-' == c)
    {
      if (2 == mode)  esign = -1.0;
      else            nsign = -1.0;
    }
    else if ((c >= '0') and (c <= '9'))
    {
      digit = c - '0';
      if (1 == mode) // fractional part
      {
        fracmul = fracmul * 0.1;
        nv = nv + digit * fracmul;
      }
      else if (2 == mode)  // exponential integer
      {
        ev = ev * 10 + digit;
      }
      else  // integer part
      {
        nv = nv * 10 + digit;
      }
    }
    else if (('.' == c) || (',' == c))
    {
      mode = 1; // change to fractional mode
    }
    else if (('e' == c) or ('E' == c))
    {
      mode = 2; // change to exponential mode
    }
    else
    {
      break;  // unhandled character
    };

    ++pc;
  }

  return nsign * nv * pow(10, esign * ev);
}

unsigned PCharHexToInt(char * ReadPtr, int len)
{
  char * cp = ReadPtr;
  char * endp = ReadPtr + len;
  int result = 0;

  while (cp < endp)
  {
    char c = *cp;
    if      (c >= '0' && c <= '9')  result = (result << 4) + (c - '0');
    else if (c >= 'A' && c <= 'F')  result = (result << 4) + (c - 'A' + 10);
    else if (c >= 'a' && c <= 'f')  result = (result << 4) + (c - 'a' + 10);
    else
      break;

    ++cp;
  }

  return result;
}

//-------------------------------------------------------------------------

void OScPosition::RecalcLineCol()  // this is slow
{
  if (!scfile or (pos < scfile->pstart) or (pos > scfile->pend))
  {
    line = 0;
    col = 0;
    return;
  }

  char *  p       = pos;
  char *  pstart  = scfile->pstart;

  int     linenum = 1;
  int     colnum  = 0;

  // colum counting
  while (p > pstart)
  {
    if ((*p == '\n') or (*p == '\r'))
    {
      break;
    }
    --p;
    ++colnum;
  }

  // line counting
  while (p > pstart)
  {
    if (*p == '\n')
    {
      ++linenum;
    }
    --p;
  }

  line = linenum;
  col  = colnum;
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
  pstart = nullptr;
  pend = nullptr;

  ifstream f(fullpath, ios::binary | ios::ate);
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
    clstart = pstart;
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

  SaveCurPos(prevpos);

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
  }

  return true;
}

bool OScFeederBase::ReadIdentifier(string & rvalue)
{
  char *   p = curp;

  SaveCurPos(prevpos);  // for precise error position tracking
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
    curp = p;
    curcol = (curp - clstart) + 1;
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

  string result = "";

  char *  savedpos = curp;
  char    startquote = *curp;  // single or double
  char    stopchars[5] = {startquote, '\\', '\n', '\r', 0};

  ++curp;  // skip "
  prevp = curp;

  while (true)
  {
    if (not ReadTo(&stopchars[0]))  // should not happen
    {
      curp = savedpos;
      return false;
    }

    result += PrevStr();

    if (startquote == *curp)  // end found !
    {
      ++curp; // skip the closing
      break;
    }
    else if ('\\' == *curp) // escape char
    {
      if (CheckSymbol("\\\"")) // Escaped " within the string literal
      {
        result += "\"";
      }
      else if (CheckSymbol("\\\'")) // Escaped ' within the string literal
      {
        result += "\'";
      }
      else if (CheckSymbol("\\n"))
      {
        result += "\n";
      }
      else if (CheckSymbol("\\r"))
      {
        result += "\r";
      }
      else if (CheckSymbol("\\t"))
      {
        result += "\t";
      }
      else if (CheckSymbol("\\\\")) // Escaped \\ within the string literal
      {
        result += "\\";
      }
      else // unhandled escape sequence
      {
        // leave it there
        result += "\\";
        ++curp;
      }
    }
    else // line ends
    {
      curp = savedpos;
      return false;
    }
  }

  rvalue = result;
  curcol = (curp - clstart) + 1;
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

    fpval = fpval * exp10(fpexp);
  }

  rvalue = fpval;
  curcol = (curp - clstart) + 1;
  return true;
}
