/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    symbols.h
 * authors: nvitya
 * created: 2026-02-01
 * brief:   Compiler Symbol Objects
 */

#pragma once

#include "stdint.h"

#include <format>
#include <string>
#include <vector>
#include <map>

#include "ll_defs.h"

#include "comp_config.h"
#include "attributes.h"

using namespace std;

class ODqCompBase;
class OType;
class OValue;
class OValSym;
class OScope;

// Symbol and Scope

class OSymbol
{
public:
  string       name;
  OType *      ptype;

  OScPosition  scpos;

  OSymbol(const string aname, OType * atype = nullptr)
  :
    name(aname),
    ptype(atype)
  {

  }

  OType * ResolvedType() const;

  virtual ~OSymbol() = default;
};

class OScope
{
public:
  OScope *    parent_scope;
  string      debugname; // Helpful for debugging (e.g., "Class Body", "Func Body")
  bool        vs_lookup_parent = true;

  map<string, OType *>    typesyms;
  map<string, OValSym *>  valsyms;

  vector<OValSym *>       firstassign; // list of the variables assigned here first

  LlDiScope * di_scope = nullptr;

public:
  OScope(OScope * aparent, const string & adebugname)
  :
    parent_scope(aparent),
    debugname(adebugname)
  {
  }

  OType *     DefineType(OType * atype);
  OValSym *   DefineValSym(OValSym * atype);

  OType *     FindType(const string & name, OScope ** rscope = nullptr, bool arecursive = true);
  OValSym *   FindValSym(const string & name, OScope ** rscope = nullptr, bool arecursive = true);

  LlDiScope *  GetDiScope();

  void        SetVarInitialized(OValSym * vs);
  void        RevertFirstAssignments();
  bool        FirstAssigned(OValSym * avs);
};

// Types

enum ETypeKind
{
  TK_VOID = 0,
  TK_INT,
  TK_FLOAT,
  TK_BOOL,
  TK_POINTER,
  TK_ARRAY,
  TK_ARRAY_SLICE,  // array descriptor {ptr, length} for function parameters
  TK_STRING,    // ODynString, OCString

  TK_ALIAS,
  TK_ENUM,
  TK_COMPOUND,  // object, struct
  TK_FUNCTION,
  TK_FUNCREF,   // function variable (function pointer)
};

class OExpr;

class OTypePointer;      // forward declaration
class OTypeFuncRef;     // forward declaration
class OTypeArray;        // forward declaration
class OTypeArraySlice;   // forward declaration

class OType : public OSymbol
{
private:
  using           super = OSymbol;

  OTypePointer *     ptr_type = nullptr;    // cached pointer-to-this type
  OTypeArraySlice *  slice_type = nullptr;  // cached slice type
  map<uint32_t, OTypeArray *>  array_types; // cached fixed-size array types

public:
  ETypeKind    kind;
  bool         incomplete = false;
  uint32_t     bytesize = 0;  // 0 = size is not fixed (string, dyn. array)

  LlType *     ll_type = nullptr;
  LlDiType *   di_type = nullptr;

  OType(const string aname, ETypeKind akind)
  :
    super(aname, nullptr),  // Types usually don't have a "type" themselves, or are meta-types
    kind(akind)
  {
  }

  virtual ~OType();

  virtual LlType *  CreateLlType() { return nullptr; }
  virtual LlType *  GetLlType()
  {
    if (!ll_type)
    {
      ll_type = CreateLlType();
    }
    return ll_type;
  }

  virtual LlDiType *  CreateDiType() { return nullptr; }
  virtual LlDiType *  GetDiType()
  {
    if (!di_type)
    {
      di_type = CreateDiType();
    }
    return di_type;
  }

  inline bool        IsCompound()   { return (kind == TK_COMPOUND);  }
  virtual OType *    ResolveAlias() { return this; }
  OTypePointer *     GetPointerType();
  OTypeArray *       GetArrayType(uint32_t alength);
  OTypeArraySlice *  GetSliceType();
  virtual OValSym *  CreateValSym(OScPosition & apos, const string aname);
  virtual OValue *   CreateValue()  { return nullptr; }
  virtual LlValue *  GenerateConversion(OScope * scope, OExpr * src)  { return nullptr; }
};

inline OType * OSymbol::ResolvedType() const
{
  return (ptype ? ptype->ResolveAlias() : nullptr);
}

class OTypeVoid : public OType
{
private:
  using        super = OType;

public:
  OTypeVoid()
  :
    super("void", TK_VOID)
  {
    bytesize = 1;
  }

  LlType * CreateLlType() override
  {
    return LlType::getVoidTy(ll_ctx);
  }

  LlDiType * GetDiType() override
  {
    return nullptr;
  }
};

class OTypeAlias : public OType
{
private:
  using        super = OType;

public:
  OType *      ptype;

  OTypeAlias(const string aname, OType * aptype)
  :
    super(aname, (aptype ? aptype->kind : TK_ALIAS)),
    ptype(aptype)
  {
    bytesize = (ptype ? ptype->bytesize : 0);
  }

  OType * ResolveAlias() override
  {
    return (ptype ? ptype->ResolveAlias() : this);
  }

  OValue * CreateValue() override
  {
    return (ptype ? ptype->CreateValue() : nullptr);
  }

  LlValue * GenerateConversion(OScope * scope, OExpr * src) override
  {
    return (ptype ? ptype->GenerateConversion(scope, src) : nullptr);
  }

  LlType * GetLlType() override
  {
    return ptype->GetLlType();
  }

  LlDiType * CreateDiType() override
  {
    return ptype->GetDiType();
  }
};


class OCompoundType : public OType
{
private:
  using        super = OType;

public:
  OScope       member_scope;
  vector<OValSym *>  member_order;  // declaration order for LLVM struct layout
  bool         is_object = false;

  OCompoundType(const string name, OScope * aparent_scope, bool ais_object = false)
  :
    super(name, TK_COMPOUND),
    member_scope(aparent_scope, name),
    is_object(ais_object)
  {
    if (is_object)
    {
      member_scope.vs_lookup_parent = false;
    }
  }

  inline OScope * Members() { return &member_scope; }

  void AddMember(OValSym * amember);
  int  FindMemberIndex(const string & aname);

  LlType *    CreateLlType() override;
  LlDiType *  CreateDiType() override;
};

class OTypePointer : public OType
{
private:
  using        super = OType;

public:
  OType *      basetype;
  bool         is_opaque = false;
  bool         is_null_literal = false;

  OTypePointer(OType * abasetype, const string & aname = "", bool aopaque = false, bool anull_literal = false)
  :
    super((aname.empty() ? "^" + (abasetype ? abasetype->name : "void") : aname), TK_POINTER),
    basetype(abasetype),
    is_opaque(aopaque),
    is_null_literal(anull_literal)
  {
    bytesize = TARGET_PTRSIZE;
  }

  bool IsTypedPointer() const
  {
    return (basetype != nullptr) && !is_opaque && !is_null_literal;
  }

  bool IsOpaquePointer() const
  {
    return is_opaque;
  }

  bool IsNullPointer() const
  {
    return is_null_literal;
  }

  LlType * CreateLlType() override
  {
    return llvm::PointerType::get(ll_ctx, 0);
  }

  LlDiType * CreateDiType() override
  {
    return di_builder->createPointerType(
        basetype ? basetype->GetDiType() : nullptr,
        bytesize * 8
    );
  }

  OValue * CreateValue() override;
  LlValue * GenerateConversion(OScope * scope, OExpr * src) override;

  static OTypePointer * GetNullPtrType()
  {
    static OTypePointer instance(nullptr, "null", false, true);
    return &instance;
  }
};

// Values

class OExpr;

class OValue
{
public:
  OType *       ptype = nullptr;
  LlConst *     ll_const = nullptr;

  virtual ~OValue() {};
  OValue(OType * atype)
  :
    ptype(atype)
  {
  }

  virtual LlConst * CreateLlConst() { return nullptr; }

  virtual LlConst * GetLlConst()
  {
    if (!ll_const)
    {
      ll_const = CreateLlConst();
    }
    return ll_const;
  }

  virtual bool CalculateConstant(OExpr * expr, bool emit_errors = true) { return false; }

  inline OType * ResolvedType() const
  {
    return (ptype ? ptype->ResolveAlias() : nullptr);
  }
};

class OValuePointer : public OValue
{
private:
  using        super = OValue;

public:
  bool         is_null = true;

  OValuePointer(OType * atype, bool ais_null)
  :
    super(atype),
    is_null(ais_null)
  {
  }

  LlConst *  CreateLlConst() override;
  bool       CalculateConstant(OExpr * expr, bool emit_errors = true) override;
};

// Expression Base

class OExpr
{
public:
  OType *  ptype; // result type (of this node), defaults to int

  OExpr();

  virtual ~OExpr() {};

  static void DeleteTree(OExpr * expr);
  static void FoldTree(OExpr ** rexpr);

  virtual LlValue * Generate(OScope * scope)
  {
    throw logic_error(std::format("Unhandled OExpr::Generate for \"{}\"", typeid(this).name()));
  }

  virtual void FoldChildren()
  {
  }

  virtual bool TryFoldSelf(OExpr ** rreplacement)
  {
    (void)rreplacement;
    return false;
  }

  virtual void DeleteChildTree()
  {
  }

  inline OType * ResolvedType() const
  {
    return (ptype ? ptype->ResolveAlias() : nullptr);
  }
};

// Value Symbols

enum EValSymKind
{
  VSK_CONST = 0,
  VSK_VARIABLE,
  VSK_PARAMETER,
  VSK_FUNCTION
};

enum EParamMode
{
  FPM_VALUE = 0,
  FPM_REF,
  FPM_REFIN,
  FPM_REFOUT,
  FPM_REFNULL
};

inline bool ParamModeIsRefLike(EParamMode amode)
{
  return (FPM_VALUE != amode);
}

class OValSym : public OSymbol
{
private:
  using        super = OSymbol;

public:
  EValSymKind  kind;
  bool         initialized = false;  // reading of uninitialized results to an error
  LlValue *    ll_value = nullptr;
  EParamMode   param_mode = FPM_VALUE;
  bool         is_ref_alias = false;
  bool         ref_nullable = false;

  uint32_t     attr_align = 0;
  string       attr_section_name = "";
  bool         attr_is_overload = false;
  bool         attr_is_override = false;
  bool         attr_is_virtual = false;
  bool         attr_is_volatile = false;

  OValSym(OScPosition & apos, const string aname, OType * atype, EValSymKind akind = VSK_VARIABLE)
  :
    super(aname, atype),  // Types usually don't have a "type" themselves, or are meta-types
    kind(akind)
  {
    scpos = apos;
  }

  virtual void ApplyAttributes(OAttr * attr, EAttrTarget atarget);
  virtual void GenGlobalDecl(bool apublic, OValue * ainitval = nullptr);

  inline bool IsRefLike() const
  {
    return (is_ref_alias || ParamModeIsRefLike(param_mode));
  }

  inline bool IsRefNullable() const
  {
    return (IsRefLike() && (ref_nullable || (FPM_REFNULL == param_mode)));
  }

  inline bool IsRefWriteable() const
  {
    return (!IsRefLike() || (FPM_REFIN != param_mode));
  }

  inline OType * GetStorageType() const
  {
    return (IsRefLike() ? ptype->GetPointerType() : ptype);
  }
};

class OValSymConst : public OValSym
{
private:
  using        super = OValSym;

public:
  OValue *     pvalue;

  OValSymConst(OScPosition & apos, const string aname, OType * atype, OValue * avalue = nullptr)
  :
    super(apos, aname, atype, VSK_CONST)
  {
    initialized = true;
    if (avalue)
    {
      pvalue = avalue;
    }
    else
    {
      pvalue = atype->CreateValue();
    }
  }

  ~OValSymConst()
  {
    delete pvalue;
  }
};
