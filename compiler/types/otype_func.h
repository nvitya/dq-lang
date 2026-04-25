/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_function.h
 * authors: nvitya
 * created: 2026-02-07
 * brief:   function type
 */

#pragma once

#include <vector>
#include "symbols.h"
#include "expressions.h"
#include "statements.h"

using namespace std;

class OValSymFunc;
class OModule;

enum EOverloadFuncRefMatch
{
  OFRM_NOT_OVERLOAD = 0,
  OFRM_NO_MATCH,
  OFRM_UNIQUE_MATCH,
  OFRM_AMBIGUOUS
};

struct TFuncCallArgMatch
{
  OExpr *  expr = nullptr;
  bool     has_init_diags = false;
};

struct TFuncCallMatchScore
{
  int   conversions = 0;
  int   defaults = 0;
  bool  uses_varargs = false;
};

class OFuncParam
{
public:
  string              name;
  OType *             ptype;
  EParamMode          mode;
  OValSymConst *      defvalue = nullptr;

  OFuncParam(const string aname, OType * atype, EParamMode amode = FPM_VALUE)
  :
    name(aname),
    ptype(atype),
    mode(amode)
  {
  }

  virtual ~ OFuncParam()
  {
    delete defvalue;
  }

  inline bool IsRefLike() const
  {
    return ParamModeIsRefLike(mode);
  }

  inline OType * GetLlArgType() const
  {
    return (IsRefLike() ? ptype->GetPointerType() : ptype);
  }
};

// A function signature is a type (TK_FUNCTION).
// It describes the parameter types, return type, calling convention, etc.
// This is what you need when you declare type Callback = function(x: int) -> int;
// or check assignment compatibility between function pointers.

class OTypeFunc : public OType
{
private:
  using        super = OType;

public:
  OType *               rettype;
  vector<OFuncParam *>  params;
  bool                  has_varargs = false;

  OTypeFunc(const string aname, OType * arettype = nullptr)
  :
    super(aname, TK_FUNCTION),
    rettype(arettype)
  {
  }

  ~OTypeFunc()
  {
    for (OFuncParam * fp : params)
    {
      delete fp;
    }
  }

  OFuncParam *  AddParam(const string aname, OType * atype, EParamMode amode = FPM_VALUE);
  bool          ParNameValid(const string aname);
  size_t        RequiredParamCount() const;
  OType *       ResolvedRetType() const;
  bool          MatchesOverloadDeclIdentity(const OTypeFunc * other) const;
  bool          MatchesSignature(const OTypeFunc * other) const;
  void          MergeForwardDeclFrom(OTypeFunc * other, bool copy_param_names);
  bool          AnalyzeCallCandidate(const vector<TFuncCallArgMatch> & callargs,
                                     TFuncCallMatchScore & rscore) const;

  static bool   SameRefBindingType(OType * dsttype, OType * srctype);
  static int    CompareCallCandidateScore(const TFuncCallMatchScore & left,
                                          const TFuncCallMatchScore & right);

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
};


// A function declaration (a named function in a module or object) is a value symbol (SK_FUNCTION)
// It isa named entity in a scope that can be called, referenced, assigned to a function pointer, etc.
// Its .type member points to a function signature type of OTypeFunc.

class OValSymFunc : public OValSym
{
private:
  using              super = OValSym;

public:
  vector<OValSym *>  args;  // will be filled in GenerateFuncBody()
  OValSym *          vsresult = nullptr;
  OCompoundType *    owner_compound_type = nullptr;
  OValSym *          receiver_arg = nullptr;
  OScPosition        scpos_endfunc;
  OStmtBlock *       body;

  bool               has_body = false;
  bool               is_external = false;
  string             external_linkage_name = "";
  string             generated_linkage_name = "";

  LlFunction *       ll_func = nullptr;
  LlDiSubPrg *       di_func = nullptr;

  OValSymFunc(OScPosition & apos, const string aname, OTypeFunc * atype, OScope * aparentscope = nullptr)
  :
    super(apos, aname, atype, VSK_FUNCTION)
  {
    body  = new OStmtBlock(aparentscope, "function_"+aname);
  }

  virtual ~OValSymFunc()
  {
    for (OValSym * a : args)
    {
      delete a;
    }
    if (vsresult)  delete vsresult;
    delete body;
    delete ptype;  // OTypeFunc owned by this function symbol
  }

  inline bool IsForwardDecl() const
  {
    return (!is_external && !has_body);
  }

  void ApplyAttributes(OAttr * attr, EAttrTarget atarget) override;
  void GenGlobalDecl(bool apublic, OValue * ainitval = nullptr) override;

  bool CheckForwardDeclMatch(OValSymFunc * other) const;
  void MergeForwardDeclFrom(OValSymFunc * other, bool copy_param_names);
  void ValidateForwardDecl() const;
  void ResetBodyScope(OScope * aparentscope);
  void GenerateFuncBody();
  void GenerateFuncRet();
};

class OValSymOverloadSet : public OValSym
{
private:
  using  super = OValSym;

public:
  vector<OValSymFunc *>  funcs;
  OCompoundType *        owner_compound_type = nullptr;
  string                 generated_linkage_prefix = "";

  OValSymOverloadSet(OScPosition & apos, const string aname, OType * atype = nullptr)
  :
    super(apos, aname, atype, VSK_FUNCTION)
  {
  }

  virtual ~OValSymOverloadSet()
  {
    for (OValSymFunc * fn : funcs)
    {
      delete fn;
    }
  }

  void AddFunc(OValSymFunc * afunc);
  OType * ResolvedRetType() const;
  bool HasMatchingReturnType(const OTypeFunc * atype) const;
  bool HasMatchingOverloadDecl(const OTypeFunc * atype) const;
  bool HasMatchingSignature(const OTypeFunc * atype) const;
  OValSymFunc * FindMatchingOverloadDecl(const OTypeFunc * atype) const;
  EOverloadFuncRefMatch FindMatchingSignature(const OTypeFunc * atype, OValSymFunc *& rfunc) const;
  void ValidateForwardDecls() const;

  inline size_t Count() const
  {
    return funcs.size();
  }
};

class OValueFuncRef : public OValue
{
public:
  bool          is_null = true;
  OValSymFunc * target_func = nullptr;

  OValueFuncRef(OType * atype, bool ais_null = true)
  :
    OValue(atype),
    is_null(ais_null)
  {
  }

  LlConst *  CreateLlConst() override;
  bool       CalculateConstant(OExpr * expr, bool emit_errors = true) override;
};

class OTypeFuncRef : public OType
{
private:
  using        super = OType;

public:
  OTypeFunc *  functype = nullptr;

  OTypeFuncRef(OTypeFunc * afunctype, const string & aname = "");
  ~OTypeFuncRef() override;

  LlType *   CreateLlType() override;
  LlDiType * CreateDiType() override;
  OValue *   CreateValue() override;
  LlValue *  GenerateConversion(OScope * scope, OExpr * src) override;
  bool       CanAccept(OType * srctype) const;
  EOverloadFuncRefMatch FindAcceptingOverload(OExpr * src, OValSymFunc *& rfunc) const;
};

extern string FuncTypeName(OTypeFunc * sigtype);
void ValidateModuleForwardFuncDecls(OModule * module);
