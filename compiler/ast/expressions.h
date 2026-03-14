/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    expressions.h
 * authors: nvitya
 * created: 2026-02-28
 * brief:   expressions
 */

#pragma once

#include <format>
#include <vector>
#include "symbols.h"
#include "ll_defs.h"

using namespace std;

class OExprTypeConv : public OExpr
{
public:
  OExpr *    src;

  /* ctor */ OExprTypeConv(OType * dsttype, OExpr * asrc);
  LlValue *  Generate(OScope * scope) override;
};

class OIntLit : public OExpr
{
public:
  int64_t    value;

  /* ctor */ OIntLit(int64_t v);
  LlValue *  Generate(OScope * scope) override;
};

class OFloatLit : public OExpr
{
public:
  double     value;
  /* ctor */ OFloatLit(double v);
  LlValue *  Generate(OScope * scope) override;
};

class OBoolLit : public OExpr
{
public:
  bool       value;
  /* ctor */ OBoolLit(bool v);
  LlValue *  Generate(OScope * scope) override;
};

// --- LValue expression hierarchy ---
// An lvalue represents an assignable location (variable, struct member, array element, deref).
// GenerateAddress() produces the pointer to the storage location.
// Generate() (default) loads the value from that address.

class OLValueExpr : public OExpr
{
public:
  virtual LlValue * GenerateAddress(OScope * scope) = 0;
  LlValue * Generate(OScope * scope) override;  // default: load from GenerateAddress()
};

class OLValueVar : public OLValueExpr
{
public:
  OValSym *  pvalsym;
  /* ctor */ OLValueVar(OValSym * avalsym);
  LlValue *  GenerateAddress(OScope * scope) override;
  LlValue *  Generate(OScope * scope) override;
};

class OLValueDeref : public OLValueExpr
{
public:
  OExpr *  ptrexpr;
  /* ctor */ OLValueDeref(OExpr * aptr);
  LlValue *  GenerateAddress(OScope * scope) override;
};

class OLValueMember : public OLValueExpr
{
public:
  OLValueExpr *  base;
  OType *        structtype;
  uint32_t       memberindex;
  /* ctor */ OLValueMember(OLValueExpr * abase, OType * astype, uint32_t aidx, OType * amembertype);
  LlValue *  GenerateAddress(OScope * scope) override;
};

class OLValueIndex : public OLValueExpr
{
public:
  OLValueExpr *  base;
  OType *        containertype;  // array, slice, or cstring type
  OExpr *        indexexpr;
  /* ctor */ OLValueIndex(OLValueExpr * abase, OType * acontainertype, OExpr * aindex);
  LlValue *  GenerateAddress(OScope * scope) override;
};

enum EBinOp
{
  BINOP_NONE = 0,
  BINOP_ADD = 1,
  BINOP_SUB,
  BINOP_MUL,
  BINOP_DIV,
  BINOP_IDIV,
  BINOP_IMOD,

  BINOP_IAND,
  BINOP_IOR,
  BINOP_IXOR,
  BINOP_ISHL,
  BINOP_ISHR
};

string GetBinopSymbol(EBinOp op);

class OBinExpr : public OExpr
{
public:
  EBinOp       op;
  OExpr *      left;
  OExpr *      right;

  /* ctor */ OBinExpr(EBinOp aop, OExpr * aleft, OExpr * aright);
  LlValue *  Generate(OScope * scope) override;
};

enum ECompareOp
{
  COMPOP_NONE = 0,
  COMPOP_EQ,
  COMPOP_NE,
  COMPOP_LT,
  COMPOP_LE,
  COMPOP_GT,
  COMPOP_GE
};

string GetCompareSymbol(ECompareOp op);

class OCompareExpr : public OExpr
{
public:
  ECompareOp   op;
  OExpr *      left;
  OExpr *      right;
  /* ctor */   OCompareExpr(ECompareOp aop, OExpr * aleft, OExpr * aright);
  LlValue *    Generate(OScope * scope) override;
};

enum ELogicalOp
{
  LOGIOP_NONE = 0,
  LOGIOP_OR,
  LOGIOP_AND,
  LOGIOP_XOR
};
// "not" implemented as an individual expression

string GetLogiOpSymbol(ELogicalOp op);

class OLogicalExpr : public OExpr
{
public:
  ELogicalOp   op;
  OExpr *      left;
  OExpr *      right;
  /* ctor */   OLogicalExpr(ELogicalOp aop, OExpr * aleft, OExpr * aright);
  LlValue *    Generate(OScope * scope) override;
};

class ONotExpr : public OExpr
{
public:
  OExpr *    operand;
  /* ctor */ ONotExpr(OExpr * expr);
  LlValue *  Generate(OScope * scope) override;
};

class OBinNotExpr : public OExpr
{
public:
  OExpr *    operand;
  /* ctor */ OBinNotExpr(OExpr * expr);
  LlValue *  Generate(OScope * scope) override;
};

class ONegExpr : public OExpr
{
public:
  OExpr *    operand;
  /* ctor */ ONegExpr(OExpr * expr);
  LlValue *  Generate(OScope * scope) override;
};

class OAddrOfExpr : public OExpr
{
public:
  OLValueExpr *  target;
  /* ctor */ OAddrOfExpr(OLValueExpr * atarget);
  LlValue *  Generate(OScope * scope) override;
};

class ONullLit : public OExpr
{
public:
  /* ctor */ ONullLit();
  LlValue *  Generate(OScope * scope) override;
};

// Pointer subscript: p[i] computes address of element i (no dereference). Use p[i]^ to read.
// Member access is the only implicit dereference: p.field is allowed for ^compound.
class OPointerIndexExpr : public OExpr
{
public:
  OExpr *  ptrexpr;
  OExpr *  indexexpr;
  /* ctor */ OPointerIndexExpr(OExpr * aptr, OExpr * aindex);
  LlValue *  Generate(OScope * scope) override;
};

// Implicit conversion from fixed array to slice when passing to int[] parameter
class OArrayToSliceExpr : public OExpr
{
public:
  OValSym *  arrayvalsym;  // source fixed-size array variable
  /* ctor */ OArrayToSliceExpr(OValSym * aarray, OType * slicetype);
  LlValue *  Generate(OScope * scope) override;
};

// Extract length from an array slice: len(slice_var)
class OSliceLengthExpr : public OExpr
{
public:
  OValSym *  slicevalsym;
  /* ctor */ OSliceLengthExpr(OValSym * aslice);
  LlValue *  Generate(OScope * scope) override;
};

enum ERoundMode
{
  RNDMODE_ROUND,
  RNDMODE_CEIL,
  RNDMODE_FLOOR
};

string GetRoundModeName(ERoundMode mode);

class OFloatRoundExpr : public OExpr
{
public:
  ERoundMode  mode;
  OExpr *     src;
  /* ctor */  OFloatRoundExpr(ERoundMode amode, OExpr * asrc);
             ~OFloatRoundExpr();
  LlValue *   Generate(OScope * scope) override;
};

class OValSymFunc;  // forward declaration for otype_func.h

class OCallExpr : public OExpr
{
public:
  OValSymFunc *     vsfunc;
  vector<OExpr *>   args;
  /* ctor */        OCallExpr(OValSymFunc * avsfunc);
                   ~OCallExpr();
  LlValue *         Generate(OScope * scope) override;

public:
  void AddArgument(OExpr * aarg)
  {
    args.push_back(aarg);
  }
};

class OArrayLit : public OExpr
{
public:
  vector<OExpr *> elements;

  /* ctor */ OArrayLit(const vector<OExpr *> & aelements);
  LlValue *  Generate(OScope * scope) override;
};

// --- cstring expressions ---

// String literal: "hello" — type is ^cchar, creates global constant
class OCStringLit : public OExpr
{
public:
  string     value;  // resolved string content (escape-processed)

  /* ctor */ OCStringLit(const string & avalue);
  LlValue *  Generate(OScope * scope) override;
};

// sizeof() for unsized cstring parameter: extracts size from {ptr, i64} descriptor
class OCStringSizeExpr : public OExpr
{
public:
  OValSym *  cstrvalsym;
  /* ctor */ OCStringSizeExpr(OValSym * avs);
  LlValue *  Generate(OScope * scope) override;
};

// len() for cstring: runtime strlen (inline loop scanning for null terminator)
class OCStringLenExpr : public OExpr
{
public:
  OValSym *  cstrvalsym;
  /* ctor */ OCStringLenExpr(OValSym * avs);
  LlValue *  Generate(OScope * scope) override;
};

// Convert cstring[N] variable to cstring descriptor {ptr, i64}
class OCStringToDescExpr : public OExpr
{
public:
  OValSym *  cstrvalsym;
  /* ctor */ OCStringToDescExpr(OValSym * avs, OType * desctype);
  LlValue *  Generate(OScope * scope) override;
};

// Convert string literal (^cchar) to cstring descriptor {ptr, i64}
class OCStringLitToDescExpr : public OExpr
{
public:
  OExpr *    litexpr;
  uint32_t   litlen;  // buffer size: strlen + 1 (includes null terminator)
  /* ctor */ OCStringLitToDescExpr(OExpr * alit, uint32_t alen, OType * desctype);
  LlValue *  Generate(OScope * scope) override;
};
