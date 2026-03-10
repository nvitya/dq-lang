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

class OVarRef : public OExpr
{
public:
  OValSym *  pvalsym;
  /* ctor */ OVarRef(OValSym * avalsym);
  LlValue *  Generate(OScope * scope) override;
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
  OValSym *  pvalsym;
  /* ctor */ OAddrOfExpr(OValSym * avalsym);
  LlValue *  Generate(OScope * scope) override;
};

// Address of an array element: &arr[index]
class OAddrOfArrayElemExpr : public OExpr
{
public:
  OValSym *  arrayvalsym;
  OExpr *    indexexpr;
  /* ctor */ OAddrOfArrayElemExpr(OValSym * aarray, OExpr * aindex);
  LlValue *  Generate(OScope * scope) override;
};

class ODerefExpr : public OExpr
{
public:
  OExpr *    operand;
  /* ctor */ ODerefExpr(OExpr * aoperand);
  LlValue *  Generate(OScope * scope) override;
};

class ONullLit : public OExpr
{
public:
  /* ctor */ ONullLit();
  LlValue *  Generate(OScope * scope) override;
};

// Array element access: arr[index]
class OArrayIndexExpr : public OExpr
{
public:
  OValSym *  arrayvalsym;  // the array variable (need alloca address for GEP)
  OExpr *    indexexpr;
  /* ctor */ OArrayIndexExpr(OValSym * aarray, OExpr * aindex);
  LlValue *  Generate(OScope * scope) override;
};

// Pointer subscript: p[i] computes address of element i (no dereference). Use p[i]^ to read.
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

// Address of cstring element: &s[index] — handles both sized and unsized
class OCStringElemAddrExpr : public OExpr
{
public:
  OValSym *  cstrvalsym;
  OExpr *    indexexpr;
  /* ctor */ OCStringElemAddrExpr(OValSym * avs, OExpr * aindex);
  LlValue *  Generate(OScope * scope) override;
};

// --- struct member expressions ---

// Read a member of a struct variable: sm.id
class OStructMemberExpr : public OExpr
{
public:
  OValSym *  structvalsym;
  uint32_t   memberindex;
  /* ctor */ OStructMemberExpr(OValSym * astruct, uint32_t aidx, OType * amembertype);
  LlValue *  Generate(OScope * scope) override;
};

// Read a member of a struct through pointer dereference: ep^.id
class ODerefMemberExpr : public OExpr
{
public:
  OExpr *    ptrexpr;      // expression that yields the pointer
  OType *    structtype;   // the compound type being pointed to
  uint32_t   memberindex;
  /* ctor */ ODerefMemberExpr(OExpr * aptr, OType * astructtype, uint32_t aidx, OType * amembertype);
  LlValue *  Generate(OScope * scope) override;
};

// Read an array element within a struct member: sm.member[i]
class OStructMemberArrayIndexExpr : public OExpr
{
public:
  OValSym *  structvalsym;
  uint32_t   memberindex;
  OType *    arraytype;    // the array member type
  OExpr *    indexexpr;
  /* ctor */ OStructMemberArrayIndexExpr(OValSym * astruct, uint32_t aidx, OType * aarrtype, OExpr * aindex);
  LlValue *  Generate(OScope * scope) override;
};

// Read an array element within a struct member through pointer: ep^.member[i]
class ODerefMemberArrayIndexExpr : public OExpr
{
public:
  OExpr *    ptrexpr;
  OType *    structtype;
  uint32_t   memberindex;
  OType *    arraytype;
  OExpr *    indexexpr;
  /* ctor */ ODerefMemberArrayIndexExpr(OExpr * aptr, OType * astructtype, uint32_t aidx, OType * aarrtype, OExpr * aindex);
  LlValue *  Generate(OScope * scope) override;
};

// Address of struct member array element: &sm.elem[i]
class OAddrOfStructMemberArrayElemExpr : public OExpr
{
public:
  OValSym *  structvalsym;
  uint32_t   memberindex;
  OType *    arraytype;
  OExpr *    indexexpr;
  /* ctor */ OAddrOfStructMemberArrayElemExpr(OValSym * astruct, uint32_t aidx, OType * aarrtype, OExpr * aindex);
  LlValue *  Generate(OScope * scope) override;
};
