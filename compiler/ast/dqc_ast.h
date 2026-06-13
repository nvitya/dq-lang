/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_ast.h
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#pragma once

#include "stdint.h"
#include "otype_compound.h"
#include <string>
#include <vector>
#include "comp_options.h"
#include "expressions.h"
#include "symbols.h"
#include "dq_module.h"

#include "dqc_base.h"

using namespace std;

class OValSymOverloadSet;
class OTypeInt;
class OTypePointer;

enum EExprConvFlags
{
  EXPCF_GENERATE_ERRORS    = 1,
  EXPCF_ALLOW_LAZY_CSTRING = 2,
  EXPCF_EXPLICIT_CAST      = 4,
  EXPCF_ALLOW_ARRAY_LITERAL_SLICE = 8
};

class ODqCompAst : public ODqCompBase
{
private:
  using            super = ODqCompBase;

public:
  bool             section_public = true;
  OScope *         cur_mod_scope = nullptr;

public:
  ODqCompAst();
  virtual ~ODqCompAst();

  ODecl * AddDeclVar(OScPosition & scpos, string aid, OType * atype);
  ODecl * AddDeclConst(OScPosition & scpos, string aid, OType * atype, OValue * avalue);
  ODecl * AddDeclFunc(OScPosition & scpos, OValSymFunc * avsfunc);
  ODecl * AddDeclOverloadSet(OScPosition & scpos, OValSymOverloadSet * avsoverload);
  bool    ResolveCompoundMemberBase(OLValueExpr * lval, OType * srctype, OLValueExpr *& memberbase, OCompoundType *& ctype);
  void    CollectIgnoredPlainAssignVars(OLValueExpr * leftexpr, vector<OLValueVar *> & ignored);
  OValSym * GetAssignRootValSym(OLValueExpr * leftexpr);
  OExpr * FreeLeftRight(OExpr * left, OExpr * right);
  OExpr * CreateBinExpr(EBinOp op, OExpr * left, OExpr * right);
  bool    ConvertExprToType(OType * dsttype, OExpr ** rexpr, uint32_t aflags = 0);
  int     GetAssignTypeConversionCost(OType * dsttype, OExpr * expr, uint32_t aflags = 0);
  bool    ResolveIifType(OExpr ** rtrueexpr, OExpr ** rfalseexpr, OType ** rresulttype);
  bool    CheckAssignType(OType * dsttype, OExpr ** rexpr, const string astmt);
  bool    IsPointerWidthIntegerType(OType * type);
  bool    TryCalculateIntConstant(OExpr * expr, int64_t & rvalue);
  bool    FitsPointerWidthConstant(OTypeInt * srctype, int64_t value);
  bool    CanAssignPointerImplicitly(OTypePointer * dst, OTypePointer * src);


public: // Moved from ODqCompParser
  OStmtBlock *  curblock = nullptr;
  OScope *      curscope = nullptr;
  int           loop_depth = 0;
  int64_t       array_index_context_len = -1;
  OLValueExpr * array_index_context_lval = nullptr;
  bool          supress_varinit_check = false;  // do not emit unititalized variable errors (for left value expression parsing)
  struct TSuppressedVarInitDiag
  {
    OLValueVar *  varexpr = nullptr;
    OValSym *     valsym = nullptr;
    OScPosition   scpos;
  };
  struct TRawCallArg
  {
    OExpr *                        expr = nullptr;
    OScPosition                    scpos_start;
    vector<TSuppressedVarInitDiag> init_diags;
  };
  vector<TSuppressedVarInitDiag>  suppressed_varinit_diags;
  bool FinalizeStmtAssign(OLValueExpr * leftexpr, EBinOp op, OExpr * rightexpr);
  bool SupportsFuncParamDefaultType(OType * ptype);
  bool BindCallArguments(const string & callname, OTypeFunc * tfunc, vector<TRawCallArg> & rawargs, vector<OExpr *> & rargs);
  bool    SpecialFunctionSignatureIsValid(OValSymFunc * vsfunc);
  void    InjectObjectReceiver(OValSymFunc * vsfunc, OCompoundType * ctype);
  bool    ObjectMemberAccessAllowed(OCompoundType * decl_type, OValSym * member) const;
  OExpr * CreateImplicitObjectMemberExpr(const string & sid, OValSym * vs, OScope * found_scope);
  OValSymFunc * AddGeneratedObjectConstructor(OTypeObject * object_type, OValSymFunc * inherited_ctor,
                                              OScPosition & scpos, size_t overload_count);
  OValSymFunc * AddGeneratedObjectDestructor(OTypeObject * object_type, OValSymFunc * inherited_dtor,
                                             OScPosition & scpos);
  void    ValidateConstructorEmbeddedObjects(OValSymFunc * vsfunc);
  bool    CheckObjectCtorArgs(OTypeObject * object_type, vector<OExpr *> & rargs, OValSymFunc *& rctor);
  OValSymFunc * FindInheritedMethod(const string & method_name, const vector<OExpr *> & args);
  void    VarInitError(OLValueVar * varexpr, OValSym * valsym, OScPosition & scpos);
  void    AddSuppressedVarInitDiag(OLValueVar * varexpr, OValSym * valsym, OScPosition & scpos);
  void    EmitSuppressedVarInitDiags();
  void    EmitFilteredAssignVarInitDiags(OLValueExpr * leftexpr, EBinOp op);
  void FreeRawCallArguments(vector<TRawCallArg> & rawargs);
  void EmitStoredVarInitDiags(const vector<TSuppressedVarInitDiag> & diags);

protected:
  void    PrepareFuncDecl(OScPosition & scpos, OValSymFunc * avsfunc);
  bool    HarmonizeNumericOperands(OExpr ** rleft, OExpr ** rright);
  bool    ResolveCommonPointerType(OExpr * leftexpr, OExpr * rightexpr, OType ** rresulttype);
  bool    ResolveCommonFuncRefType(OExpr * leftexpr, OExpr * rightexpr, OType ** rresulttype);

};
