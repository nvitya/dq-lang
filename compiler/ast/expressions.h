/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
#include "otype_cstring.h"
#include "otype_string.h"
#include "otype_anyvalue.h"

using namespace std;

void EmitExpressionExceptionCheck(OScope * scope);
LlValue * GenerateFunctionCall(OScope * scope, OValSymFunc * vsfunc,
                               const vector<LlValue *> & ll_args, bool force_direct = false);

enum EBinOp : int;

class OExprTypeConv : public OExpr
{
public:
  OExpr *    src;

  /* ctor */ OExprTypeConv(OType * dsttype, OExpr * asrc);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  bool       TryFoldSelf(OExpr ** rreplacement) override;
  void       DeleteChildTree() override;
};

class OIntLit : public OExpr
{
public:
  int64_t    value;

  /* ctor */ OIntLit(int64_t v, OType * atype = nullptr);
  LlValue *  Generate(OScope * scope) override;
};

class OFloatLit : public OExpr
{
public:
  double     value;
  /* ctor */ OFloatLit(double v, OType * atype = nullptr);
  LlValue *  Generate(OScope * scope) override;
};

class OBoolLit : public OExpr
{
public:
  bool       value;
  /* ctor */ OBoolLit(bool v, OType * atype = nullptr);
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
  virtual bool IsObjectReferenceExpr() const { return false; }
  virtual bool IsFixedObjectStorageExpr() const { return false; }
  virtual bool RequiresVolatileMemoryAccess() const { return false; }
  virtual LlValue * GenerateObjectAddress(OScope * scope);
};

class OLValueVar : public OLValueExpr
{
public:
  OValSym *  pvalsym;
  /* ctor */ OLValueVar(OValSym * avalsym);
  LlValue *  GenerateAddress(OScope * scope) override;
  LlValue *  Generate(OScope * scope) override;
  bool       IsObjectReferenceExpr() const override;
  bool       IsFixedObjectStorageExpr() const override;
  LlValue *  GenerateObjectAddress(OScope * scope) override;
};

class OLValueDeref : public OLValueExpr
{
public:
  OExpr *  ptrexpr;
  /* ctor */ OLValueDeref(OExpr * aptr);
  LlValue *  GenerateAddress(OScope * scope) override;
  LlValue *  Generate(OScope * scope) override;
  bool       IsObjectReferenceExpr() const override;
  bool       RequiresVolatileMemoryAccess() const override { return true; }
  LlValue *  GenerateObjectAddress(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

class OLValueMember : public OLValueExpr
{
public:
  OLValueExpr *  base;
  OType *        structtype;
  uint32_t       memberindex;
  /* ctor */ OLValueMember(OLValueExpr * abase, OType * astype, uint32_t aidx, OType * amembertype);
  LlValue *  GenerateAddress(OScope * scope) override;
  LlValue *  Generate(OScope * scope) override;
  bool       IsObjectReferenceExpr() const override;
  bool       IsFixedObjectStorageExpr() const override;
  LlValue *  GenerateObjectAddress(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

class OLValueIndex : public OLValueExpr
{
public:
  OLValueExpr *  base;
  OType *        containertype;  // array, slice, dynamic array, or cstring type
  OExpr *        indexexpr;
  /* ctor */ OLValueIndex(OLValueExpr * abase, OType * acontainertype, OExpr * aindex);
  LlValue *  GenerateAddress(OScope * scope) override;
  LlValue *  Generate(OScope * scope) override;
  bool       IsObjectReferenceExpr() const override;
  LlValue *  GenerateObjectAddress(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

// A property can be an assignment target, but it is never addressable storage.
class OPropertyExpr : public OLValueExpr
{
public:
  OExpr *            receiver;
  OValSymProperty *  property;
  vector<OExpr *>    indices;

  /* ctor */ OPropertyExpr(OExpr * areceiver, OValSymProperty * aproperty);
  LlValue * GenerateAddress(OScope * scope) override;
  LlValue * Generate(OScope * scope) override;
  bool IsObjectReferenceExpr() const override;
  LlValue * GenerateObjectAddress(OScope * scope) override;
  void GenerateWrite(OScope * scope, OExpr * value);
  void GenerateModifyWrite(OScope * scope, EBinOp op, OExpr * value);
  void FoldChildren() override;
  void DeleteChildTree() override;
};

class OArraySliceExpr : public OExpr
{
public:
  OLValueExpr *  base;
  OType *        containertype;
  OExpr *        startexpr = nullptr;
  OExpr *        endexpr = nullptr;
  bool           end_inclusive = false;

  /* ctor */ OArraySliceExpr(OLValueExpr * abase, OType * acontainertype, OExpr * astart, OExpr * aend,
                             bool aend_inclusive = false);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

class OStringSliceExpr : public OExpr
{
public:
  OLValueExpr *  base;
  OExpr *        startexpr = nullptr;
  OExpr *        endexpr = nullptr;
  bool           end_inclusive = false;

  /* ctor */ OStringSliceExpr(OLValueExpr * abase, OExpr * astart, OExpr * aend,
                              bool aend_inclusive = false);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

class OStringWCharIndexExpr : public OExpr
{
public:
  OLValueExpr *  base;
  OExpr *        indexexpr = nullptr;

  /* ctor */ OStringWCharIndexExpr(OLValueExpr * abase, OExpr * aindex);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

class OStringWCharSliceExpr : public OExpr
{
public:
  OLValueExpr *  base;
  OExpr *        startexpr = nullptr;
  OExpr *        endexpr = nullptr;
  bool           end_inclusive = false;

  /* ctor */ OStringWCharSliceExpr(OLValueExpr * abase, OExpr * astart, OExpr * aend,
                                   bool aend_inclusive = false);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

enum EBinOp : int
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
  void       FoldChildren() override;
  bool       TryFoldSelf(OExpr ** rreplacement) override;
  void       DeleteChildTree() override;
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
  void         FoldChildren() override;
  bool         TryFoldSelf(OExpr ** rreplacement) override;
  void         DeleteChildTree() override;
};

class OIifExpr : public OExpr
{
public:
  OExpr *      condition;
  OExpr *      true_expr;
  OExpr *      false_expr;

  /* ctor */   OIifExpr(OExpr * acond, OExpr * atrue, OExpr * afalse, OType * aresult_type);
               ~OIifExpr() override;
  LlValue *    Generate(OScope * scope) override;
  void         FoldChildren() override;
  bool         TryFoldSelf(OExpr ** rreplacement) override;
  void         DeleteChildTree() override;
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
  void         FoldChildren() override;
  bool         TryFoldSelf(OExpr ** rreplacement) override;
  void         DeleteChildTree() override;
};

class ONotExpr : public OExpr
{
public:
  OExpr *    operand;
  /* ctor */ ONotExpr(OExpr * expr);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  bool       TryFoldSelf(OExpr ** rreplacement) override;
  void       DeleteChildTree() override;
};

class OBinNotExpr : public OExpr
{
public:
  OExpr *    operand;
  /* ctor */ OBinNotExpr(OExpr * expr);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  bool       TryFoldSelf(OExpr ** rreplacement) override;
  void       DeleteChildTree() override;
};

class ONegExpr : public OExpr
{
public:
  OExpr *    operand;
  /* ctor */ ONegExpr(OExpr * expr);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  bool       TryFoldSelf(OExpr ** rreplacement) override;
  void       DeleteChildTree() override;
};

class OAddrOfExpr : public OExpr
{
public:
  OLValueExpr *  target;
  /* ctor */ OAddrOfExpr(OLValueExpr * atarget);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

class OObjectAddrExpr : public OExpr
{
public:
  OLValueExpr * target;
  /* ctor */ OObjectAddrExpr(OLValueExpr * atarget);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

class OObjectUpcastExpr : public OExpr
{
public:
  OExpr * src;
  OType * dst_object_type;
  /* ctor */ OObjectUpcastExpr(OType * adsttype, OExpr * asrc);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

class ONullLit : public OExpr
{
public:
  /* ctor */ ONullLit();
  LlValue *  Generate(OScope * scope) override;
};

class OObjectTypeLiteralExpr : public OExpr
{
public:
  OTypeObject * object_type = nullptr;

  /* ctor */ OObjectTypeLiteralExpr(OTypeObject * aobject_type);
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
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

class OValSymFunc;
class OValSymOverloadSet;
class OArrayLit;

class ONewExpr : public OExpr
{
public:
  OType *        alloc_type;
  OExpr *        initexpr;
  OValSymFunc *  ctor_func = nullptr;
  vector<OExpr *> ctor_args;
  OValSymFunc *  memalloc_func;

  /* ctor */ ONewExpr(OType * aalloc_type, OExpr * ainitexpr, OValSymFunc * amemalloc_func);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

class ODynamicNewObjectExpr : public OExpr
{
public:
  OExpr *        type_expr = nullptr;
  OTypeObject *  base_object_type = nullptr;
  OValSymFunc *  ctor_func = nullptr;
  int            ctor_slot = -1;
  vector<OExpr *> ctor_args;
  OValSymFunc *  memalloc_func = nullptr;

  /* ctor */ ODynamicNewObjectExpr(OExpr * atype_expr, OTypeObject * abase_object_type,
                                   OValSymFunc * amemalloc_func);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

// Implicit conversion from fixed array lvalue to slice when passing/storing []int.
class OArrayToSliceExpr : public OExpr
{
public:
  OLValueExpr *  arrayexpr;  // source fixed-size array lvalue
  /* ctor */ OArrayToSliceExpr(OLValueExpr * aarray, OType * slicetype);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

// Convert an array literal to a call-lifetime slice temporary.
class OArrayLitToSliceExpr : public OExpr
{
public:
  OArrayLit *  arraylit;
  /* ctor */ OArrayLitToSliceExpr(OArrayLit * alit, OType * slicetype);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

// Convert an array literal to a dynamic array temporary.
class OArrayLitToDynArrayExpr : public OExpr
{
public:
  OArrayLit *  arraylit;
  /* ctor */ OArrayLitToDynArrayExpr(OArrayLit * alit, OType * dyntype);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

// Convert a fixed array lvalue to a dynamic array temporary.
class OArrayToDynArrayExpr : public OExpr
{
public:
  OLValueExpr *  arrayexpr;
  /* ctor */ OArrayToDynArrayExpr(OLValueExpr * aarray, OType * dyntype);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

// Convert an array slice to a dynamic array temporary.
class OSliceToDynArrayExpr : public OExpr
{
public:
  OExpr *  sliceexpr;
  /* ctor */ OSliceToDynArrayExpr(OExpr * aslice, OType * dyntype);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

// Implicit conversion from dynamic array manager reference to a full slice.
class ODynArrayToSliceExpr : public OExpr
{
public:
  OLValueExpr *  arrayexpr;
  /* ctor */ ODynArrayToSliceExpr(OLValueExpr * aarray, OType * slicetype);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

enum EArrayMetaField
{
  AMF_LENGTH,
  AMF_CAPACITY,
  AMF_REFCOUNT
};

// Extract .length / .capacity from array-like lvalues.
class OArrayMetaFieldExpr : public OExpr
{
public:
  OLValueExpr *    target;
  OType *          containertype;
  EArrayMetaField  field;
  /* ctor */ OArrayMetaFieldExpr(OLValueExpr * atarget, OType * acontainertype, EArrayMetaField afield);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

// Extract length from an array slice: len(slice_var)
class OSliceLengthExpr : public OExpr
{
public:
  OValSym *  slicevalsym;
  /* ctor */ OSliceLengthExpr(OValSym * aslice);
  LlValue *  Generate(OScope * scope) override;
};

class ODynArrayLengthExpr : public OExpr
{
public:
  OValSym *  dynvalsym;
  /* ctor */ ODynArrayLengthExpr(OValSym * adyn);
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
  void        FoldChildren() override;
  bool        TryFoldSelf(OExpr ** rreplacement) override;
  void        DeleteChildTree() override;
};

class OTypeFuncRef;
class OTypeFunc;

class OCallExpr : public OExpr
{
public:
  OValSymFunc *     vsfunc;
  vector<OExpr *>   args;
  bool              force_direct = false;
  /* ctor */        OCallExpr(OValSymFunc * avsfunc);
                   ~OCallExpr();
  LlValue *         Generate(OScope * scope) override;
  void              FoldChildren() override;
  void              DeleteChildTree() override;

public:
  void AddArgument(OExpr * aarg)
  {
    args.push_back(aarg);
  }
};

enum EDynArrayMethod
{
  DYNM_CLEAR,
  DYNM_RESERVE,
  DYNM_COMPACT,
  DYNM_SET_LENGTH,
  DYNM_SET_CAPACITY,
  DYNM_APPEND,
  DYNM_APPEND_SLICE,
  DYNM_PREPEND,
  DYNM_PREPEND_SLICE,
  DYNM_INSERT,
  DYNM_INSERT_SLICE,
  DYNM_DELETE,
  DYNM_CLONE,
  DYNM_POP,
  DYNM_POP_FIRST
};

class ODynArrayMethodCallExpr : public OExpr
{
public:
  EDynArrayMethod   method;
  OLValueExpr *     receiver;
  vector<OExpr *>   args;

  /* ctor */ ODynArrayMethodCallExpr(EDynArrayMethod amethod, OLValueExpr * areceiver, OType * arettype);
  ~ODynArrayMethodCallExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

class OFuncRefExpr : public OExpr
{
public:
  OValSymFunc *   vsfunc;
  /* ctor */      OFuncRefExpr(OValSymFunc * avsfunc, OType * atype);
  LlValue *       Generate(OScope * scope) override;
};

class OBoundMethodExpr : public OExpr
{
public:
  OValSymFunc *   vsfunc;
  OLValueExpr *   receiver;

  /* ctor */      OBoundMethodExpr(OValSymFunc * avsfunc, OLValueExpr * areceiver);
                 ~OBoundMethodExpr() override = default;
  LlValue *       Generate(OScope * scope) override;
  void            FoldChildren() override;
  void            DeleteChildTree() override;
};

class OBoundMethodOverloadExpr : public OExpr
{
public:
  OValSymOverloadSet * ovset;
  OValSymFunc *        matched_func = nullptr;
  OLValueExpr *        receiver;

  /* ctor */      OBoundMethodOverloadExpr(OValSymOverloadSet * aovset, OLValueExpr * areceiver);
                 ~OBoundMethodOverloadExpr() override = default;
  LlValue *       Generate(OScope * scope) override;
  void            FoldChildren() override;
  void            DeleteChildTree() override;
};

class OIndirectCallExpr : public OExpr
{
public:
  OExpr *         callee;
  OTypeFunc *     sigtype;
  bool            object_ref = false;
  vector<OExpr *> args;

  /* ctor */      OIndirectCallExpr(OExpr * acallee, OTypeFuncRef * acalltype);
                  ~OIndirectCallExpr() override;
  LlValue *       Generate(OScope * scope) override;
  void            FoldChildren() override;
  void            DeleteChildTree() override;

public:
  void AddArgument(OExpr * aarg)
  {
    args.push_back(aarg);
  }
};

class OTypeNameExpr : public OExpr
{
public:
  OExpr *         expr;

  /* ctor */      OTypeNameExpr(OExpr * aexpr);
                  ~OTypeNameExpr() override = default;
  LlValue *       Generate(OScope * scope) override;
  void            FoldChildren() override;
  void            DeleteChildTree() override;
};

class OArrayLit : public OExpr
{
public:
  vector<OExpr *> elements;

  /* ctor */ OArrayLit(const vector<OExpr *> & aelements);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

// --- anyvalue expressions ---

class OAnyValueBoxExpr : public OExpr
{
public:
  OExpr * source;

  /* ctor */ OAnyValueBoxExpr(OExpr * asource, OType * atype);
  ~OAnyValueBoxExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

class OAnyValueMethodCallExpr : public OExpr
{
public:
  OLValueExpr *      receiver;
  EAnyValueMethod    method;
  vector<OExpr *>    args;

  /* ctor */ OAnyValueMethodCallExpr(OLValueExpr * areceiver, EAnyValueMethod amethod, OType * arettype = nullptr);
  ~OAnyValueMethodCallExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

// Parser recovery placeholder for erroneous call-like member expressions.
class OInvalidCallExpr : public OExpr
{
public:
  /* ctor */ OInvalidCallExpr();
  LlValue *  Generate(OScope * scope) override;
};

// --- cstring expressions ---

// String literal: "hello" — type is ^char, creates global constant
class OCStringLit : public OExpr
{
public:
  string     value;  // resolved string content (escape-processed)

  /* ctor */ OCStringLit(const string & avalue);
  LlValue *  Generate(OScope * scope) override;
};

// Convert a char/char literal to a temporary zero-terminated C string pointer.
class OCharLitToCStringPtrExpr : public OExpr
{
public:
  uint8_t value;

  /* ctor */ OCharLitToCStringPtrExpr(uint8_t avalue);
  LlValue * Generate(OScope * scope) override;
};

// sizeof() for unsized cstring parameter: reads SDqTextInfo-compatible descriptor metadata
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

class OCStringMetaFieldExpr : public OExpr
{
public:
  OLValueExpr *     receiver;
  ECStringMetaField field;

  /* ctor */ OCStringMetaFieldExpr(OLValueExpr * areceiver, ECStringMetaField afield);
  ~OCStringMetaFieldExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

class OCStringMethodCallExpr : public OExpr
{
public:
  OLValueExpr *    receiver;
  ECStringMethod   method;
  vector<OExpr *>  args;

  /* ctor */ OCStringMethodCallExpr(OLValueExpr * areceiver, ECStringMethod amethod);
  ~OCStringMethodCallExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

// Convert cstring(N) variable to SDqTextInfo-compatible cstring descriptor.
class OCStringToDescExpr : public OExpr
{
public:
  OValSym *  cstrvalsym;
  /* ctor */ OCStringToDescExpr(OValSym * avs, OType * desctype);
  LlValue *  Generate(OScope * scope) override;
};

class OCStringLValueToDescExpr : public OExpr
{
public:
  OLValueExpr * cstrlval;

  /* ctor */ OCStringLValueToDescExpr(OLValueExpr * alval, OType * desctype);
  ~OCStringLValueToDescExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

// Convert a ^char expression to an SDqTextInfo-compatible cstring descriptor.
// String literals carry a known length; other pointers scan lazily at runtime.
class OCStringLitToDescExpr : public OExpr
{
public:
  OExpr *    litexpr;
  uint32_t   litlen;  // known buffer size: strlen + 1, or zero for unknown ^char
  /* ctor */ OCStringLitToDescExpr(OExpr * alit, uint32_t alen, OType * desctype);
  LlValue *  Generate(OScope * scope) override;
  void       FoldChildren() override;
  void       DeleteChildTree() override;
};

// --- str / strview expressions ---

class OTextSourceToViewExpr : public OExpr
{
public:
  OExpr * source;

  /* ctor */ OTextSourceToViewExpr(OExpr * asource, OType * atype);
  ~OTextSourceToViewExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

class OTextSourceToStringExpr : public OExpr
{
public:
  OExpr * source;

  /* ctor */ OTextSourceToStringExpr(OExpr * asource, OType * atype);
  ~OTextSourceToStringExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

class OStringMetaFieldExpr : public OExpr
{
public:
  OLValueExpr *     receiver;
  EStringMetaField  field;

  /* ctor */ OStringMetaFieldExpr(OLValueExpr * areceiver, EStringMetaField afield);
  ~OStringMetaFieldExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

class OStringMethodCallExpr : public OExpr
{
public:
  OLValueExpr *    receiver;
  EStringMethod    method;
  vector<OExpr *>  args;

  /* ctor */ OStringMethodCallExpr(OLValueExpr * areceiver, EStringMethod amethod, OType * arettype = nullptr);
  ~OStringMethodCallExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

// --- type casting expressions ---

class OTryCastExpr : public OExpr
{
public:
  OType *   target_type;
  OExpr *   source_expr;

  /* ctor */ OTryCastExpr(OType * atarget_type, OExpr * asource_expr);
  ~OTryCastExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};

class OIsExpr : public OExpr
{
public:
  OExpr *   source_expr;
  OType *   target_type;

  /* ctor */ OIsExpr(OExpr * asource_expr, OType * atarget_type);
  ~OIsExpr() override = default;
  LlValue * Generate(OScope * scope) override;
  void      FoldChildren() override;
  void      DeleteChildTree() override;
};
