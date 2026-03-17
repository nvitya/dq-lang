/* -----------------------------------------------------------------------------
 * This file was originated here: https://github.com/nvitya/cpputils
 * Copyright (c) 2023 Viktor Nagy, nvitya
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software. Permission is granted to anyone to use this
 * software for any purpose, including commercial applications, and to alter
 * it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software in
 *    a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 * --------------------------------------------------------------------------- */
/*
 *  file:     strparse.cpp
 *  brief:    light string parser using pointers, avoiding copiing
 *  created:  2023-05-31
 *  authors:  nvitya
*/

#include "strparse.h"
#include "string.h"
#include <strings.h>
#include "math.h"

TStrParseObj::TStrParseObj()
{
  Init(NULL, 0);
}

TStrParseObj::TStrParseObj(char * astr, int buflen)
{
  Init(astr, buflen);
}

void TStrParseObj::Init(char * astr, int buflen)
{
  bufstart = astr;
  if (buflen == 0 && (bufstart != NULL))
  {
    buflen = strlen(astr);
  }
  bufend = bufstart + buflen;

  readptr = bufstart;
  prevptr = bufstart;
  prevlen = 0;
}

void TStrParseObj::SkipSpaces(bool askiplineend)
{
  char * cp;
  cp = readptr;
  while ( (cp < bufend) && ( (*cp == 32) || (*cp == 9) || (askiplineend && ((*cp == 13) || (*cp == 10))) ) )
  {
    ++cp;
  }
  readptr = cp;
}

void TStrParseObj::SkipWhite()
{
  while (1)
  {
    SkipSpaces();
    if ((readptr < bufend) && (*readptr == commentmarker))
    {
      ReadTo("\n\r");
    }
    else
    {
      return;
    }
  }
}


/*
  skips line end too, but LineLength does not contain the line end chars
  bufend shows the end of the buffer (one after the last character)
  so bufend-bufstart = buffer length
  returns false if end of buffer reached without line end
*/
bool TStrParseObj::ReadLine()
{
  char * cp;

  prevptr = readptr;
  cp = readptr;

  while ((cp < bufend) && (*cp != 13) && (*cp != 10))
  {
    ++cp;
  }

  prevlen = cp - readptr;

  // skip the line end, but only one!
  if ((cp < bufend) && (*cp == 13))  ++cp;
  if ((cp < bufend) && (*cp == 10))  ++cp;

  readptr = cp;

  return (prevptr < bufend);
}

bool TStrParseObj::ReadTo(const char * checkchars)  // reads until one of the checkchars reached.
{
  char * cp;
  char * ccstart;
  char * ccend;
  char * ccptr;

  prevptr = readptr;
  cp = readptr;
  ccstart = (char *)checkchars;
  ccend = ccstart + strlen(checkchars);

  while (cp < bufend)
  {
    // check chars
    ccptr = ccstart;
    while (ccptr < ccend)
    {
      if (*ccptr == *cp)
      {
        prevlen = cp - readptr;
        readptr = cp;
        return true;
      }
      ++ccptr;
    }

    ++cp;
  }

  // end of buffer, store the remaining length:
  prevlen = cp - readptr;
  readptr = cp;
  return false;
}

bool TStrParseObj::SearchPattern(const char * checkchars)  // reads until the checkstring is found, readptr points to matching start
{
  char * cp;
  char * cps;
  char * ccstart;
  char * ccend;
  char * ccptr;
  unsigned ccslen = strlen(checkchars);

  prevptr = readptr;
  cps = readptr;
  ccstart = (char *)checkchars;
  ccend = ccstart + ccslen;

  // check start pos cycle
  while (cps < bufend - ccslen)
  {
    // check chars cycle
    cp = cps;
    ccptr = ccstart;
    char match = 1;
    while (ccptr < ccend)
    {
      if (*cp != *ccptr)
      {
        match = 0;
        break;
      }
      ++cp;
      ++ccptr;
    }

    if (match)
    {
      // does not skip the matching pattern, readptr points to the matching pattern
      prevlen = cps - readptr;
      readptr = cps;
      return true;
    }

    ++cps;
  }

  return false;
}

bool TStrParseObj::ReadToChar(char achar)
{
  char * cp = readptr;
  bool result = false;

  prevptr = readptr;

  while (cp < bufend)
  {
    if (*cp == achar)
    {
      result = true;
      break;
    }
    ++cp;
  }

  prevlen = cp - readptr;
  readptr = cp;

  return result;
}

bool TStrParseObj::CheckSymbol(const char * checkstring)
{
  char * cp = readptr;
  char * csptr = (char *)checkstring;
  char * csend = csptr + strlen(checkstring);

  while ((csptr < csend) && (cp < bufend) && (*csptr == *cp))
  {
    ++csptr;
    ++cp;
  }

  if (csptr != csend)
  {
    return false;
  }

  readptr = cp;
  return true;
}

bool TStrParseObj::ReadAlphaNum()
{
  char * cp = readptr;
  bool result = false;

  prevptr = readptr;

  while (cp < bufend)
  {
    char c = *cp;

    if (
        ((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'))
        || (c == '_')
        || ((c == '-') && (cp == readptr))  // sign: allowed only at the first character
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

  prevlen = cp - readptr;
  readptr = cp;

  return result;
}

bool TStrParseObj::ReadDecimalNumbers()
{
  char * cp = readptr;
  bool result = false;

  prevptr = readptr;

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

  prevlen = cp - readptr;
  readptr = cp;

  return result;
}

bool TStrParseObj::ReadHexNumbers()
{
  char * cp = readptr;
  bool result = false;

  prevptr = readptr;

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

  prevlen = cp - readptr;
  readptr = cp;

  return result;
}

bool TStrParseObj::ReadFloatNum()
{
  char * cp = readptr;
  bool result = false;

  prevptr = readptr;

  while (cp < bufend)
  {
    char c = *cp;

    if (
        ((c >= '0') && (c <= '9'))
        || (c == float_decimal_point) || (c == '-') || ('+' == c) || ('e' == c) || ('E' == c)
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

  prevlen = cp - readptr;
  readptr = cp;

  return result;
}

bool TStrParseObj::ReadQuotedString()
{
  if (readptr >= bufend)
  {
    return false;
  }

  if (*readptr != '"')
  {
    return false;
  }

  ++readptr;  // skip "

  ReadToChar('"'); // read to line end

  if ((readptr < bufend) && (*readptr == '"'))
  {
    ++readptr;
  }

  return true;
}

string TStrParseObj::PrevStr()
{
  string result(prevptr, prevlen);
  return result;
}

bool TStrParseObj::UCComparePrev(const char * checkstring)
{
  return PCharUCCompare(&prevptr, prevlen, checkstring);
}

int TStrParseObj::PrevToInt()
{
  return PCharToInt(prevptr, prevlen);
}

unsigned TStrParseObj::PrevHexToInt()
{
  return PCharHexToInt(prevptr, prevlen);
}

double TStrParseObj::PrevToFloat()
{
  return PCharToFloat(prevptr, prevlen);
}


int TStrParseObj::GetLineNum(const char * pos)
{
  int result = 1;
  char * cp = bufstart;

  while ((cp < pos) && (cp < bufend))
  {
    if (*cp == '\n')
    {
      ++result;
    }
    ++cp;
  }

  return result;
}

int TStrParseObj::GetLineNum()
{
  return GetLineNum(prevptr);
}

//-------------------------------------------------------------------------

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


//EOF
