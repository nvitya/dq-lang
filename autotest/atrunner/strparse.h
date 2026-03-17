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
 *  file:     strparse.h
 *  brief:    light string parser using pointers, avoiding copiing
 *  created:  2023-05-31
 *  authors:  nvitya
*/

#ifndef __STRPARSE_H_
#define __STRPARSE_H_

#include <string>

using namespace std;

class TStrParseObj
{
  public:
    char * bufstart;
    char * bufend;

    char * readptr;    // current parsing position

    char * prevptr;    // usually signs token start
    int prevlen;       // usually signs token length

    char commentmarker = '#';
    char float_decimal_point = '.';

    TStrParseObj();
    TStrParseObj(char * astr, int buflen = 0);

    void Init(char * astr, int buflen = 0);

    void SkipSpaces(bool askiplineend = true);
    void SkipWhite();

    bool ReadLine();                 // sets prevptr, prevlen
    bool ReadTo(const char * checkchars);  // sets prevptr, prevlen
    bool ReadToChar(char achar);     // sets prevptr, prevlen
    bool ReadAlphaNum();             // sets prevptr, prevlen
    bool ReadDecimalNumbers();       // sets prevptr, prevlen
    bool ReadHexNumbers();           // sets prevptr, prevlen
    bool ReadFloatNum();             // sets prevptr, prevlen
    bool ReadQuotedString();
    bool CheckSymbol(const char * checkstring);
    bool SearchPattern(const char * checkchars);  // sets prevptr, prevlen

    string PrevStr();
    bool UCComparePrev(const char * checkstring);
    int PrevToInt();
    unsigned PrevHexToInt();
    double PrevToFloat();


    int GetLineNum(const char * pos);
    int GetLineNum();
};

typedef TStrParseObj *  PStrParseObj;

bool PCharUCCompare(char * * ReadPtr, int len, const char * checkstring);
int PCharToInt(char * ReadPtr, int len);
unsigned PCharHexToInt(char * ReadPtr, int len);
double PCharToFloat(char * ReadPtr, int len);

#endif
