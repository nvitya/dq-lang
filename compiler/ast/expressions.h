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
