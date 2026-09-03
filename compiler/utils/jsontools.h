/* -----------------------------------------------------------------------------
 * This file is originated here: https://github.com/nvitya/cpputils
 * Copyright (c) 2026 Viktor Nagy, nvitya
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:

 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * --------------------------------------------------------------------------- */
/*
 *  file:     jsontools.cpp
 *  created:  2026-01-06
 *  authors:  nvitya, ChatGPT-5.2
 *  brief:    Simple, fast and well designed JSON Parser and Writer.
 *            This library is created from the Pascal JsonTools here: https://github.com/nvitya/JsonTools
 *            The original Pascal version is translated by ChatGPT-5.2, with more iteration and hinting.
*/

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <iosfwd>

using namespace std;

// Pascal compatibility
class EJsonException : public runtime_error {
public:
  explicit EJsonException(const string& msg);
};

enum TJsonNodeKind {
  nkObject,
  nkArray,
  nkBool,
  nkNull,
  nkNumber,
  nkString
};

class TJsonNode {
public:
  TJsonNode();
  explicit TJsonNode(TJsonNodeKind k);

  /* Pascal property replacements */

  TJsonNode*        GetRoot();
  const TJsonNode*  GetRoot() const;

  TJsonNode*        GetParent();
  const TJsonNode*  GetParent() const;

  TJsonNodeKind     GetKind() const;
  void              SetKind(TJsonNodeKind k);

  string            GetName() const;
  void              SetName(const string& name);

  // Pascal Value: write triggers Parse
  string            GetValue() const;
  void              SetValue(const string& json);

  int               GetCount() const;

  string            GetAsJson(bool compact = false,
                              const string& indent = "  ") const;
  void              SetAsJson(const string& json);

  TJsonNode&        GetAsArray();
  TJsonNode&        GetAsObject();
  TJsonNode&        GetAsNull();

  bool              GetAsBoolean() const;
  void              SetAsBoolean(bool v);

  string            GetAsString() const;
  void              SetAsString(const string& v);

  double            GetAsNumber() const;
  void              SetAsNumber(double v);

  /* IO */

  void LoadFromStream(istream& is);
  void SaveToStream(ostream& os, bool compact = false) const;

  void LoadFromFile(const string& fileName);
  void SaveToFile(const string& fileName, bool compact = false) const;

  /* Parsing */

  void Parse(const string& json);
  bool TryParse(const string& json) noexcept;

  /* Building */

  TJsonNode& Add(const string& name);
  TJsonNode& Add(const string& name, const char * astr);
  TJsonNode& Add(const string& name, int v);
  TJsonNode& Add(const string& name, long v);
  TJsonNode& Add(const string& name, long long v);
  TJsonNode& Add(const string& name, bool b);
  TJsonNode& Add(const string& name, double d);
  TJsonNode& Add(const string& name, const string& s);

  TJsonNode& Add(); // array item

  /* Deletion */

  void Delete(int index);
  void Delete(const string& name);
  void Clear();

  /* Access */

  TJsonNode&        Child(int index);
  const TJsonNode&  Child(int index) const;

  TJsonNode*        Child(const string& name);
  const TJsonNode*  Child(const string& name) const;

  /* Path navigation */

  bool              Exists(const string& path) const;

  TJsonNode*        Find(const string& path);
  const TJsonNode*  Find(const string& path) const;

  bool              Find(const string& path, TJsonNode*& out);
  bool              Find(const string& path, const TJsonNode*& out) const;

  // Create intermediate nodes as needed (object keys, array items).
  // Missing array items are appended as nkNull.
  TJsonNode&        Force(const string& path);

private:
  TJsonNodeKind FKind = nkNull;
  string        FName;
  string        FValue;   // for primitives: string content / number lexeme / "true"/"false"
  TJsonNode*    FParent = nullptr;

  vector<unique_ptr<TJsonNode>> FChildren;

private:
  /* Internal helpers */

  TJsonNode& AddInternal(TJsonNodeKind kind, const string& name, const string& value);

  void RequireCollection() const;

  void ParseValueIntoThis(const char*& p);

  string FormatPretty(const string& pad, const string& indent) const;
  string FormatCompact() const;
};
