/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_string.h
 * authors: nvitya
 * created: 2026-06-09
 * brief:   Byte-only str, rostr, and strview types
 */

#pragma once

#include "symbols.h"

class OExpr;
class OLValueExpr;

enum EStringMetaField
{
  SMF_LENGTH,
  SMF_CAPACITY,
  SMF_REFCOUNT,
  SMF_PCHAR,
  SMF_WCLEN
};

enum EStringMethod
{
  STRM_CLEAR,
  STRM_SET,
  STRM_RESERVE,
  STRM_COMPACT,
  STRM_SET_LENGTH,
  STRM_SET_CAPACITY,
  STRM_TRUNCATE,
  STRM_APPEND,
  STRM_PREPEND,
  STRM_INSERT,
  STRM_DELETE,
  STRM_CLONE,
  STRM_POP,
  STRM_POP_FIRST,
  STRM_POP_CHAR,
  STRM_POP_FIRST_CHAR,
  STRM_ADDFMT,
  STRM_TO_WCHARS
};

class OTypeString : public OType
{
private:
  using super = OType;

public:
  OTypeString(const string aname, ETypeKind akind)
  :
    super(aname, akind)
  {
  }

  virtual LlValue * GenerateLength(OScope * scope, LlValue * straddr) = 0;
  virtual LlValue * GeneratePChar(OScope * scope, LlValue * straddr) = 0;
  virtual LlValue * GenerateGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index) = 0;
  virtual LlValue * GenerateCharAddress(OScope * scope, OLValueExpr * receiver, LlValue * index);
  virtual LlValue * GenerateSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                                  OExpr * end_expr, bool end_inclusive) = 0;
  virtual LlValue * GenerateMetaField(OScope * scope, OLValueExpr * receiver, EStringMetaField field);

  LlValue * GenerateWcLen(OScope * scope, OLValueExpr * receiver);
  LlValue * GenerateWCharAt(OScope * scope, OLValueExpr * receiver, OExpr * index);
  LlValue * GenerateWCharSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                               OExpr * end_expr, bool end_inclusive);
  LlValue * GenerateToWchars(OScope * scope, OLValueExpr * receiver);

  static LlValue * GenerateEqual(OScope * scope, OExpr * left, OExpr * right);
};

class OTypeDynString : public OTypeString
{
private:
  using super = OTypeString;

public:
  OTypeDynString()
  :
    super("str", TK_DYNSTR)
  {
    bytesize = TARGET_PTRSIZE;
    alignsize = TARGET_PTRSIZE;
  }

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
  bool GenerateAssignment(OScope * scope, LlValue * targetaddr, OExpr * value, bool volatile_store = false) override;

  LlValue * GenerateLength(OScope * scope, LlValue * straddr) override;
  LlValue * GeneratePChar(OScope * scope, LlValue * straddr) override;
  LlValue * GenerateGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index) override;
  LlValue * GenerateSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                          OExpr * end_expr, bool end_inclusive) override;
  LlValue * GenerateMetaField(OScope * scope, OLValueExpr * receiver, EStringMetaField field) override;

  LlValue * GenerateCapacity(OScope * scope, LlValue * straddr);
  LlValue * GenerateRefCount(OScope * scope, LlValue * straddr);
  void GenerateCreate(OScope * scope, LlValue * straddr);
  void GenerateIncRef(OScope * scope, LlValue * straddr);
  void GenerateDestroy(OScope * scope, LlValue * straddr);
  bool GenerateAssignExpr(OScope * scope, LlValue * targetaddr, OExpr * value);
  void GenerateSetChar(OScope * scope, OLValueExpr * receiver, OExpr * index, OExpr * value);
  LlValue * GenerateMethodCall(OScope * scope, OLValueExpr * receiver, EStringMethod method,
                               const vector<OExpr *> & args);

  static LlValue * GenerateConcat(OScope * scope, OExpr * left, OExpr * right);
  static LlValue * GenerateConcatFromStringValue(OScope * scope, LlValue * leftvalue, OExpr * right);
};

class OTypeStrView : public OTypeString
{
private:
  using super = OTypeString;

public:
  OTypeStrView()
  :
    super("strview", TK_STRVIEW)
  {
    bytesize = TARGET_PTRSIZE + 8;
    alignsize = TARGET_PTRSIZE;
  }

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;

  LlValue * GenerateLength(OScope * scope, LlValue * straddr) override;
  LlValue * GeneratePChar(OScope * scope, LlValue * straddr) override;
  LlValue * GenerateGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index) override;
  LlValue * GenerateSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                          OExpr * end_expr, bool end_inclusive) override;
};

// Literal-backed rostr constants reuse relocatable C string literal storage.
class OValueRoStr : public OValue
{
public:
  OValuePointer literal;
  OValueRoStr(OType * atype);
  LlConst * CreateLlConst() override;
  bool CalculateConstant(OExpr * expr, bool emit_errors = true) override;
  bool WriteDqmIfValue(ODqmIfWriter & writer) override;
};

class OTypeRoStr : public OTypeString
{
public:
  OTypeRoStr() : OTypeString("rostr", TK_ROSTR)
  {
    alignsize = TARGET_PTRSIZE;
    bytesize = AlignUpU32(TARGET_PTRSIZE + 4, alignsize);
  }

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  OValue * CreateValue() override { return new OValueRoStr(this); }
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
  LlValue * GenerateLength(OScope * scope, LlValue * straddr) override;
  LlValue * GeneratePChar(OScope * scope, LlValue * straddr) override;
  LlValue * GenerateGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index) override;
  LlValue * GenerateSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                          OExpr * end_expr, bool end_inclusive) override;

  LlValue * GenerateBorrow(OScope * scope, OExpr * source);
  LlValue * GenerateTextInfo(OScope * scope, OExpr * source);
  LlValue * ExtractPChar(LlValue * value);
};

inline bool IsTextSourceType(OType * type) { return type && type->IsTextSource(); }
inline bool IsStringComparableTextType(OType * type) { return type && type->IsStringComparable(); }
inline bool IsStringFamilyTextType(OType * type) { return type && type->IsStringFamily(); }

bool EnsureDynStringRtlUseForStringTypes();
OValSymFunc * TextFormatFunc(const string & name);
LlValue * CallTextFormatFunc(OScope * scope, const string & name, vector<LlValue *> args = {});
LlValue * GenerateTextInfoAddress(OScope * scope, OExpr * expr);
LlValue * GenerateTextInfoValue(OScope * scope, OExpr * expr);

void GenerateStringCreate(OScope * scope, LlValue * straddr);
void GenerateStringIncRef(OScope * scope, LlValue * straddr);
void GenerateStringDestroy(OScope * scope, LlValue * straddr);
bool GenerateStringAssignExpr(OScope * scope, LlValue * targetaddr, OExpr * value);

LlValue * GenerateStringLength(OScope * scope, OType * strtype, LlValue * straddr);
LlValue * GenerateStringPChar(OScope * scope, OType * strtype, LlValue * straddr);
LlValue * GenerateStringCapacity(OScope * scope, OType * strtype, LlValue * straddr);
LlValue * GenerateStringRefCount(OScope * scope, OType * strtype, LlValue * straddr);
LlValue * GenerateStringEqual(OScope * scope, OExpr * left, OExpr * right);
LlValue * GenerateStringConcat(OScope * scope, OExpr * left, OExpr * right);
LlValue * GenerateStringConcatFromStringValue(OScope * scope, LlValue * leftvalue, OExpr * right);
LlValue * GenerateStringGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index);
LlValue * GenerateStringCharAddress(OScope * scope, OLValueExpr * receiver, LlValue * index);
void GenerateStringSetChar(OScope * scope, OLValueExpr * receiver, OExpr * index, OExpr * value);
LlValue * GenerateStringSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                              OExpr * end_expr, bool end_inclusive);
LlValue * GenerateStringWcLen(OScope * scope, OLValueExpr * receiver);
LlValue * GenerateStringWCharAt(OScope * scope, OLValueExpr * receiver, OExpr * index);
LlValue * GenerateStringWCharSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                                   OExpr * end_expr, bool end_inclusive);
LlValue * GenerateStringToWchars(OScope * scope, OLValueExpr * receiver);
LlValue * GenerateStringMethodCall(OScope * scope, OLValueExpr * receiver, EStringMethod method,
                                   const vector<OExpr *> & args);
