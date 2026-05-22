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
class ODqmIfWriter;
class OModuleBase;
class OModuleUse;

// Symbol and Scope

class OSymbol
{
public:
  string         name;
  OType *        ptype;

  OModuleBase *  module = nullptr;
  OScPosition    scpos;

  OSymbol(const string aname, OType * atype = nullptr, OModuleBase * amodule = nullptr)
  :
    name(aname),
    ptype(atype),
    module(amodule)
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
  vector<OValSym *>       fixed_object_vars;

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

enum EIntfDeclKind
{
  IDK_TYPE,
  IDK_VALSYM
};

enum EModuleUseMergeMode
{
  MUM_ALL,
  MUM_NONE,
  MUM_ONLY,
  MUM_EXCLUDE
};

class OIntfDecl
{
public:
  EIntfDeclKind  kind;

  union
  {
    OType *      ptype;
    OValSym *    pvalsym;
  };

  OIntfDecl(OType * atype)
  :
    kind(IDK_TYPE),
    ptype(atype)
  {
  }

  OIntfDecl(OValSym * avalsym)
  :
    kind(IDK_VALSYM),
    pvalsym(avalsym)
  {
  }
};

class OModuleUse
{
public:
  OModuleBase *        module = nullptr;
  OScope *             scope_use = nullptr;
  bool                 is_private = false;
  bool                 reexport = false;
  EModuleUseMergeMode  merge_mode = MUM_ALL;
  vector<string>       symbol_names;

  OModuleUse(OModuleBase * amodule, bool ais_private, EModuleUseMergeMode amerge_mode,
             const vector<string> & asymbol_names, bool areexport = false)
  :
    module(amodule),
    is_private(ais_private),
    reexport(areexport),
    merge_mode(amerge_mode),
    symbol_names(asymbol_names)
  {
  }

  ~OModuleUse()
  {
    delete scope_use;
  }

  bool SymbolSelected(const string & aname) const;
  bool ValidateSymbolNames() const;
  void CopySelectedSymbolsTo(OScope * adst) const;
  void FillScope() const;
  vector<string> EffectiveSymbolNames() const;
};

class OModuleBase
{
public:
  string           name;
  OScope *         scope_pub;

  vector<OIntfDecl *>  declarations;  // interface declarations in source code order
  vector<OModuleUse *>  used_modules;  // semantic use records

  OModuleBase(OScope * aparent, const string aname)
  :
    name(aname)
  {
    scope_pub = new OScope(aparent, aname);
  }

  virtual ~OModuleBase()
  {
    for (OIntfDecl * decl : declarations)
    {
      delete decl;
    }
    for (OModuleUse * use : used_modules)
    {
      delete use;
    }
    delete scope_pub;
  }
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
class OValSymFunc;

enum EObjectStorageKind
{
  OSK_PLAIN = 0,
  OSK_OBJECT_REF,
  OSK_OBJECT_FIXED
};

enum EObjectSpecFuncKind
{
  OSF_NONE = 0,
  OSF_CREATE,
  OSF_DESTROY
};

enum EMemberVisibility
{
  MV_PRIVATE = 0,
  MV_PROTECTED,
  MV_PUBLIC
};

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
  uint32_t     alignsize = 1;

  LlType *     ll_type = nullptr;
  LlDiType *   di_type = nullptr;

  OType(const string aname, ETypeKind akind)
  :
    super(aname, nullptr),  // Types usually don't have a "type" themselves, or are meta-types
    kind(akind)
  {
  }

  virtual ~OType();

  virtual void EnsureLayout() {}
  virtual LlType *  CreateLlType() { return nullptr; }
  virtual LlType *  GetLlType()
  {
    EnsureLayout();
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
  virtual bool       WriteDqmIfTypeSpec(ODqmIfWriter & writer);
  virtual bool       WriteDqmIfDecl(ODqmIfWriter & writer);
};

uint32_t EffectiveStorageAlign(OType * atype, uint32_t aattr_align = 0);

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
    alignsize = 1;
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
    alignsize = (ptype ? ptype->alignsize : 1);
  }

  void EnsureLayout() override
  {
    if (ptype)
    {
      ptype->EnsureLayout();
      bytesize = ptype->bytesize;
      alignsize = ptype->alignsize;
    }
  }

  OType * ResolveAlias() override
  {
    return (ptype ? ptype->ResolveAlias() : this);
  }

  OValSym * CreateValSym(OScPosition & apos, const string aname) override;

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

  bool WriteDqmIfDecl(ODqmIfWriter & writer) override;
};


class OCompoundType : public OType
{
private:
  using        super = OType;

public:
  OScope       member_scope;
  vector<OValSym *>  member_order;  // declaration order for LLVM struct layout
  bool         is_object = false;
  bool         is_packed = false;
  bool         layout_ready = false;
  bool         layout_busy = false;
  bool         manual_ll_layout = false;
  OCompoundType * base_type = nullptr;
  bool         has_own_vtable = false;
  bool         is_polymorphic = false;
  bool         is_abstract = false;
  uint32_t     vtable_field_index = 0;
  LlValue *    ll_vtable = nullptr;
  vector<OValSymFunc *> virtual_methods;
  vector<OValSymFunc *> constructors;
  OValSymFunc * destructor = nullptr;

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

  OValSym * CreateValSym(OScPosition & apos, const string aname) override;
  void AddMember(OValSym * amember);
  int  FindMemberIndex(const string & aname);
  OValSymFunc * FindSpecialMethod(EObjectSpecFuncKind akind, size_t auser_arg_count = size_t(-1)) const;
  bool IsSameOrDerivedFrom(OCompoundType * abase) const;
  OValSym * FindObjectMemberSymbol(const string & aname, OCompoundType ** rdecl_type = nullptr) const;
  int FindObjectFieldIndex(const string & aname, OCompoundType ** rdecl_type = nullptr) const;
  OValSymFunc * FindVirtualBaseMethod(OValSymFunc * afunc, OCompoundType ** rdecl_type = nullptr) const;
  int FindVirtualSlot(OValSymFunc * afunc) const;
  void UpdateObjectInheritanceFlags();
  void GenVTableGlobal(bool apublic);
  void GenerateVTableStore(LlValue * ll_object_addr);

  void        EnsureLayout() override;
  LlType *    CreateLlType() override;
  LlDiType *  CreateDiType() override;
  bool        WriteDqmIfDecl(ODqmIfWriter & writer) override;
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
    alignsize = TARGET_PTRSIZE;
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
  virtual bool WriteDqmIfValue(ODqmIfWriter & writer);

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
  bool       WriteDqmIfValue(ODqmIfWriter & writer) override;
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
  static OExpr * FoldScalarExpr(OExpr * expr);
  static bool TryFoldScalarReplacement(OExpr * expr, OExpr ** rreplacement);

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
  uint32_t     field_offset = 0;
  uint32_t     ll_field_index = 0;

  uint32_t     attr_align = 0;
  string       attr_section_name = "";
  bool         attr_has_linkage_name = false;
  string       attr_linkage_name = "";
  string       owner_module_name = "";
  bool         attr_is_overload = false;
  bool         attr_is_override = false;
  bool         attr_is_virtual = false;
  bool         attr_is_abstract = false;
  bool         attr_is_final = false;
  bool         attr_is_volatile = false;
  OExpr *      field_init_expr = nullptr;
  EMemberVisibility member_visibility = MV_PUBLIC;

  OValSym(OScPosition & apos, const string aname, OType * atype, EValSymKind akind = VSK_VARIABLE)
  :
    super(aname, atype),  // Types usually don't have a "type" themselves, or are meta-types
    kind(akind)
  {
    scpos = apos;
  }

  virtual ~OValSym();
  virtual void ApplyAttributes(OAttr * attr, EAttrTarget atarget);
  virtual void GenGlobalDecl(bool apublic, OValue * ainitval = nullptr);
  void GenGlobalImportDecl();
  string GetLinkageName(bool apublic, char atype_prefix, const string & asymbol_name = "") const;
  virtual bool WriteDqmIfAttributes(ODqmIfWriter & writer, uint64_t aextra_flags = 0);
  virtual bool WriteDqmIfDecl(ODqmIfWriter & writer);

  inline bool IsRefLike() const
  {
    return (is_ref_alias || ParamModeIsRefLike(param_mode));
  }

  bool IsObjectType() const;

  inline bool IsRefNullable() const
  {
    return (IsRefLike() && (ref_nullable || (FPM_REFNULL == param_mode)));
  }

  inline bool IsRefWriteable() const
  {
    return (!IsRefLike() || (FPM_REFIN != param_mode));
  }

  virtual OType * GetStorageType() const;
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

  bool WriteDqmIfDecl(ODqmIfWriter & writer) override;
};
